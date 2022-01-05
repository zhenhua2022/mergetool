
#include <stdio.h>  
#include <string.h>
#include <stdlib.h>

#include <math.h>
#include <unistd.h>
#include <getopt.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define VERSION_STRING          "1.00.2"

#define	GETC_SIZE			(1024)		//fread 's buff
#define	MAX_SIZE			(32)		//storage config pamars size
#define	BUF_SIZE			(1024)		//copy buff size
#define	MAX_PARTITIONNUM	(32)		//partition file 's length
#define	BYTE_NUMER_OF_LINE	(16)		//in partition file ,each line 's number
#define	BLANKCONFIG			(4)			//Ignore blank lines

#define	OFFSET_RESET		(0)
#define	FILL_NUMBER			(0xff)		//megre image's fill number

#define	CH_SUCCESS			(0)
#define	CH_FAILURE			(-1)

#define	CH_TRUE 			(1)
#define	CH_FALSE 			(0)

typedef	unsigned int		BOOL;		/*布尔型无符号数*/
typedef	unsigned char		U8;			/*8位无符号数*/
typedef	char				S8;			/*8位有符号数*/
typedef	unsigned short		U16;		/*16位无符号数*/
typedef	short				S16;		/*16位有符号数*/
typedef	unsigned int		U32;		/*32位无符号数*/
typedef	int					S32;		/*32位有符号数*/

typedef struct NAND_FLASH_PARAM_S
{
	S32	i_pagesize;
	S32	i_oobsize;
	S32	i_blocksize;
	
}NAND_FLASH_PARAM;

typedef struct CONFIG_PARAM_S
{
	U8 uc_SelectFile[MAX_SIZE];
	U8 uc_StartAddr[MAX_SIZE];
	U8 uc_Length[MAX_SIZE];
	
}CONFIG_PARAM;

S32 CH_TOOL_GetFileLine(S8* rc_ConfigFileName, S32* rpi_FileLine)
{	
	FILE*	pf_ConfigFile = NULL;
	S32		i_FileLine = 0;		
	S8		c_Buf[BUF_SIZE] = {0};	


	pf_ConfigFile = fopen(rc_ConfigFileName, "rt");
	if(NULL == pf_ConfigFile)
	{
		printf("[%s]:%d Fail to open file!\n", __FUNCTION__, __LINE__);
		return CH_FAILURE;
	}

	while(NULL != fgets(c_Buf ,GETC_SIZE ,pf_ConfigFile))
	{
		if(BLANKCONFIG <= strlen(c_Buf) && c_Buf[0] != '#')
		{
			i_FileLine = i_FileLine + 1;
		}
	}
	
	*rpi_FileLine = i_FileLine;
	
	printf("config line : %d\n",i_FileLine);

	fclose(pf_ConfigFile);

	return CH_SUCCESS;
}

S32 CH_TOOL_GetConfig(S8* rc_ConfigFileName ,S32 ri_FileLine ,CONFIG_PARAM** rppstru_ConfigParam)
{
	FILE*	pf_ConfigFile = NULL;
	S32		i_FileLine = ri_FileLine;
	
	CONFIG_PARAM*	pstru_Param = NULL;
	S32		i_CurFileLine = 0;	
	S8		c_LineStr[GETC_SIZE+1] = {0};
	S8		tmpBuf[128] = {0};
	S32		i_Result = 0;

	pf_ConfigFile = fopen(rc_ConfigFileName, "rt");
	if(NULL == pf_ConfigFile)
	{
		printf("[%s]:%d Fail to open file!\n", __FUNCTION__, __LINE__);
		return CH_FAILURE;
	}

	pstru_Param = (CONFIG_PARAM*)malloc(i_FileLine	 * sizeof(CONFIG_PARAM));
	if (NULL == pstru_Param)
	{
		printf("[%s]:%d get memory false\n", __FUNCTION__ ,__LINE__);
		goto ERR1;
	}

	memset(pstru_Param, 0, sizeof(NAND_FLASH_PARAM) * i_FileLine);

	while(NULL != fgets(c_LineStr, GETC_SIZE, pf_ConfigFile))
	{
		if(BLANKCONFIG <= strlen(c_LineStr) && c_LineStr[0] != '#')
		{
			sscanf(c_LineStr, "%[^','], %[^','], %[^','],%s", 
                pstru_Param[i_CurFileLine].uc_SelectFile, pstru_Param[i_CurFileLine].uc_StartAddr, pstru_Param[i_CurFileLine].uc_Length, tmpBuf);           
			
			memset(c_LineStr ,0 ,sizeof(c_LineStr));
		
			i_CurFileLine++;
		}
	}
	
	for(i_CurFileLine = 0 ;i_CurFileLine < i_FileLine ;i_CurFileLine++)
	{
		printf("line[%d]\t [%s]\t [%s]\t [%s] \n", i_CurFileLine, 
				pstru_Param[i_CurFileLine].uc_SelectFile, pstru_Param[i_CurFileLine].uc_StartAddr, pstru_Param[i_CurFileLine].uc_Length);
	}
	
	*rppstru_ConfigParam = pstru_Param;
	
	fclose(pf_ConfigFile);

	return CH_SUCCESS;

ERR2:
	free(pstru_Param);
	
ERR1:
	fclose(pf_ConfigFile);
		
	CH_FAILURE;
}

S32 CH_TOOL_CheckPartitionLength(CONFIG_PARAM* rpstru_ConfigParam ,S32 ri_FileLine)
{
	CONFIG_PARAM* pstru_ConfigParam = rpstru_ConfigParam;
	S32		i_FileLine = ri_FileLine;

	S32 	i_CurFileLine = 1;
	S32		i_StartAddr = 0;
	S32		i_LastStartAddr = 0;
	S32		i_LastPartitionLength = 0;

	for(i_CurFileLine = 1 ;i_CurFileLine < i_FileLine ;i_CurFileLine++)	
	{
		i_StartAddr = strtol(pstru_ConfigParam[i_CurFileLine].uc_StartAddr, NULL, 16);
		i_LastStartAddr = strtol(pstru_ConfigParam[i_CurFileLine - 1].uc_StartAddr, NULL, 16);
		i_LastPartitionLength = strtol(pstru_ConfigParam[i_CurFileLine - 1].uc_Length, NULL, 16);
		
		if(i_StartAddr < (i_LastStartAddr + i_LastPartitionLength))
		{
			printf("please check config params, line[%d] partition length error. (*Third param is partition length) \n", (i_CurFileLine - 1));
			return CH_FAILURE;
		}
	}

	return CH_SUCCESS;
}

S32 CH_TOOL_CheckFileWhetherExist(S32 ri_FileLine ,CONFIG_PARAM* rpstru_ConfigParam ,S8* rpc_FilePath)
{
	CONFIG_PARAM*	pstru_ConfigParam = rpstru_ConfigParam;
	S32		i_FileLine = ri_FileLine;		
	
	S8* 	pc_FilePath = NULL;
	S8* 	pc_ConfigFileName = NULL;
	S8*		pc_InputFileName = NULL;
	
	S32		i = 0;
	BOOL	i_WhetherExist = 1;	// 1 is exist   0 is not exist
	S32		i_CurFileLine = 0;
		
	for(i_CurFileLine = 0; i_CurFileLine < i_FileLine; i_CurFileLine++)
	{
		pc_ConfigFileName = pstru_ConfigParam[i_CurFileLine].uc_SelectFile;

		pc_FilePath = (char *) malloc(sizeof(pc_FilePath) + 1);
		if(NULL == pc_FilePath)
		{
			printf("[%s]:%d get memory false\n", __FUNCTION__ ,__LINE__);
			return CH_FAILURE;
		}	

		strcpy(pc_FilePath ,rpc_FilePath);

		pc_InputFileName = (char *) malloc(strlen(pc_FilePath) + strlen(pc_ConfigFileName) +1);
		if(NULL == pc_InputFileName)
		{
			printf("[%s]:%d get memory false\n", __FUNCTION__ ,__LINE__);
			free(pc_FilePath);
			return CH_FAILURE;
		}

		memset(pc_InputFileName ,0 ,sizeof(pc_InputFileName));

		sprintf(pc_InputFileName ,"%s%s",pc_FilePath ,pc_ConfigFileName);

		if(CH_FAILURE == access(pc_InputFileName, F_OK))
		{

			printf("line[%d] [%s] file not exist.\r\n", i_CurFileLine, pstru_ConfigParam[i_CurFileLine].uc_SelectFile);
			i_WhetherExist = CH_FALSE;
		}

		free(pc_FilePath);
		free(pc_InputFileName);
	}

	if(CH_FALSE == i_WhetherExist)
	{
		return CH_FAILURE;
	}

	return CH_SUCCESS;
}

S32 CH_TOOL_CheckFile(S32 ri_FileLine ,CONFIG_PARAM* rpstru_ConfigParam ,S8* rpc_FilePath)
{
	CONFIG_PARAM*	pstru_ConfigParam = rpstru_ConfigParam;
	S32		i_FileLine = ri_FileLine;
	S8* 	pc_FilePath = rpc_FilePath;

	S32		i_Result = 0;

	printf("start check file.\n");
	
	/*** check partition length whether false ***/
	i_Result = CH_TOOL_CheckPartitionLength(pstru_ConfigParam ,i_FileLine);
	if(CH_FAILURE == i_Result)
	{		
		printf("end check file.\n");
		return CH_FAILURE;
	}

	/*** judge image file whether exist ***/
	i_Result = CH_TOOL_CheckFileWhetherExist(i_FileLine ,pstru_ConfigParam ,pc_FilePath);
	if(CH_FAILURE == i_Result)
	{
		printf("end check file.\n");
		return CH_FAILURE;
	}
	
	printf("end check file.\n");

	return CH_SUCCESS;
}

S32 CH_TOOL_FillBlankPartition(S32 ri_BlankLength ,NAND_FLASH_PARAM rstru_FlashParam ,S8* rpc_MegreFileName)
{
	S8* 	pc_MegreFileName = rpc_MegreFileName;
	S32		i_FillLength = ri_BlankLength;
	S32		i_pagesize = rstru_FlashParam.i_pagesize;
	S32		i_oobsize = rstru_FlashParam.i_oobsize;
	
	FILE*	pf_Merge = NULL;
	S8*		pc_MergeName = NULL;
	S8*		uc_FillNumber = NULL;
	S32		i = 0;
	
	pc_MergeName = pc_MegreFileName;
	if(NULL == pc_MergeName || NULL == (pf_Merge = fopen(pc_MergeName ,"ab+")))
	{
		printf("[%s]:%d open file failed\n", __FUNCTION__ ,__LINE__);
		return CH_FAILURE;
	}

	i_FillLength = i_FillLength / i_pagesize * (i_pagesize + i_oobsize);

	uc_FillNumber = (char *)malloc(i_FillLength);
	if(NULL == uc_FillNumber)
	{
		printf("[%s]:%d get memory false\n", __FUNCTION__ ,__LINE__);
		fclose(pf_Merge);
		return CH_FAILURE;
	}

	memset(uc_FillNumber, FILL_NUMBER, i_FillLength);

	fwrite(uc_FillNumber, i_FillLength, 1, pf_Merge);
	
	fclose(pf_Merge);
	free(uc_FillNumber);

	return CH_SUCCESS;
}

S32 CH_TOOL_OutputPratitionTable(CONFIG_PARAM rstru_ConfigParam, NAND_FLASH_PARAM rstru_FlashParam,S32 ri_FileLine,
												BOOL ri_WhetherLastLine,S8* rpc_PratitionFileName, S8* rpc_FilePath)
{
	CONFIG_PARAM		stru_ConfigParam = rstru_ConfigParam;
	NAND_FLASH_PARAM	stru_FlashParam = rstru_FlashParam;
	S32		i_FileLine = ri_FileLine;
	BOOL	i_WhetherLastLine = ri_WhetherLastLine;
	S8*		pc_PratitionFileName = rpc_PratitionFileName;
	S32		i_PageSize = stru_FlashParam.i_pagesize;
	S32 	i_OobSize = stru_FlashParam.i_oobsize;
	S32 	i_BlockSize = stru_FlashParam.i_blocksize;
	
	FILE*	pf_PartitionTable = NULL;
	FILE*	pf_Config = NULL;
	S8*		pc_ConfigFileName = NULL;
	S8*		pc_InputFileName = NULL;
	S8*		pc_FilePath = NULL;
	
	S32		i_StartAddr = 0;
	S32		i_PartitionLength = 0;
	S32		i_StartBlock = 0;
	S32		i_PartitionBlock = 0;
	S32		i_EndBlock = 0;
    
	S32		i = 0;
	S32		i_BlockNumber = 0;
	U32 	ui_FileSize = 0;	
	U8 		uc_FillNumber = FILL_NUMBER;
	
	/*** read file ,link path ***/	
	pc_ConfigFileName = stru_ConfigParam.uc_SelectFile;

	pc_FilePath = (char *) malloc(sizeof(pc_FilePath) + 1);
	if(NULL == pc_FilePath)
	{
		printf("[%s]:%d get memory false\n", __FUNCTION__ ,__LINE__);
		return CH_FAILURE;
	}	
	
	strcpy(pc_FilePath ,rpc_FilePath);
	
	pc_InputFileName = (char *) malloc(strlen(pc_FilePath) + strlen(pc_ConfigFileName) + 1);
	if(NULL == pc_InputFileName)
	{
		printf("[%s]:%d get memory false\n", __FUNCTION__ ,__LINE__);
		goto ERR1;
	}
	
	memset(pc_InputFileName ,0 ,sizeof(pc_InputFileName));
	
	sprintf(pc_InputFileName ,"%s%s",pc_FilePath ,pc_ConfigFileName);

	/*** open file burn file ***/
	if(NULL == pc_InputFileName || NULL == (pf_Config = fopen(pc_InputFileName ,"rb")))
	{
		printf("[%s]:%d open input file : [%s] failed\n", __FUNCTION__ ,__LINE__ ,pc_InputFileName);
		goto ERR2;
	}

	/*** count file size ***/
	fseek(pf_Config ,OFFSET_RESET ,SEEK_END);
	ui_FileSize = ftell(pf_Config);
		
	/*** creat or open file : "partition_table" ***/
	if(NULL == pc_PratitionFileName || NULL == (pf_PartitionTable = fopen(pc_PratitionFileName ,"ab+")))
	{
		printf("[%s]:%d open pratition table file : [%s] failed\n", __FUNCTION__ ,__LINE__ ,pc_PratitionFileName);
		goto ERR3;
	}

	/*** output data to file "partition_table" ***/	
	i_StartAddr = strtol(stru_ConfigParam.uc_StartAddr, NULL, 16);
	i_PartitionLength = strtol(stru_ConfigParam.uc_Length, NULL, 16);

    i_StartBlock = i_StartAddr / (i_PageSize * i_BlockSize);
    i_PartitionBlock = i_PartitionLength / (i_PageSize * i_BlockSize);
	i_EndBlock = i_StartBlock + i_PartitionBlock - 1;
	
	i_BlockNumber = ((ui_FileSize -1) / ((i_PageSize + i_OobSize) * i_BlockSize)) + 1;

	fwrite(&i_StartBlock ,1 ,sizeof(i_StartBlock) ,pf_PartitionTable);
	fwrite(&i_EndBlock ,1 ,sizeof(i_EndBlock) ,pf_PartitionTable);	
	fwrite(&i_BlockNumber ,1 ,sizeof(i_BlockNumber) ,pf_PartitionTable);
	
	for(i=0 ;i < 4 ;i++ )
	{
		fwrite(&uc_FillNumber ,1 ,sizeof(uc_FillNumber) ,pf_PartitionTable);
	}

	/*** use "fill_number" , fill "Data_Merge" to need size ***/
	if(CH_TRUE == i_WhetherLastLine) /* if the line is the last line */
	{
		for(i=0 ;i < BYTE_NUMER_OF_LINE * (MAX_PARTITIONNUM - i_FileLine) ;i++)
		{
	    	fwrite(&uc_FillNumber ,1 ,sizeof(uc_FillNumber) ,pf_PartitionTable);
		}
	}

	free(pc_FilePath);
	free(pc_InputFileName);
	fclose(pf_Config);
	fclose(pf_PartitionTable);
	
	return CH_SUCCESS;

ERR3:
	fclose(pf_Config);
	
ERR2:
	free(pc_InputFileName);
	
ERR1:
	free(pc_FilePath);

	return CH_FAILURE;
}

S32 CH_TOOL_OutputMergeImage(CONFIG_PARAM rstru_ConfigParam, NAND_FLASH_PARAM rstru_FlashParam,
											BOOL ri_WhetherLastLine, S8* rpc_MegreFileName, S8* rpc_FilePath,S32* rpi_EndAddr)
{
	CONFIG_PARAM		stru_ConfigParam = rstru_ConfigParam;
	NAND_FLASH_PARAM	stru_FlashParam = rstru_FlashParam;
	BOOL				i_WhetherLastLine = ri_WhetherLastLine;
	S8*		pc_MegreFileName = rpc_MegreFileName;		
	S32		i_PageSize = stru_FlashParam.i_pagesize;
	S32 	i_OobSize = stru_FlashParam.i_oobsize;
	S32 	i_BlockSize = stru_FlashParam.i_blocksize;
	
	FILE*	pf_Merge = NULL;
	FILE*	pf_Config = NULL;
	S8* 	pc_ConfigFileName = NULL;
	S8*		pc_InputFileName = NULL;
	S8*		pc_FilePath = NULL;
	S8*		pc_Buff = NULL;
	
	S32		i_BlockNumber = 0;
	S32		i_FillLength = 0;
	S32		i_EndAddr = 0;
	U32		ui_FileSize = 0;		
	S32 	i_Read = 0;
	U8 		uc_FillNumber = FILL_NUMBER;	
	S32   	i = 0;	

	/*** read file ,link path ***/	
	pc_ConfigFileName = stru_ConfigParam.uc_SelectFile;

	pc_FilePath = (char *) malloc(sizeof(pc_FilePath) + 1);
	if(NULL == pc_FilePath)
	{
		printf("[%s]:%d get memory false\n", __FUNCTION__ ,__LINE__);
		return CH_FAILURE;
	}	
	
	strcpy(pc_FilePath ,rpc_FilePath);
	
	pc_InputFileName = (char *) malloc(strlen(pc_FilePath) + strlen(pc_ConfigFileName) +1);
	if(NULL == pc_InputFileName)
	{
		printf("[%s]:%d get memory false\n", __FUNCTION__ ,__LINE__);
		goto ERR1;
	}

	memset(pc_InputFileName ,0 ,sizeof(pc_InputFileName));

	sprintf(pc_InputFileName ,"%s%s",pc_FilePath ,pc_ConfigFileName);

	/*** open burn file ***/
	if(NULL == pc_InputFileName || NULL == (pf_Config = fopen(pc_InputFileName ,"rb")))
	{
		printf("[%s] : open input file failed: %s , maybe not exist\n", __FUNCTION__ ,pc_InputFileName);
		goto ERR2;
	}
	
	/*** count file size ***/
	fseek(pf_Config ,OFFSET_RESET ,SEEK_END);
	
	ui_FileSize = ftell(pf_Config);
	
	/*** creat file : "Data_Merge" ***/
	if(NULL == pc_MegreFileName || NULL == (pf_Merge = fopen(pc_MegreFileName ,"ab+")))
	{
		printf("[%s]:%d open file failed\n", __FUNCTION__ ,__LINE__);
		goto ERR3;
	}

	/*** copy input file to "Data_Merge" ***/
	pc_Buff = (char *)malloc(BUF_SIZE);
	if (NULL == pc_Buff)
	{
		printf("[%s]:%d get memory false\n", __FUNCTION__ ,__LINE__);
		goto ERR4;	
	}

	fseek(pf_Config ,OFFSET_RESET ,SEEK_SET);
	
	while (1)
	{
		i_Read = fread(pc_Buff, 1, BUF_SIZE, pf_Config);
		
		if (i_Read != BUF_SIZE)
		{
			fwrite(pc_Buff, i_Read, 1, pf_Merge);
		}
		else
		{
			fwrite(pc_Buff, BUF_SIZE, 1, pf_Merge);
		}
		
		if (feof(pf_Config))
		{
			break;
		}
	}
	
	/*** use "fill_number" , fill "Data_Merge" to need size ***/	
	/* if the line is the last line */
	if(CH_TRUE == i_WhetherLastLine)
	{
		/* size of the last pratition, round up  */
		i_BlockNumber = ((ui_FileSize - 1) / ((i_PageSize + i_OobSize) * i_BlockSize)) + 1;

		i_FillLength = i_BlockNumber * ((i_PageSize + i_OobSize) * i_BlockSize) - ui_FileSize;

		for( i=0 ;i < i_FillLength ;i++ )
		{
	    	fwrite(&uc_FillNumber ,1 ,sizeof(uc_FillNumber) ,pf_Merge);
		}
	}
	else
	{
		i_FillLength = strtol(stru_ConfigParam.uc_Length, NULL, 16) / i_PageSize * (i_PageSize + i_OobSize) - ui_FileSize;
			
		for( i=0 ;i < i_FillLength ;i++ )
		{
	    	fwrite(&uc_FillNumber ,1 ,sizeof(uc_FillNumber) ,pf_Merge);
		}

		i_EndAddr = strtol(stru_ConfigParam.uc_StartAddr, NULL, 16) + strtol(stru_ConfigParam.uc_Length, NULL, 16);
		
		*rpi_EndAddr = i_EndAddr;
	}
	
	fclose(pf_Config);
	fclose(pf_Merge);
	free(pc_FilePath);
	free(pc_InputFileName);
	free(pc_Buff);
	
	return CH_SUCCESS;
	
ERR4:
	fclose(pf_Merge);
	
ERR3:
	fclose(pf_Config);
	
ERR2:
	free(pc_InputFileName);
	
ERR1:
	free(pc_FilePath);

	return CH_FAILURE;
}

S32 CH_TOOL_OutFile(S32 ri_FileLine, CONFIG_PARAM* rpstru_ConfigParam, NAND_FLASH_PARAM rstru_FlashParam 
								,S8* rpc_MegreFileName, S8* rpc_PratitionFileName, S8* rpc_FilePath)
{
	S32					i_FileLine = ri_FileLine;
	CONFIG_PARAM*		pstru_ConfigParam = rpstru_ConfigParam;
	NAND_FLASH_PARAM	stru_FlashParam = rstru_FlashParam;	
	S8*		pc_MegreFileName = rpc_MegreFileName;
	S8*		pc_PratitionFileName = rpc_PratitionFileName;
	S8*		pc_FilePath = rpc_FilePath;
	
	S32		i_CurFileLine = 0;
	S32		i_StartAddr = 0;
	S32		i_EndAddr = 0;
	S32		i_BlankLength = 0;
	S32		i_Result = 0;
	BOOL	b_WhetherLastLine = 0;
	S32		i = 0;

	if(NULL == rpstru_ConfigParam)
	{
		printf("[%s]:%d get config param faile\n", __FUNCTION__ ,__LINE__);
		return CH_FAILURE;
	}
	
	/*** remove file that last time create ***/
    if(CH_SUCCESS == access(rpc_MegreFileName, F_OK))
    {
        printf("remove %s \r\n", rpc_MegreFileName);
        remove(rpc_MegreFileName);
    }

    if(CH_SUCCESS == access(rpc_PratitionFileName, F_OK))
    {
        printf("remove %s \r\n", rpc_PratitionFileName);
        remove(rpc_PratitionFileName);
    }

	/*** output megre image and pratition table ***/
	for(i_CurFileLine = 0; i_CurFileLine < i_FileLine; i_CurFileLine++)
	{
		/* judge current line whether the last line  */
		if(i_CurFileLine == i_FileLine - 1)
		{
			b_WhetherLastLine = CH_TRUE;			
		}
		else
		{
			b_WhetherLastLine = CH_FALSE;
		}
		
		i_StartAddr = strtol(pstru_ConfigParam[i_CurFileLine].uc_StartAddr, NULL, 16);

		/* judge blank pratition; if exsit blank, fill numer 'ff' */
		if(i_StartAddr != i_EndAddr)
		{
			i_BlankLength = i_StartAddr - i_EndAddr;	
			
			i_Result = CH_TOOL_FillBlankPartition(i_BlankLength, stru_FlashParam, pc_MegreFileName);
			if(CH_SUCCESS != i_Result)
			{
				printf("[%s]:%d fill blank faile\n", __FUNCTION__ ,__LINE__);
				return CH_FAILURE;
			}
		}

		/* output pratition table */
		i_Result = CH_TOOL_OutputPratitionTable(pstru_ConfigParam[i_CurFileLine], stru_FlashParam, i_FileLine,
													b_WhetherLastLine, pc_PratitionFileName, pc_FilePath);
		if(CH_SUCCESS != i_Result)
		{
			printf("[%s]:%d output pratition table faile\n", __FUNCTION__ ,__LINE__);
			return CH_FAILURE;
		}
		
		/* output megre image */
		i_Result = CH_TOOL_OutputMergeImage(pstru_ConfigParam[i_CurFileLine], stru_FlashParam,
												b_WhetherLastLine, pc_MegreFileName, pc_FilePath, &i_EndAddr);		
		if(CH_SUCCESS != i_Result)
		{
			printf("[%s]:%d megre image faile\n", __FUNCTION__ ,__LINE__);
			return CH_FAILURE;
		}

	}		

	return CH_SUCCESS;
}

S32 main(S32 argc, S8* argv[])
{
	FILE*				pf_ConfigFile;
	CONFIG_PARAM*		pstru_ConfigParam = NULL;	
	NAND_FLASH_PARAM	stru_FlashParam = {0};
	S32		i_FileLine = 0;
	S32		i = 0;
	S32		i_Result = 0;
	
	/*** default params ***/
	S8 		c_ConfigFileName[BUF_SIZE] = "./updata/config.txt";
	S8 		c_MegreFileName[BUF_SIZE] = "Data_Merge.bin";
	S8 		c_PratitionFileName[BUF_SIZE] = "Partition_Table.bin";
	S8		c_FilePath[BUF_SIZE] = "./updata/";
	
	if (argc < 7)
	{
	    printf("Version:%s \n", VERSION_STRING);
		printf("Usage: %s [-p page_size] [-o oob_size] [-b block_size] [-I config_file] [-M output megre] [-P output pratition_table]\n"
				"\t\t%s will read 'config file' then create 'megre file' and 'pratition table'.\n"
				"		-p page_size : page size(Byte)\n"
				"		-o oob_size : oob size(Byte)\n"
				"		-b block_size : page count in each block\n"
				"		-I config file : the configuration file descript partition information, default './updata/config.txt'\n"
				"		-M megre file : output data merge file, default 'Data_Merge.bin'\n"
				"		-P partition table : output partition table file, default 'Partition_Table.bin'\n",			
				argv[0] ,argv[0]);

		printf("Example: %s -p 2048 -o 128 -b 64 -I ./updata/config.txt -M Data_Merge.bin -P Partition_Table.bin\n", argv[0]);
		return CH_FAILURE;
	}

	while ((i_Result = getopt(argc, argv, "p:o:b:I:M:P:")) != -1)
	{
		switch (i_Result) 
		{
			case 'p':	//page_size
			{
				stru_FlashParam.i_pagesize = atoi(optarg);
			}break;
			case 'o':	//oob_size
			{
				stru_FlashParam.i_oobsize = atoi(optarg);
			}break;
			case 'b':	//block_size
			{
				stru_FlashParam.i_blocksize = atoi(optarg);
			}break;			
			case 'I':	//config file path
			{
				sprintf(c_ConfigFileName, "%s", optarg);
				
				/** collect the path of config file  **/
				strcpy(c_FilePath ,c_ConfigFileName);
				for(i = strlen(c_FilePath) ;i > 0 ;i--)
				{
					if(c_FilePath[i] == '/')
					{
						c_FilePath[i + 1] = '\0';						
						break;		
					}
					if(1 == i)
					{
						c_FilePath[0] = '\0';
					}
				}
			}break;
			case 'M':	//megre file
			{
				sprintf(c_MegreFileName, "%s", optarg);						
			}break;
			case 'P':	//pratition table
			{
				sprintf(c_PratitionFileName, "%s", optarg);
			}break;	
			break;			
			default:
				printf("unknow args\n");
			break;
		}
	}

	/* judge whether name of output file  is illegal */
	if(CH_FALSE == strcmp(c_MegreFileName, c_PratitionFileName))
	{
		printf("Megre file's name can't same as Pratition file's name\n");
		return CH_FAILURE;
	}

	/*** count file line ***/	
	i_Result = CH_TOOL_GetFileLine(c_ConfigFileName , &i_FileLine);
	if(CH_FAILURE == i_Result)
	{
        printf("[%s]:%d Fail to get the file line!\n", __FUNCTION__ ,__LINE__);
        return CH_FAILURE;
    }
	
	/*** resolution file line ***/	
	i_Result = CH_TOOL_GetConfig(c_ConfigFileName, i_FileLine, &pstru_ConfigParam);
	if(CH_FAILURE == i_Result)
	{
        printf("[%s]:%d get config false\n", __FUNCTION__ ,__LINE__);
        return CH_FAILURE;
    }

	/*** check file ***/	
	i_Result = CH_TOOL_CheckFile(i_FileLine ,pstru_ConfigParam ,c_FilePath);
	if(CH_FAILURE == i_Result)
	{
		printf("[%s]:%d please rewrite input file\n", __FUNCTION__ ,__LINE__);
		free(pstru_ConfigParam);
		return CH_FAILURE;
    }
	
	/*** get megre file and pratition information file ***/
	i_Result = CH_TOOL_OutFile(i_FileLine, pstru_ConfigParam, stru_FlashParam,
								c_MegreFileName, c_PratitionFileName, c_FilePath);
	if(CH_FAILURE == i_Result)
	{
		printf("[%s]:%d out file false\n", __FUNCTION__ ,__LINE__);
		free(pstru_ConfigParam);
		return CH_FAILURE;
	}
			
	free(pstru_ConfigParam);
	
	return CH_SUCCESS;
}


