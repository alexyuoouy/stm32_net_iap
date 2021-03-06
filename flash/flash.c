/*
 * flash read and program
 *
 * Use HAL Library
 *
 * Change Logs:
 * Date	        Author      Notes
 * 2020.6.14	yuoouy      the first version
 *
 *
 *
*/

#include "flash.h"
#include "iap_config.h"


/**
 * read half word from flash
 *
 * @param  addr : data address
 *         buf  : read buffer
 *         size : read data size(16 bits)
 *
 * @return   -1 : Illegal address
 *            n : readed size
 */
static int flash_read_halfword(volatile unsigned short *addr, unsigned short *buf, int size)
{
    if (size <= 0)
    {
        DBG_LOG("flash read size is invalid!\n");
        return -1;
    }
    if (addr < (volatile unsigned short *)FLASH_BASE || addr > (volatile unsigned short *)FLASH_BANK1_END)
    {
        DBG_LOG("flash read Illegal address!\n");
        return -1;
    }
    int i;
    for(i = 0; i < size; i++)
    {
        buf[i] = addr[i];
    }
    return i;
}


/**
 * erase flash page
 *
 * @param  addr : start address
 *         size : write data size(16 bits)
 *
 * @return   HAL_StatusTypeDef
 */
static int flash_erase_page(volatile unsigned short *addr, int size)
{
    FLASH_EraseInitTypeDef FLASH_EraseInitStruct;
    FLASH_EraseInitStruct.TypeErase = FLASH_TYPEERASE_PAGES;
    FLASH_EraseInitStruct.PageAddress = (uint32_t)addr;
    FLASH_EraseInitStruct.NbPages = (size - 1)/(FLASH_PAGE_SIZE) + 1;
    uint32_t Page_err;
    int ret;
    ret = HAL_FLASHEx_Erase(&FLASH_EraseInitStruct, &Page_err);

    if (ret != 0)
    {
        DBG_LOG("flash write error!\n");
        return -ret;
    }

    return 0;
}
/**
 * write half word to flash
 *
 * @param  addr : data address
 *         buf  : write buffer
 *         size : write data size(16 bits)
 *
 * @return   -1 : Illegal address
 *            n : writed size
 */

static int flash_write_halfword(volatile unsigned short *addr, unsigned short *buf, int size)
{
    if (size <= 0)
    {
        DBG_LOG("flash write size is invalid!\n");
        return -1;
    }
    if (addr < (volatile unsigned short *)FLASH_BASE || addr > (volatile unsigned short *)FLASH_BANK1_END)
    {
        DBG_LOG("flash write Illegal address!: %08x\n", addr);
        return -1;
    }

    uint32_t i, address;
    address = (uint32_t)addr;
    HAL_StatusTypeDef status;
    for(i = 0; i < size; i++)
    {

        status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, (uint32_t)address, buf[i]);

        if (status != HAL_OK)
        {
            DBG_LOG("flash program error in:%08x\n", address);
            break;
        }
        address += 2;
    }
    return -status;
}




static struct Flash_fd flash_fd = {NULL, NULL, NULL, 0};


/**
 * flash set parameter and lock
 *
 * @param  start_addr   flash program start address
 *         size         the data size that need to write
 *         flag         write or read
 *
 * @return   -1 : error
 *            0 : ok
 *
 */
int flash_open( volatile unsigned short * start_addr,int size ,Open_Flash_Type flag )
{
    if ((uint32_t)start_addr % 2 == 1)
    {
        DBG_LOG("start address must be 2 bytes alined!\n");
        return -1;
    }
//  flash_get_fd();
//  if(flash_fd.lock == 1)
//  {
//      DBG_LOG("flash is locked, please close flash!\n");
//      return -1;
//  }

    //flash_fd.lock = 1;
    //flash_fd.num = 0;

    flash_fd.start_addr = start_addr;
    flash_fd.cur_ptr = (uint32_t)start_addr;
    flash_fd.RW_flag = flag;
    
    if ( (flag & 2) > 0 )
    {
        flash_fd.temp = 0;
        flash_fd.temp_flag = 0;
        flash_fd.size = size;
	 
        HAL_FLASH_Unlock();
        if (flash_erase_page(start_addr, size) != 0)
        {
            DBG_LOG("flash erase failed!\n");
            return -1;
        }
    }
    else
    {
        
    }
    while (FLASH_WaitForLastOperation((uint32_t)FLASH_TIMEOUT_VALUE) != HAL_OK);
	return 0;
}


/**
 * flash read data
 *
 * @param   buf 		read buffer
 *			length  	read length
 *
 * @return   -1 : error
 *            n : read length
 */
int flash_read(unsigned char *buf, int length)
{
    int read_length = length;
    if ((flash_fd.RW_flag & 1) == 0)
    {
        DBG_LOG("you can not read!\n");
        return -1;
    }
    if ((uint32_t)flash_fd.cur_ptr + read_length > (uint32_t)flash_fd.start_addr + flash_fd.size)
    {
        read_length = (uint32_t)flash_fd.start_addr + flash_fd.size - (uint32_t)flash_fd.cur_ptr;
    }
    else if((uint32_t)flash_fd.cur_ptr == (uint32_t)flash_fd.start_addr + flash_fd.size)																
    {
        return 0;
    }

	int i;
	if ((uint32_t)flash_fd.cur_ptr % 2 == 1)
	{
		int size;
		size = read_length / 2 + 1;
		unsigned short buffer[size];
		if(flash_read_halfword((volatile unsigned short *)(flash_fd.cur_ptr - 1), buffer, size) == -1)
		{
			DBG_LOG("flash read halfword error!\n");
			return -1;
		}

		for(i = 0; i < read_length; i++)
		{
			if (i % 2 == 0)
			{
				buf[i] = (char)(buffer[(i + 1) / 2] >> 8);
			}
			else
			{
				buf[i] = (char)(buffer[(i + 1) / 2] & 0x00ff);
			}
		}
	}
	else
	{
		int size;
		size = (read_length - 1) / 2 + 1;
		unsigned short buffer[size];
        //DBG_LOG("size = %d\n", size);
		if(flash_read_halfword((volatile unsigned short *)flash_fd.cur_ptr, buffer, size) == -1)
		{
			DBG_LOG("flash read halfword error!\n");
			return -1;
		}

		for(i = 0; i < read_length; i++)
		{
			if (i % 2 == 0)
			{
				buf[i] = (char)(buffer[i / 2] & 0x00ff);
			}
			else
			{
				buf[i] = (char)(buffer[i / 2] >> 8);
			}
//            DBG_LOG("buf[%d] = %c\n", i, buf[i]);
		}

//         for (i = 0; i < size; i++)
//        {
//            //unsigned short temp = flash_buffer[i];
//            DBG_LOG("read read_length:%d, buf size :%d\n", read_length, size);
//            DBG_LOG("%d:%c %c\n", i, (char)(buffer[i]>>8), (char)(buffer[i]&0x00ff));
//        }
	}
	flash_fd.cur_ptr += read_length;

	return i;
}


/**
 * flash write data
 *
 * @param   buf 		write buffer
 *			length  	write length
 *
 * @return   -1 : error
 *            n : write length
 */
int flash_write(unsigned char *buf, int length)
{
	int write_length = length;
    if ((flash_fd.RW_flag & 2) == 0)
    {
        DBG_LOG("you can not write!\n");
        return -1;
    }
	if (length == 0)
	{
		return 0;
	}
	if ((uint32_t)flash_fd.cur_ptr + write_length > (uint32_t)flash_fd.start_addr + flash_fd.size)
    {
        write_length = (uint32_t)flash_fd.start_addr + flash_fd.size - (uint32_t)flash_fd.cur_ptr;
    }
    else if((uint32_t)flash_fd.cur_ptr == (uint32_t)flash_fd.start_addr + flash_fd.size)
    {
        return 0;
    }
    //DBG_LOG("flash_write will write %d bytes to address%08x...\n", length, flash_fd.cur_ptr);

	if (flash_fd.temp_flag)
    {
        //DBG_LOG("last time temp is full\n");
        flash_fd.temp += buf[0] << 8;
        if (flash_write_halfword((volatile unsigned short *)flash_fd.cur_ptr, &flash_fd.temp, 1) == -1)
		{
			DBG_LOG("flash write error!\n");
			return -1;
		}
        flash_fd.cur_ptr += 2;
        flash_fd.temp = 0;
        flash_fd.temp_flag = 0;
        //DBG_LOG("flash_write writed %d bytes to flash, now current write pointer is %08x\n", 2, flash_fd.cur_ptr);

        if (flash_write_halfword((volatile unsigned short *)flash_fd.cur_ptr, (unsigned short *)(buf + 1), (length - 1) / 2) == -1)
        {
            DBG_LOG("flash write error!\n");
            return -1;
        }

        if ((length - 1) % 2 == 0)
        {
            flash_fd.cur_ptr += (length - 1);
            //DBG_LOG("flash_write writed %d bytes to flash, now current write pointer is %08x\n", length - 1, flash_fd.cur_ptr);

        }
        else
        {
            flash_fd.cur_ptr += (length - 2);
            flash_fd.temp += buf[length -1];
            flash_fd.temp_flag = 1;
            //DBG_LOG("add data to temp\n");
            //DBG_LOG("flash_write  writed %d bytes to flash, now current write pointer is %08x\n", length - 2, flash_fd.cur_ptr);

        }

        return length;
    }
    else
    {
        if (flash_write_halfword((volatile unsigned short *)flash_fd.cur_ptr, (unsigned short *)buf, length / 2) == -1)
        {
            DBG_LOG("flash write error!\n");
            return -1;
        }

        if (length % 2 == 0)
        {
            flash_fd.cur_ptr += length;
            //DBG_LOG("flash_write writed %d bytes to flash, now current write pointer is %08x\n", length, flash_fd.cur_ptr);
        }
        else
        {
            flash_fd.cur_ptr += length - 1;
            flash_fd.temp += buf[length -1];
            flash_fd.temp_flag = 1;
            //DBG_LOG("add data to temp\n");
            //DBG_LOG("flash_write writed %d bytes to flash, now current write pointer is %08x\n", length - 1, flash_fd.cur_ptr);
        }

        return length;
    }
}

/**
 * flash change pointer position 
 *
 * @param   offset      偏移量
 *          whence      SEEK_SET    从开始偏移（offset不支持负值）
 *                      SEEK_CUR    从当前偏移
 *                      SEEK_END    从结尾偏移（offset不支持正值）
 *
 * @return   -1 : error
 *            0 : ok
 */
int flash_lseek(int offset, int whence)
{
    switch (whence)
    {
        case SEEK_SET:
            if (offset < 0)
            {
                DBG_LOG("you can not set current position before start position!\n");
                return -1;
            }
            else
            {
                flash_fd.cur_ptr = (uint32_t)flash_fd.start_addr + offset;
            }
            break;
        case SEEK_CUR:
            flash_fd.cur_ptr = flash_fd.cur_ptr + offset;
            break;
        case SEEK_END:
            if (offset > 0)
            {
                DBG_LOG("you can not set current position after end position!\n");
                return -1;
            }
            else
            {
                flash_fd.cur_ptr = (uint32_t)flash_fd.start_addr + flash_fd.size - 1 + offset;
            }
            break;
    }
    return 0;       
}

/**
 * flash unlock ,write the remaining data to flash and clear parameter
 *
 * @param   none
 *
 * @return   -1 : error
 *            0 : ok
 */
int flash_close(void)
{
//	if(flash_fd.lock == 0)
//	{
//		return 0;
//	}
	if (flash_fd.temp_flag)
    {
        if (flash_write_halfword((volatile unsigned short *)flash_fd.cur_ptr, &flash_fd.temp, 1) == -1)
		{
			DBG_LOG("flash write error!\n");
			return -1;
		}
        
        flash_fd.temp = 0;
        flash_fd.temp_flag = 0;
    }

    HAL_FLASH_Lock();
	flash_fd.start_addr = NULL;
	flash_fd.cur_ptr = NULL;
	

	return 0;
}
