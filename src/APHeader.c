/*
 * APHeader.c
 *
 *  Created on: Nov 18, 2012
 *      Author: xonar
 */

#include "APHeader.h"

APHeader readAPHeader(FILE *f)
{
    APHeader out;
    uint32_t* magic = (uint32_t*) &out.magic;

    INIT_APHEADER(out);

    /*READ MAGIC NUMBER*/
    fread(&out, sizeof(char) * 4, 1, f);

    /*PASS TO CORRECT FUNCTION BASED ON MAGIC NUMBER*/
    switch(*magic)
    {
        case 0xaa55a5a5:
            out = readAPHeaderA5A555AA(f);
            break;
        case 0xaa55dd44:
            out = readAPHeader44DD55AA(f);
            break;
        default:
            fprintf(stderr, "Unsupported Bin Type : ");
            printHex(stderr, magic, sizeof(*magic));
            fprintf(stderr, "\n");
            break;
    }

    return out;
}

APHeader readAPHeaderA5A555AA(FILE *f)
{
    APHeader out;
    int i = 0;

    INIT_APHEADER(out);

    out.magic.magic=0xaa55a5a5;

    /*COUNT AND ALLOCATE AP ENTRIES*/
    while(1)
    {
        uint64_t tmp;
        fread(&tmp, sizeof(tmp), 1, f);
        if(tmp == UINT64_MAX) break;
        else out.pent_num++;
    }

    fseek(f, 4, SEEK_SET);

    out.pent_arr = (APPartitionEntry*) calloc(out.pent_num, sizeof(APPartitionEntry));

    /*READ FILE OFFSET AND SIZE*/
    for(; i < out.pent_num; i++)
    {
        fread(&out.pent_arr[i].file_off, sizeof(uint32_t), 1, f);
        fread(&out.pent_arr[i].file_size, sizeof(uint32_t), 1, f);
    }

    skipToNextLBA(f);

    /*READ DISK SIZE IGNORING FIRST ID REFERENCE*/
    for(i = 0; i < out.pent_num; i++)
    {
        /*fread(&out.pent_arr[i].pent_id,sizeof(char),1,f);*/
        fseek(f, 4, SEEK_CUR);
        fread(&out.pent_arr[i].disk_size, sizeof(uint32_t), 1, f);
        fseek(f, 504, SEEK_CUR);
    }

    fseek(f, 0x2200, SEEK_SET);

    /*READ ID AND NAME AND SET DISK OFF*/
    for(i = 0; i < out.pent_num; i++)
    {
        fread(&out.pent_arr[i].pent_id, sizeof(uint32_t), 1, f);
        fread(&out.pent_arr[i].name, sizeof(char), 256, f);
        out.pent_arr[i].disk_off = 0xFFFFFFFF;
    }

    return out;
}

APHeader readAPHeader44DD55AA(FILE *f)
{
    APHeader out;
    int i = 4, j;
    char buf[512];

    DataBlock dataBlock, *curDataBlock;
    MagicNumber *curMagicNum;
    uint32_t tmp;

    INIT_APHEADER(out);

    /*READ FIRST MAGIC NUMER*/
    curMagicNum = &out.magic;

    fseek(f, 0, SEEK_SET);
    fread(&curMagicNum->magic, sizeof(uint32_t), 1, f);

    /*SCAN FOR MAGIC NUMBERS*/
    for(; i < 0x2004; i += 4)
    {
        fread(&tmp, sizeof(uint32_t), 1, f);

        if(tmp != 0xffffffff)
        {
            /*FOUND MAGIC NUM*/
            curMagicNum->next = malloc(sizeof(MagicNumber));
            curMagicNum = curMagicNum->next;
            curMagicNum->magic = tmp;
            curMagicNum->off = i;
        }
    }

    /*START WITH 44 DD 55 AA and ENDING WITH 33 EC 55 AA*/
    if(out.magic.magic != 0xaa55dd44 || curMagicNum->magic != 0xaa55ec33)
    {
        fprintf(stderr, "Not 0x0    :44DD55AA\n    0x2000 :33EC55AA\n");
        goto failed;
    }

    curMagicNum = &out.magic;

    /*SET FIRST DATABLOCK*/
    dataBlock.blockOff = 0x2010;
    dataBlock.blockSize = 12;
    dataBlock.numItems = 4;

    dataBlock.items = calloc(sizeof(Item), 4);
    dataBlock.items[0].type = DISK_OFF;
    dataBlock.items[0].size = 4;
    dataBlock.items[1].type = FILE_OFF;
    dataBlock.items[1].size = 4;
    dataBlock.items[2].type = FILE_SIZE;
    dataBlock.items[2].size = 4;
    dataBlock.items[3].type = SKIP;
    dataBlock.items[3].size = 4;

    dataBlock.next = NULL;

    curDataBlock = &dataBlock;

    /*TOT FILES*/
    if(out.magic.next->off == 0x8 && out.magic.next->next->off == 0x2000) switch(out.magic.next->magic)
    {
        default:
            printf("Did not detect known magic numbers!\n"
                    "  The File Appears to be a TOT File.All known TOT Files\n"
                    "  have the same format with different magic numbers.\n"
                    "  Do you want to assume that and try? Y/N");

            scanf("%s", buf);

            if(buf[0] == 'Y' || buf[0] == 'y') goto tot;
            else break;

        case 0x8978f62b:
        case 0x5062c8ea:
        case 0x49838b94:
        case 0xdebf33af:
            tot: curDataBlock->next = malloc(sizeof(DataBlock));
            curDataBlock = curDataBlock->next;
            curDataBlock->blockOff = 0x4220;
            curDataBlock->blockSize = 0x20;

            curDataBlock->numItems = 1;
            curDataBlock->items = calloc(sizeof(Item), 1);
            curDataBlock->items[0].type = BLOCK_NAME;
            curDataBlock->items[0].size = 0x20;

            goto readBlocks;
    }

    if(out.magic.next->off == 0x600 && out.magic.next->next->off == 0x2000)
        switch(out.magic.next->magic)
        {
            case 0xcc00bbaa:
                curDataBlock->next = malloc(sizeof(DataBlock));
                curDataBlock = curDataBlock->next;
                curDataBlock->blockOff = 0x2400;
                curDataBlock->blockSize = 0x20;

                curDataBlock->numItems = 5;
                curDataBlock->items = calloc(sizeof(Item), 5);
                curDataBlock->items[0].type = BLOCK_ID;
                curDataBlock->items[0].size = 4;
                curDataBlock->items[1].type = DISK_SIZE;
                curDataBlock->items[1].size = 4;
                curDataBlock->items[2].type = SKIP;
                curDataBlock->items[2].size = 4;
                curDataBlock->items[3].type = BLOCK_NAME;
                curDataBlock->items[3].size = 0x14;
                curDataBlock->items[4].type = SKIP;
                curDataBlock->items[4].size = 0x1E0;

                goto readBlocks;
        }

    if(out.magic.next->off == 0x2000)
    {
        curDataBlock->next = malloc(sizeof(DataBlock));
        curDataBlock = curDataBlock->next;
        curDataBlock->blockOff = 0x2404;
        curDataBlock->blockSize = 0x14;

        curDataBlock->numItems = 1;
        curDataBlock->items = calloc(sizeof(Item), 1);
        curDataBlock->items[0].type = BLOCK_NAME;
        curDataBlock->items[0].size = 0x14;
        goto readBlocks;
    }

    /*SEARCH FOR NAMES*/
    goto failed;

    readBlocks:

    /*COUNT AND ALLOCATE AP ENTRIES*/
    fseek(f, 0x2010, SEEK_SET);

    fread(buf, sizeof(char), 0x10, f);

    while(*((uint32_t*) buf) != 0xFFFFFFFF)
    {
        out.pent_num++;
        fread(buf, sizeof(uint32_t), 4, f);
    }

    out.pent_arr = (APPartitionEntry*) calloc(out.pent_num, sizeof(APPartitionEntry));

    /*INIT AP PARTITION ENTRIES*/
    for(i = 0; i < out.pent_num; i++)
    {
        out.pent_arr[i].disk_off = 0xffffffff;
        out.pent_arr[i].file_off = 0xffffffff;
        out.pent_arr[i].disk_size = 0xffffffff;
        out.pent_arr[i].file_size = 0xffffffff;
        out.pent_arr[i].pent_id = 0xffffffff;
        out.pent_arr[i].name[0] = '\0';
    }

    /*READ APDATA*/
    curDataBlock = &dataBlock;

    while(curDataBlock != NULL )
    {
        fseek(f, curDataBlock->blockOff, SEEK_SET);

        for(i = 0; i < out.pent_num; i++)
        {
            for(j = 0; j < curDataBlock->numItems; j++)
            {
                switch(curDataBlock->items[j].type)
                {
                    case DISK_SIZE:
                        fread(&out.pent_arr[i].disk_size, curDataBlock->items[j].size, 1, f);
                        break;
                    case DISK_OFF:
                        fread(&out.pent_arr[i].disk_off, curDataBlock->items[j].size, 1, f);
                        break;
                    case FILE_SIZE:
                        fread(&out.pent_arr[i].file_size, curDataBlock->items[j].size, 1, f);
                        break;
                    case FILE_OFF:
                        fread(&out.pent_arr[i].file_off, curDataBlock->items[j].size, 1, f);
                        break;
                    case BLOCK_ID:
                        fread(&out.pent_arr[i].pent_id, curDataBlock->items[j].size, 1, f);
                        break;
                    case BLOCK_NAME:
                        fread(&out.pent_arr[i].name, sizeof(char), curDataBlock->items[j].size, f);
                        break;
                    case SKIP:
                        fseek(f, curDataBlock->items[j].size, SEEK_CUR);
                        break;
                }
            }
        }

        curDataBlock = curDataBlock->next;
    }

    /*MAKE SURE ID IS SET*/
    for(i = 0; i < out.pent_num; i++)
        if(out.pent_arr[i].pent_id == 0xffffffff) out.pent_arr[i].pent_id = i;

    return out;
    failed: fprintf(stderr, "Failed Reading APHeader!\n");
    return out;
}

void printAPHeader(const APHeader h, FILE *f)
{
    /*PRINT MAGIC NUMBER(S)*/
    MagicNumber* tmp = h.magic.next;
    fprintf(f, "AP HEADER\n----------\n%-30s0x%-8X: ", "Magic Number",h.magic.off);

    printHex(f, &h.magic.magic, 4);

    while(tmp)
    {
        fprintf(f, "\n%-30s0x%-8X: ", "",tmp->off);
        printHex(f, &tmp->magic, 4);
        tmp = tmp->next;
    }

    /*NUMBER OF PARTITIONS*/
    fprintf(f, "\n%-30s%d\n", "Number of Partitions", h.pent_num);
}

void printAPPartitionEntry(const APPartitionEntry pe, FILE *f)
{
    fprintf(f, "PARTITION ENTRY\n------------\n"
            "    %-26s%s\n"
            "    %-26s%d\n"
            "    %-26s%" PRIu32 "\n"
    "    %-26s%" PRIu32 "\n", "Data Block Name", pe.name, "Data Block ID", pe.pent_id,
            "Size on File", pe.file_size, "File Offset", pe.file_off);

    if(pe.disk_size != 0xFFFFFFFF)
    {
        fprintf(f, "    %-26s%" PRIu32 "\n", "Size on Disk", pe.disk_size);
    }

    if(pe.disk_off != 0xFFFFFFFF)
    {
        fprintf(f, "    %-26s%" PRIu32 "\n", "Disk Offset", pe.disk_off);
    }
}

void printFullAPInfo(const APHeader h, FILE *f)
{
    int i = 0;

    printAPHeader(h, f);

    fprintf(f, "\nPARTITION ENTRIES\n-----------------\n");

    for(; i < h.pent_num; i++)
    {
        fputs("\n", f);
        printAPPartitionEntry(h.pent_arr[i], f);
    }
}
