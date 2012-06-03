/*
 * Driver for S5K6AAFX from Samsung Electronics
 *
 * 1/6" 1.3Mp CMOS Image Sensor SoC with an Embedded Image Processor
 *
 * Copyright (C) 2010, Dongsoo Nathaniel Kim<dongsoo45.kim@samsung.com>
 * Copyright (C) 2010, HeungJun Kim<riverful.kim@samsung.com>
 * Copyright (C) 2010, Arun c <arun.c@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/version.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-i2c-drv.h>
#include <linux/slab.h>
#ifdef CONFIG_VIDEO_SAMSUNG_V4L2
#include <linux/videodev2_samsung.h>
#endif
#include <media/s5k6aafx_platform.h>
#include "s5k6aafx.h"

//#define CONFIG_LOAD_FILE
#define S5K6AAFX_USE_BURSTMODE
static unsigned int i2c_retry = 0;

#ifdef CONFIG_LOAD_FILE
#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
//#define max_size 200000

struct test {
	char data;
	struct test *nextBuf;
};
struct test *testBuf;
#endif

//#define FUNC_DEBUG

#ifdef FUNC_DEBUG
#define FUNC_ENTR() printk("[~~~~ S5K6AAFX ~~~~] %s entered\n", __func__)
#else
#define FUNC_ENTR()
#endif

extern long simple_strtol(const char *,char **,unsigned int);

static inline int s5k6aafx_read(struct i2c_client *client,
	unsigned short subaddr, unsigned short *data)
{
	unsigned char buf[2];
	int err = 0;
	struct i2c_msg msg = {client->addr, 0, 2, buf};

	*(unsigned short *)buf = cpu_to_be16(subaddr);

	//printk(KERN_ERR "\n\n\n%X %X\n\n\n", buf[0], buf[1]);

	err = i2c_transfer(client->adapter, &msg, 1);
	if (unlikely(err < 0))
		printk("%s: %d register read fail\n", __func__, __LINE__);

	msg.flags = I2C_M_RD;

	err = i2c_transfer(client->adapter, &msg, 1);
	if (unlikely(err < 0))
		printk("%s: %d register read fail\n", __func__, __LINE__);

	//printk(KERN_ERR "\n\n\n%X %X\n\n\n", buf[0], buf[1]);

	*data = ((buf[0] << 8) | buf[1]);

	return err;
}

/*
 * s5k6aafx sensor i2c write routine
 * <start>--<Device address><2Byte Subaddr><2Byte Value>--<stop>
 */
#ifdef CONFIG_LOAD_FILE
static int loadFile(void)
{
	struct file *fp;
	struct test *nextBuf = testBuf;

	char *nBuf;
	int max_size;
	int l;
	int i = 0;
	//int j = 0;
	int starCheck = 0;
	int check = 0;
	int ret = 0;
	loff_t pos;

	mm_segment_t fs = get_fs();
	set_fs(get_ds());

	fp = filp_open("/mnt/sdcard/external_sd/s5k6aafx.h", O_RDONLY, 0);

	if (IS_ERR(fp))
	{
		printk("%s : file open error\n", __func__);
		return PTR_ERR(fp);
	}

	l = (int) fp->f_path.dentry->d_inode->i_size;

	max_size = l;

	printk("l = %d\n", l);
	nBuf = kmalloc(l, GFP_KERNEL);
	testBuf = (struct test*)kmalloc(sizeof(struct test) * l, GFP_KERNEL);

	if (nBuf == NULL)
	{
		printk( "Out of Memory\n");
		filp_close(fp, current->files);
	}

	pos = 0;
	memset(nBuf, 0, l);
	memset(testBuf, 0, l * sizeof(struct test));

	ret = vfs_read(fp, (char __user *)nBuf, l, &pos);

	if (ret != l)
	{
		printk("failed to read file ret = %d\n", ret);
		kfree(nBuf);
		kfree(testBuf);
		filp_close(fp, current->files);
		return -1;
	}

	filp_close(fp, current->files);

	set_fs(fs);

	i = max_size;

	printk("i = %d\n", i);

	while (i)
	{
		testBuf[max_size - i].data = *nBuf;
		if (i != 1)
		{
			testBuf[max_size - i].nextBuf = &testBuf[max_size - i + 1];
		}
		else
		{
			testBuf[max_size - i].nextBuf = NULL;
			break;
		}
		i--;
		nBuf++;
	}

	i = max_size;
	nextBuf = &testBuf[0];

#if 1
	while (i - 1)
	{
		if (!check && !starCheck)
		{
			if (testBuf[max_size - i].data == '/')
			{
				if(testBuf[max_size-i].nextBuf != NULL)
				{
					if (testBuf[max_size-i].nextBuf->data == '/')
					{
						check = 1;// when find '//'
						i--;
					}
					else if (testBuf[max_size-i].nextBuf->data == '*')
					{
						starCheck = 1;// when find '/*'
						i--;
					}
				}
				else
				{
					break;
				}
			}
			if (!check && !starCheck)
			{
				if (testBuf[max_size - i].data != '\t')//ignore '\t'
				{
					nextBuf->nextBuf = &testBuf[max_size-i];
					nextBuf = &testBuf[max_size - i];
				}
			}
		}
		else if (check && !starCheck)
		{
			if (testBuf[max_size - i].data == '/')
			{
				if(testBuf[max_size-i].nextBuf != NULL)
				{
					if (testBuf[max_size-i].nextBuf->data == '*')
					{
						starCheck = 1;// when find '/*'
						check = 0;
						i--;
					}
				}
				else
				{
					break;
				}
			}

			if(testBuf[max_size - i].data == '\n' && check) // when find '\n'
			{
				check = 0;
				nextBuf->nextBuf = &testBuf[max_size - i];
				nextBuf = &testBuf[max_size - i];
			}

		}
		else if (!check && starCheck)
		{
			if (testBuf[max_size - i].data == '*')
			{
				if(testBuf[max_size-i].nextBuf != NULL)
				{
					if (testBuf[max_size-i].nextBuf->data == '/')
					{
						starCheck = 0;// when find '*/'
						i--;
					}
				}
				else
				{
					break;
				}
			}
		}

		i--;

		if (i < 2)
		{
			nextBuf = NULL;
			break;
		}

		if (testBuf[max_size - i].nextBuf == NULL)
		{
			nextBuf = NULL;
			break;
		}
	}
#endif

#if 0 // for print
	printk("i = %d\n", i);
	nextBuf = &testBuf[0];
	while (1)
	{
		//printk("sdfdsf\n");
		if (nextBuf->nextBuf == NULL)
			break;
		printk("%c", nextBuf->data);
		nextBuf = nextBuf->nextBuf;
	}
#endif

	return 0;
}
#endif

static inline int s5k6aafx_write(struct i2c_client *client,
		unsigned long packet)
{
	unsigned char buf[4];

	int err = 0;
	int retry_count = 5;

	struct i2c_msg msg =
	{
		.addr	= client->addr,
		.flags	= 0,
		.buf	= buf,
		.len	= 4,
	};

	if (!client->adapter)
	{
	  dev_err(&client->dev, "%s: can't search i2c client adapter\n", __func__);
	  return -EIO;
	}

	while(retry_count--)
	{
		*(unsigned long *)buf = cpu_to_be32(packet);
		err = i2c_transfer(client->adapter, &msg, 1);
		if (likely(err == 1))
			break;
		mdelay(10);
	}

	if (unlikely(err < 0))
	{
		dev_err(&client->dev, "%s: 0x%08x write failed\n", __func__, (unsigned int)packet);
		return err;
	}

	return (err != 1) ? -1 : 0;
}

#ifdef CONFIG_LOAD_FILE

static int s5k6aafx_write_regs_from_sd(struct v4l2_subdev *sd, char s_name[])
{

	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct test *tempData;

	int ret = -EAGAIN;
	unsigned long temp;
	char delay = 0;
	char data[11];
	int searched = 0;
	int size = strlen(s_name);
	int i;

	FUNC_ENTR();

	printk("size = %d, string = %s\n", size, s_name);
	tempData = &testBuf[0];
	while(!searched)
	{
		searched = 1;
		for (i = 0; i < size; i++)
		{
			if (tempData->data != s_name[i])
			{
				searched = 0;
				break;
			}
			tempData = tempData->nextBuf;
		}
		tempData = tempData->nextBuf;
	}
	//structure is get..

	while(1)
	{
		if (tempData->data == '{')
		{
			break;
		}
		else
		{
			tempData = tempData->nextBuf;
		}
	}

	while (1)
	{
		searched = 0;
		while (1)
		{
			if (tempData->data == 'x')
			{
				//get 10 strings
				data[0] = '0';
				for (i = 1; i < 11; i++)
				{
					data[i] = tempData->data;
					tempData = tempData->nextBuf;
				}
				//printk("%s\n", data);
				temp = simple_strtoul(data, NULL, 16);
				break;
			}
			else if (tempData->data == '}')
			{
				searched = 1;
				break;
			}
			else
			{
				tempData = tempData->nextBuf;
			}

			if (tempData->nextBuf == NULL)
			{
				return -1;
			}
		}

		if (searched)
		{
			break;
		}

		//let search...
		if ((temp & S5K6AAFX_DELAY) == S5K6AAFX_DELAY)
		{
			delay = temp & 0xFFFF;
			//printk("func(%s):line(%d):delay(0x%x):delay(%d)\n",__func__,__LINE__,delay,delay);
			msleep(delay);
			continue;
		}

		ret = s5k6aafx_write(client, temp);

		/* In error circumstances */
		/* Give second shot */
		if (unlikely(ret))
		{
			dev_info(&client->dev,
					"s5k6aafx i2c retry one more time\n");
			ret = s5k6aafx_write(client, temp);

			/* Give it one more shot */
			if (unlikely(ret))
			{
				dev_info(&client->dev,
						"s5k6aafx i2c retry twice\n");
				ret = s5k6aafx_write(client, temp);
			}
		}
	}

	return ret;
}
#endif

#ifdef S5K6AAFX_USE_BURSTMODE
#define BURST_MODE_BUFFER_MAX_SIZE 2700
unsigned char s5k6aafx_buf_for_burstmode[BURST_MODE_BUFFER_MAX_SIZE];
static int s5k6aafx_write_regs(struct v4l2_subdev *sd,
		unsigned long *packet, unsigned int num)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret = -EAGAIN;	/* FIXME */
	unsigned long temp, temp_next;
	char delay = 0;

	int idx = 0;
	int retry = 0;
	unsigned short subaddr=0,next_subaddr=0;
	unsigned short value=0;
	struct i2c_msg msg = { client->addr, 0, 0, s5k6aafx_buf_for_burstmode };

I2C_RETRY:
	idx = 0;
	while (num--)
	{
		temp = *packet++;

		if ((temp & S5K6AAFX_DELAY) == S5K6AAFX_DELAY)
		{
			delay = temp & 0xFFFF;
			//printk("func(%s):line(%d):delay(0x%x):delay(%d)\n",__func__,__LINE__,delay,delay);
			msleep(delay);
			continue;
		}

		subaddr = (temp >> 16); //address
		if(subaddr == 0x0F12) 
		{
			temp_next = *packet;
			next_subaddr= (temp_next >> 16); //address
		}
		value = (temp & 0xFFFF); //value

		switch(subaddr)
		{
			case 0x0F12 :
				// make and fill buffer for burst mode write
				if(idx ==0) 
				{
					s5k6aafx_buf_for_burstmode[idx++] = 0x0F;
					s5k6aafx_buf_for_burstmode[idx++] = 0x12;
				}
				s5k6aafx_buf_for_burstmode[idx++] = value>> 8;
				s5k6aafx_buf_for_burstmode[idx++] = value & 0xFF;
			 	//write in burstmode
				if(next_subaddr != 0x0F12)
				{
					msg.len = idx;
					ret = i2c_transfer(client->adapter, &msg, 1) == 1 ? 0 : -EIO;
					//printk("s5k4ecgx_sensor_burst_write, idx = %d\n",idx);
					idx=0;
				}
				break;
			case 0xFFFF :
				break;
			default:
				// Set Address
				idx=0;
				ret = s5k6aafx_write(client, temp);
				break;
		}
	}

	if (unlikely(ret < 0))
	{
		printk("[S5K6AAFX]%s: register set failed. try again.\n",__func__);
		i2c_retry++;
		if((retry++)<10)goto I2C_RETRY;
		return ret;
	}
	dev_info(&client->dev, "S5K6AAFX register programming ends up\n");
	return 0;	/* FIXME */
}

#else

/* program multiple registers */
static int s5k6aafx_write_regs(struct v4l2_subdev *sd,
		unsigned long *packet, unsigned int num)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret = -EAGAIN;	/* FIXME */
	unsigned long temp;
	char delay = 0;

	while (num--)
	{
		temp = *packet++;

/*
		if ((temp & S5K6AAFX_DELAY) == S5K6AAFX_DELAY)
		{
			if (temp & 0x1)
			{
				dev_info(&client->dev, "delay for 100msec\n");
				msleep(100);
				continue;
			}
			else
			{
				dev_info(&client->dev, "delay for 10msec\n");
				msleep(10);
				continue;
			}
		}
*/

#if 1
		if ((temp & S5K6AAFX_DELAY) == S5K6AAFX_DELAY)
		{
			delay = temp & 0xFFFF;
			//printk("func(%s):line(%d):delay(0x%x):delay(%d)\n",__func__,__LINE__,delay,delay);
			msleep(delay);
			continue;
		}
#endif
		ret = s5k6aafx_write(client, temp);

		/* In error circumstances */
		/* Give second shot */
		if (unlikely(ret))
		{
			dev_info(&client->dev,
				"s5k6aafx i2c retry one more time\n");
			ret = s5k6aafx_write(client, temp);

			/* Give it one more shot */
			if (unlikely(ret))
			{
				dev_info(&client->dev,
					"s5k6aafx i2c retry twice\n");
				ret = s5k6aafx_write(client, temp);
				break;
			}
		}
	}

	dev_info(&client->dev, "S5K6AAFX register programming ends up\n");
	if( ret < 0)
		return -EIO;

	return ret;	/* FIXME */
}
#endif


static int s5k6aafx_set_capture_start(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct s5k6aafx_state *state = to_state(sd);

	int err = -EINVAL;
	unsigned short lvalue = 0;

	FUNC_ENTR();

	s5k6aafx_write(client, 0x002C7000);
	s5k6aafx_write(client, 0x002E1AAA);
	s5k6aafx_read(client, 0x0F12, &lvalue); //read light value

	printk("%s : light value is %x\n", __func__, lvalue);

	/* set initial regster value */
#ifdef CONFIG_LOAD_FILE
	err = s5k6aafx_write_regs_from_sd(sd, "s5k6aafx_capture");
#else
	err = s5k6aafx_write_regs(sd, s5k6aafx_capture,
		sizeof(s5k6aafx_capture) / sizeof(s5k6aafx_capture[0]));
#endif
	if (lvalue < 0x40) {
		printk("\n----- low light -----\n\n");
		msleep(100);//add 100 ms delay for capture sequence
	} else {
		printk("\n----- normal light -----\n\n");
		msleep(50);
	}

	if (unlikely(err))
	{
		printk("%s: failed to make capture\n", __func__);
		return err;
	}

	state->runmode = S5K6AAFX_RUNMODE_IDLE;

	return err;
}

static int s5k6aafx_set_preview_start(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct s5k6aafx_state *state = to_state(sd);

	int err = -EINVAL;

	FUNC_ENTR();

	dev_info(&client->dev, "%s: set_Preview_start\n", __func__);


	if(state->check_dataline) {		// Output Test Pattern from VGA sensor
	     printk(" pattern on setting~~~~~~~~~~~\n");
	     err = s5k6aafx_write_regs(sd, s5k6aafx_pattern_on, sizeof(s5k6aafx_pattern_on) / sizeof(s5k6aafx_pattern_on[0]));
          //  mdelay(200);
	} else {
		if((!state->vt_mode) && (state->set_vhflip == 1)) {
			err = s5k6aafx_write_regs(sd, s5k6aafx_vhflip_on,
					sizeof(s5k6aafx_vhflip_on) / sizeof(s5k6aafx_vhflip_on[0]));
		} else if((!state->vt_mode) && (state->set_vhflip == 0)) {
			err = s5k6aafx_write_regs(sd, s5k6aafx_vhflip_off,
					sizeof(s5k6aafx_vhflip_off) / sizeof(s5k6aafx_vhflip_off[0]));
		}
		//msleep(100);
		/* set initial regster value */
#ifdef CONFIG_LOAD_FILE
	        err = s5k6aafx_write_regs_from_sd(sd, "s5k6aafx_preview");
#else
	        err = s5k6aafx_write_regs(sd, s5k6aafx_preview,
				sizeof(s5k6aafx_preview) / sizeof(s5k6aafx_preview[0]));
#endif
	        if (unlikely(err)) {
	                printk("%s: failed to make preview\n", __func__);
	                return err;
	            }
		}

		state->runmode = S5K6AAFX_RUNMODE_RUNNING;
		msleep(200); // add 200 ms for displaying preview

	return err;
}

static int s5k6aafx_set_preview_stop(struct v4l2_subdev *sd)
{
	struct s5k6aafx_state *state = to_state(sd);
	int err = 0;

	FUNC_ENTR();

	state->runmode = S5K6AAFX_RUNMODE_IDLE;

	return err;
}

#if 0
static int s5k6aafx_s_crystal_freq(struct v4l2_subdev *sd, u32 freq, u32 flags)
{
	int err = -EINVAL;

	FUNC_ENTR();
	return err;
}
#endif

static int s5k6aafx_g_fmt(struct v4l2_subdev *sd, struct v4l2_format *fmt)
{
	FUNC_ENTR();
	return 0;
}

static int s5k6aafx_enum_framesizes(struct v4l2_subdev *sd, \
					struct v4l2_frmsizeenum *fsize)
{
	struct s5k6aafx_state *state = to_state(sd);

	FUNC_ENTR();

	/*
	 * Return the actual output settings programmed to the camera
	 */
	if((state->set_fmt.width == 480) && (state->set_fmt.height == 640)) {
		fsize->discrete.width = 640;
		fsize->discrete.height = 480;
	} 
	else if ((state->set_fmt.width == 960) && (state->set_fmt.height == 1280)) {
		fsize->discrete.width = 1280;
		fsize->discrete.height = 960;
	}
	else {
		fsize->discrete.width = state->set_fmt.width;
		fsize->discrete.height = state->set_fmt.height;
	}
	printk("%s : width - %d , height - %d\n", __func__, fsize->discrete.width, fsize->discrete.height);

	return 0;
}

#if 0
static int s5k6aafx_enum_fmt(struct v4l2_subdev *sd, struct v4l2_fmtdesc *fmtdesc)
{
	int err = 0;

	FUNC_ENTR();
	return err;
}

static int s5k6aafx_enum_frameintervals(struct v4l2_subdev *sd,
					struct v4l2_frmivalenum *fival)
{
	int err = 0;

	FUNC_ENTR();
	return err;
}
#endif

static int s5k6aafx_try_fmt(struct v4l2_subdev *sd, struct v4l2_format *fmt)
{
	int err = 0;

	FUNC_ENTR();

	return err;
}

static int s5k6aafx_s_fmt(struct v4l2_subdev *sd, struct v4l2_format *fmt)
{
	struct s5k6aafx_state *state = to_state(sd);

	FUNC_ENTR();

	/*
	 * Just copying the requested format as of now.
	 * We need to check here what are the formats the camera support, and
	 * set the most appropriate one according to the request from FIMC
	 */
	state->req_fmt.width = fmt->fmt.pix.width;
	state->req_fmt.height = fmt->fmt.pix.height;
	state->set_fmt.width = fmt->fmt.pix.width;
	state->set_fmt.height = fmt->fmt.pix.height;

	state->req_fmt.pixelformat = fmt->fmt.pix.pixelformat;
	state->req_fmt.colorspace = fmt->fmt.pix.colorspace;

	printk("%s : width - %d , height - %d\n", __func__, state->req_fmt.width, state->req_fmt.height);

	return 0;
}

static int s5k6aafx_g_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	int err = 0;

	FUNC_ENTR();

	return err;
}

static int s5k6aafx_s_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	int err = 0;

	FUNC_ENTR();

	return err;
}

#if defined (CONFIG_SAMSUNG_P1L) || defined (CONFIG_SAMSUNG_P1N)
//latin_cam VT CAM Antibanding
static int s5k6aafx_set_60hz_antibanding(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct s5k6aafx_state *state = to_state(sd);
	int err = -EINVAL;

	FUNC_ENTR();

	unsigned long s5k6aafx_antibanding60hz[] = {
	0xFCFCD000,
	0x00287000,
	// Anti-Flicker //
	// End user init script
	0x002A0400,
	0x0F12005F,  //REG_TC_DBG_AutoAlgEnBits //Auto Anti-Flicker is enabled bit[5] = 1.
	0x002A03DC,
	0x0F120002,  //02 REG_SF_USER_FlickerQuant //Set flicker quantization(0: no AFC, 1: 50Hz, 2: 60 Hz)
	0x0F120001,
	};

	err = s5k6aafx_write_regs(sd, s5k6aafx_antibanding60hz,
				       	sizeof(s5k6aafx_antibanding60hz) / sizeof(s5k6aafx_antibanding60hz[0]));
	printk("%s:  setting 60hz antibanding \n", __func__);
	if (unlikely(err))
	{
		printk("%s: failed to set 60hz antibanding \n", __func__);
		return err;
	}

	return 0;
}
//hmin84.park - 100706
#endif

static int s5k6aafx_init(struct v4l2_subdev *sd, u32 val)
{
	//struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct s5k6aafx_state *state = to_state(sd);

	int err = -EINVAL;

	FUNC_ENTR();

#ifdef CONFIG_LOAD_FILE
	err = loadFile();
	if (unlikely(err)) {
		printk("%s: failed to init\n", __func__);
		return err;
	}
#endif

	/* set initial regster value */
	if (!state->vt_mode)
	{
		printk("%s: load camera common setting \n", __func__);
#ifdef CONFIG_LOAD_FILE
		err = s5k6aafx_write_regs_from_sd(sd, "s5k6aafx_common");
#else
		err = s5k6aafx_write_regs(sd, s5k6aafx_common,	sizeof(s5k6aafx_common) / sizeof(s5k6aafx_common[0]));
#endif
	}
	else
	{
		printk("%s: load camera VT call setting \n", __func__);
#ifdef CONFIG_LOAD_FILE
		err = s5k6aafx_write_regs_from_sd(sd, "s5k6aafx_vt_common");
#else
		err = s5k6aafx_write_regs(sd, s5k6aafx_vt_common, sizeof(s5k6aafx_vt_common) / sizeof(s5k6aafx_vt_common[0]));
#endif

	}

	if (unlikely(err))
	{
		printk("%s: failed to init\n", __func__);
		return err;
	}

#if defined (CONFIG_SAMSUNG_P1L) || defined (CONFIG_SAMSUNG_P1N)
	//latin_cam VT Cam Antibanding
	if (state->anti_banding == ANTI_BANDING_60HZ)
	{
		err = s5k6aafx_set_60hz_antibanding(sd);
		if (unlikely(err))
		{
			printk("%s: failed to s5k6aafx_set_60hz_antibanding \n", __func__);
			return err;
		}
	}
	//hmin84.park -10.07.06
#endif

	state->set_fmt.width = DEFAULT_WIDTH;
	state->set_fmt.height = DEFAULT_HEIGHT;

	return 0;
}

/*
 * s_config subdev ops
 * With camera device, we need to re-initialize
 * every single opening time therefor,
 * it is not necessary to be initialized on probe time.
 * except for version checking
 * NOTE: version checking is optional
 */
static int s5k6aafx_s_config(struct v4l2_subdev *sd,
		int irq, void *platform_data)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct s5k6aafx_state *state = to_state(sd);
	struct s5k6aafx_platform_data *pdata;

	FUNC_ENTR();

	pdata = client->dev.platform_data;

	if (!pdata) {
		dev_err(&client->dev, "%s: no platform data\n", __func__);
		return -ENODEV;
	}

	/*
	 * Assign default format and resolution
	 * Use configured default information in platform data
	 * or without them, use default information in driver
	 */
	if (!(pdata->default_width && pdata->default_height)) {
		state->req_fmt.width = DEFAULT_WIDTH;
		state->req_fmt.height = DEFAULT_HEIGHT;
	} else {
		state->req_fmt.width = pdata->default_width;
		state->req_fmt.height = pdata->default_height;

		printk("%s : width - %d , height - %d\n", __func__, state->req_fmt.width, state->req_fmt.height);
	}

	if (!pdata->pixelformat) {
		state->req_fmt.pixelformat = DEFAULT_FMT;
	} else {
		state->req_fmt.pixelformat = pdata->pixelformat;
	}

	return 0;
}

#if 0
static int s5k6aafx_queryctrl(struct v4l2_subdev *sd, struct v4l2_queryctrl *qc)
{
	FUNC_ENTR();
	return 0;
}

static int s5k6aafx_querymenu(struct v4l2_subdev *sd, struct v4l2_querymenu *qm)
{
	FUNC_ENTR();
	return 0;
}
#endif

static int s5k6aafx_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct s5k6aafx_state *state = to_state(sd);

	FUNC_ENTR();

	int err;
	if (!enable)
		return 0;

	if (state->req_fmt.colorspace != V4L2_COLORSPACE_JPEG) {
		err = s5k6aafx_set_preview_start(sd);
		printk("s5k6aafx_set_preview_start~~~~~ \n");
		if (err < 0) {
			printk("faild to start preview\n");
			return err;
		}
	}
	else {
		err = s5k6aafx_set_capture_start(sd);
		printk("s5k6aafx_set_capture_start~~~~~ \n");
		if (err < 0) {
			printk("faild to start capture\n");
			return err;
		}
	}
	return 0;
}

static int s5k6aafx_get_shutterspeed(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int err = -1;
	unsigned short svalue = 0;
	static char str_svalue[8];

	FUNC_ENTR();

	s5k6aafx_write(client, 0x002C7000);
	s5k6aafx_write(client, 0x002E1508);
	err = s5k6aafx_read(client, 0x0F12, &svalue); //read shutter speed value

	sprintf(str_svalue, "%d", svalue);
	//printk("%s : shutter speed is (%ld)(%d)\n", __func__, simple_strtol( str_svalue, NULL, 10),ctrl->value);
	ctrl->value = (simple_strtol( str_svalue, NULL, 10) / 500);

	return err;
}

static int s5k6aafx_get_iso(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int err = -1;
	unsigned short ivalue = 0;
	static char str_ivalue[8];
	unsigned int iso_table[4] = {50, 100, 200, 400};

	FUNC_ENTR();

	s5k6aafx_write(client, 0x002C7000);
	s5k6aafx_write(client, 0x002E1508);
	err = s5k6aafx_read(client, 0x0F12, &ivalue); //read iso value

	sprintf(str_ivalue, "%d", ivalue);
	ivalue = (simple_strtol( str_ivalue, NULL, 10) / 256);
	if(ivalue <= 50) {
		ctrl->value = iso_table[0];
	} else if(ivalue > 50 && ivalue <= 100){
		ctrl->value = iso_table[1];
	} else if(ivalue > 100 && ivalue <= 200){
		ctrl->value = iso_table[2];
	} else if(ivalue > 200){
		ctrl->value = iso_table[3];
	}
	//printk("%s : iso value is (%ld)(%d)(%d)\n", __func__, simple_strtol( str_ivalue, NULL, 10),ivalue, ctrl->value);
	return err;
}

static int s5k6aafx_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int err = -EINVAL;

	FUNC_ENTR();

	//printk("ctrl->id : %d \n", ctrl->id - V4L2_CID_PRIVATE_BASE);

	switch (ctrl->id)
	{
		case V4L2_CID_CAMERA_GET_SHT_TIME:
			err = s5k6aafx_get_shutterspeed(sd, ctrl);
			break;

		case V4L2_CID_CAMERA_GET_ISO:
			err = s5k6aafx_get_iso(sd, ctrl); 
			break;

		case V4L2_CID_EXPOSURE:
			err = 0;	//Not support
			break;

		case V4L2_CID_AUTO_WHITE_BALANCE:
			err =0;	//Not support
			break;

		case V4L2_CID_WHITE_BALANCE_PRESET:
			err = 0;	//Not support
			break;

		case V4L2_CID_COLORFX:
			err = 0;	//Not support
			break;

		case V4L2_CID_CONTRAST:
			err = 0;	//Not support
			break;

		case V4L2_CID_SATURATION:
			err = 0;	//Not support
			break;

		case V4L2_CID_SHARPNESS:
			err = 0;	//Not support
			break;

		case V4L2_CID_CAM_JPEG_MAIN_SIZE:
			err = 0;	//Not support
			break;

		case V4L2_CID_CAM_JPEG_MAIN_OFFSET:
			err = 0;	//Not support
			break;

		case V4L2_CID_CAM_JPEG_THUMB_SIZE:
			err = 0;	//Not support
			break;

		case V4L2_CID_CAM_JPEG_THUMB_OFFSET:
			err = 0;	//Not support
			break;

		case V4L2_CID_CAM_JPEG_POSTVIEW_OFFSET:
			err = 0;	//Not support
			break; 

		case V4L2_CID_CAM_JPEG_MEMSIZE:
			err = 0;	//Not support
			break;

		case V4L2_CID_CAM_JPEG_QUALITY:
			err = 0;	//Not support
			break;

		case V4L2_CID_CAMERA_OBJ_TRACKING_STATUS:
			err = 0;	//Not support
			break;

		case V4L2_CID_CAMERA_SMART_AUTO_STATUS:
			err = 0;	//Not support
			break;

		case V4L2_CID_CAMERA_AUTO_FOCUS_RESULT:
			err = 0;	//Not support
			break;

		case V4L2_CID_CAM_DATE_INFO_YEAR:
			err = 0;	//Not support
			break; 

		case V4L2_CID_CAM_DATE_INFO_MONTH:
			err = 0;	//Not support
			break; 

		case V4L2_CID_CAM_DATE_INFO_DATE:
			err = 0;	//Not support
			break; 

		case V4L2_CID_CAM_SENSOR_VER:
			err = 0;	//Not support
			break; 

		case V4L2_CID_CAM_FW_MINOR_VER:
			err = 0;	//Not support
			break; 

		case V4L2_CID_CAM_FW_MAJOR_VER:
			err = 0;	//Not support
			break; 

		case V4L2_CID_CAM_PRM_MINOR_VER:
			err = 0;	//Not support
			break; 

		case V4L2_CID_CAM_PRM_MAJOR_VER:
			err = 0;	//Not support
			break; 

		case V4L2_CID_CAMERA_GET_FLASH_ONOFF:
			err = 0;	//Not support
			break;

		case V4L2_CID_ESD_INT:
			err = 0;	//Not support
			break;

		case V4L2_CID_CAM_SENSOR_TYPE:
			err = 0;	//Not support
			break;

		default:
			dev_err(&client->dev, "%s: no such control\n", __func__);
			break;
	}

	if (err < 0)
	{
		dev_err(&client->dev, "%s : i2c_write for get shutter speed\n", __func__);
		return -EIO;
	}

	return 0;
}

static int s5k6aafx_set_brightness(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int err = -EINVAL;

	FUNC_ENTR();

#ifdef CONFIG_LOAD_FILE
	switch (ctrl->value) {
		case EV_MINUS_4:
			err = s5k6aafx_write_regs_from_sd(sd, "s5k6aafx_bright_m4");
			break;
		case EV_MINUS_3:
			err = s5k6aafx_write_regs_from_sd(sd, "s5k6aafx_bright_m3");
			break;
		case EV_MINUS_2:
			err = s5k6aafx_write_regs_from_sd(sd, "s5k6aafx_bright_m2");
			break;
		case EV_MINUS_1:
			err = s5k6aafx_write_regs_from_sd(sd, "s5k6aafx_bright_m1");
			break;
		case EV_DEFAULT:
			err = s5k6aafx_write_regs_from_sd(sd, "s5k6aafx_bright_default");
			break;
		case EV_PLUS_1:
			err = s5k6aafx_write_regs_from_sd(sd, "s5k6aafx_bright_p1");
			break;
		case EV_PLUS_2:
			err = s5k6aafx_write_regs_from_sd(sd, "s5k6aafx_bright_p2");
			break;
		case EV_PLUS_3:
			err = s5k6aafx_write_regs_from_sd(sd, "s5k6aafx_bright_p3");
			break;
		case EV_PLUS_4:
			err = s5k6aafx_write_regs_from_sd(sd, "s5k6aafx_bright_p4");
			break;
		default:
			dev_err(&client->dev, "%s : there's no brightness value with [%d]\n", __func__,ctrl->value);
			return err;
			break;
	}
#else
	switch (ctrl->value) {
		case EV_MINUS_4:
			err = s5k6aafx_write_regs(sd, s5k6aafx_bright_m4, \
				sizeof(s5k6aafx_bright_m4) / sizeof(s5k6aafx_bright_m4[0]));
			break;
		case EV_MINUS_3:
			err = s5k6aafx_write_regs(sd, s5k6aafx_bright_m3, \
				sizeof(s5k6aafx_bright_m3) / sizeof(s5k6aafx_bright_m3[0]));

			break;
		case EV_MINUS_2:
			err = s5k6aafx_write_regs(sd, s5k6aafx_bright_m2, \
				sizeof(s5k6aafx_bright_m2) / sizeof(s5k6aafx_bright_m2[0]));
			break;
		case EV_MINUS_1:
			err = s5k6aafx_write_regs(sd, s5k6aafx_bright_m1, \
				sizeof(s5k6aafx_bright_m1) / sizeof(s5k6aafx_bright_m1[0]));
			break;
		case EV_DEFAULT:
			err = s5k6aafx_write_regs(sd, s5k6aafx_bright_default, \
				sizeof(s5k6aafx_bright_default) / sizeof(s5k6aafx_bright_default[0]));
			break;
		case EV_PLUS_1:
			err = s5k6aafx_write_regs(sd, s5k6aafx_bright_p1, \
				sizeof(s5k6aafx_bright_p1) / sizeof(s5k6aafx_bright_p1[0]));
			break;
		case EV_PLUS_2:
			err = s5k6aafx_write_regs(sd, s5k6aafx_bright_p2, \
				sizeof(s5k6aafx_bright_p2) / sizeof(s5k6aafx_bright_p2[0]));
			break;
		case EV_PLUS_3:
			err = s5k6aafx_write_regs(sd, s5k6aafx_bright_p3, \
				sizeof(s5k6aafx_bright_p3) / sizeof(s5k6aafx_bright_p3[0]));
			break;
		case EV_PLUS_4:
			err = s5k6aafx_write_regs(sd, s5k6aafx_bright_p4, \
				sizeof(s5k6aafx_bright_p4) / sizeof(s5k6aafx_bright_p4[0]));
			break;
		default:
			dev_err(&client->dev, "%s : there's no brightness value with [%d]\n", __func__,ctrl->value);
			return err;
			break;
	}
#endif

	if (err < 0)
	{
		dev_err(&client->dev, "%s : i2c_write for set brightness\n", __func__);
		return -EIO;
	}

	return err;
}

static int s5k6aafx_set_white_balance(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	int err = -EINVAL;

	FUNC_ENTR();

	switch(ctrl->value) {
		case WHITE_BALANCE_AUTO:
			err = s5k6aafx_write_regs(sd, s5k6aafx_wb_auto, \
				sizeof(s5k6aafx_wb_auto) / sizeof(s5k6aafx_wb_auto[0]));
			break;

		case WHITE_BALANCE_SUNNY:
			err = s5k6aafx_write_regs(sd, s5k6aafx_wb_daylight, \
				sizeof(s5k6aafx_wb_daylight) / sizeof(s5k6aafx_wb_daylight[0]));
			break;

		case WHITE_BALANCE_CLOUDY_DAYLIGHT:
			err = s5k6aafx_write_regs(sd, s5k6aafx_wb_cloudy, \
				sizeof(s5k6aafx_wb_cloudy) / sizeof(s5k6aafx_wb_cloudy[0]));
			break;

		case WHITE_BALANCE_TUNGSTEN:
			err = s5k6aafx_write_regs(sd, s5k6aafx_wb_incandescent, \
				sizeof(s5k6aafx_wb_incandescent) / sizeof(s5k6aafx_wb_incandescent[0]));
			break;

		case WHITE_BALANCE_FLUORESCENT:
			err = s5k6aafx_write_regs(sd, s5k6aafx_wb_fluorescent, \
				sizeof(s5k6aafx_wb_fluorescent) / sizeof(s5k6aafx_wb_fluorescent[0]));
			break;

		default:
			printk(KERN_ERR "%s: unsupported wb(%d) value.\n", __func__, ctrl->value);
			break;
	}

	if (err < 0) {
		dev_err(&client->dev, "%s : i2c_write for set WB\n", __func__);
		return -EIO;
	}

	return err;
}


static int s5k6aafx_set_effect(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	int err = -EINVAL;

	FUNC_ENTR();

	switch(ctrl->value) {
		case IMAGE_EFFECT_NONE:
		case IMAGE_EFFECT_AQUA:
		case IMAGE_EFFECT_ANTIQUE:
		case IMAGE_EFFECT_SHARPEN:
			err = s5k6aafx_write_regs(sd, s5k6aafx_effect_none, \
				sizeof(s5k6aafx_effect_none) / sizeof(s5k6aafx_effect_none[0]));
			break;

		case IMAGE_EFFECT_BNW:
			err = s5k6aafx_write_regs(sd, s5k6aafx_effect_gray, \
				sizeof(s5k6aafx_effect_gray) / sizeof(s5k6aafx_effect_gray[0]));
			break;

		case IMAGE_EFFECT_SEPIA:
			err = s5k6aafx_write_regs(sd, s5k6aafx_effect_sepia, \
				sizeof(s5k6aafx_effect_sepia) / sizeof(s5k6aafx_effect_sepia[0]));
			break;

		case IMAGE_EFFECT_NEGATIVE:
			err = s5k6aafx_write_regs(sd, s5k6aafx_effect_negative, \
				sizeof(s5k6aafx_effect_negative) / sizeof(s5k6aafx_effect_negative[0]));
			break;

		default:
			printk(KERN_ERR "%s: unsupported effect(%d) value.\n", __func__, ctrl->value);
			break;
	}

	if (err < 0)
	{
		dev_err(&client->dev, "%s : i2c_write for set effect\n", __func__);
		return -EIO;
	}

	return err;
}

static int s5k6aafx_set_blur(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	int err = -EINVAL;

	FUNC_ENTR();

#ifdef CONFIG_LOAD_FILE
	switch (ctrl->value) {
		case BLUR_LEVEL_0:
			err = s5k6aafx_write_regs_from_sd(sd, "s5k6aafx_vt_pretty_default");
			break;
		case BLUR_LEVEL_1:
			err = s5k6aafx_write_regs_from_sd(sd, "s5k6aafx_vt_pretty_1");
			break;
		case BLUR_LEVEL_2:
			err = s5k6aafx_write_regs_from_sd(sd, "s5k6aafx_vt_pretty_2");
			break;
		case BLUR_LEVEL_3:
		case BLUR_LEVEL_MAX:
			err = s5k6aafx_write_regs_from_sd(sd, "s5k6aafx_vt_pretty_3");
			break;
		default:
			dev_err(&client->dev, "%s : there's no blur value with [%d]\n", __func__,ctrl->value);
			return err;
			break;
	}
#else
	switch (ctrl->value) {
		case BLUR_LEVEL_0:
			err = s5k6aafx_write_regs(sd, s5k6aafx_vt_pretty_default, \
				sizeof(s5k6aafx_vt_pretty_default) / sizeof(s5k6aafx_vt_pretty_default[0]));
			break;
		case BLUR_LEVEL_1:
			err = s5k6aafx_write_regs(sd, s5k6aafx_vt_pretty_1, \
				sizeof(s5k6aafx_vt_pretty_1) / sizeof(s5k6aafx_vt_pretty_1[0]));
			break;
		case BLUR_LEVEL_2:
			err = s5k6aafx_write_regs(sd, s5k6aafx_vt_pretty_2, \
				sizeof(s5k6aafx_vt_pretty_2) / sizeof(s5k6aafx_vt_pretty_2[0]));
			break;
		case BLUR_LEVEL_3:
		case BLUR_LEVEL_MAX:
			err = s5k6aafx_write_regs(sd, s5k6aafx_vt_pretty_3, \
				sizeof(s5k6aafx_vt_pretty_3) / sizeof(s5k6aafx_vt_pretty_3[0]));
			break;
		default:
			dev_err(&client->dev, "%s : there's no blur value with [%d]\n", __func__,ctrl->value);
			return err;
			break;
	}
#endif

	if (err < 0)
	{
		dev_err(&client->dev, "%s : i2c_write for set blur\n", __func__);
		return -EIO;
	}

	return err;
}

static int s5k6aafx_check_dataline_stop(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct s5k6aafx_state *state = to_state(sd);
	int err = -EINVAL;

	extern int s5k6aafx_power_reset(void);

	printk( "pattern off setting~~~~~~~~~~~~~~\n");

	s5k6aafx_write(client, 0xFCFCD000);
	s5k6aafx_write(client, 0x0028D000);
	s5k6aafx_write(client, 0x002A3100);
    	s5k6aafx_write(client, 0x0F120000);

   //	err =  s5k6aafx_write_regs(sd, s5k6aafx_pattern_off,	sizeof(s5k6aafx_pattern_off) / sizeof(s5k6aafx_pattern_off[0]));
	printk("%s: sensor reset\n", __func__);

	s5k6aafx_power_reset();

	printk("%s: load camera init setting \n", __func__);
	err =  s5k6aafx_write_regs(sd, s5k6aafx_common,	sizeof(s5k6aafx_common) / sizeof(s5k6aafx_common[0]));
	state->check_dataline = 0;
//       mdelay(100);
	return err;
}

static int s5k6aafx_set_flip(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct s5k6aafx_state *state = to_state(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int err = -EINVAL;

	FUNC_ENTR();

	if(state->runmode != S5K6AAFX_RUNMODE_RUNNING) {
		printk(KERN_ERR "func(%s):line(%d) NOT RUNNING\n",__func__,__LINE__);
		return 0;
	}

	if(state->set_vhflip == 1) {
		err = s5k6aafx_write_regs(sd, s5k6aafx_vhflip_on,
					sizeof(s5k6aafx_vhflip_on) / sizeof(s5k6aafx_vhflip_on[0]));
	} else {
		err = s5k6aafx_write_regs(sd, s5k6aafx_vhflip_off,
					sizeof(s5k6aafx_vhflip_off) / sizeof(s5k6aafx_vhflip_off[0]));
	}

	if (err < 0) {
		dev_err(&client->dev, "%s : i2c_write for checkdata\n", __func__);
		return -EIO;
	}

	return err;
}

static int s5k6aafx_set_frame_rate(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int err = -EINVAL;

	FUNC_ENTR();
	switch(ctrl->value) {
		case 15:
			err = s5k6aafx_write_regs(sd, s5k6aafx_vt_15fps,
					sizeof(s5k6aafx_vt_15fps) / sizeof(s5k6aafx_vt_15fps[0]));
			break;

		default:
			printk("%s: no such %d framerate.\nDefault framerate is 15fps.\n", __func__, ctrl->value);
			err = 0;
			break;
	}

	if (err < 0) {
		dev_err(&client->dev, "%s : i2c_write for frame rate\n", __func__);
		return -EIO;
	}

	return err;
}


static int s5k6aafx_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct s5k6aafx_state *state = to_state(sd);

	int err = 0;

	FUNC_ENTR();

	printk("ctrl->id : %d \n", ctrl->id - V4L2_CID_PRIVATE_BASE);
	if(state->check_dataline) {
		if( ( ctrl->id != V4L2_CID_CAM_PREVIEW_ONOFF ) &&
			( ctrl->id != V4L2_CID_CAMERA_CHECK_DATALINE_STOP ) &&
			( ctrl->id != V4L2_CID_CAMERA_CHECK_DATALINE ) ) {
				return 0;
			}
	}

	switch (ctrl->id) {
		case V4L2_CID_CAM_PREVIEW_ONOFF:
			if (ctrl->value) {
				err = s5k6aafx_set_preview_start(sd);
			} else {
				err = s5k6aafx_set_preview_stop(sd);
			}
			printk("V4L2_CID_CAM_PREVIEW_ONOFF [%d] \n", ctrl->value);
			break;

		case V4L2_CID_CAM_CAPTURE:
			err = s5k6aafx_set_capture_start(sd);
			printk("V4L2_CID_CAM_CAPTURE [%d] \n", ctrl->value);
			break;
			//add capture mode and separate preview mode

		case V4L2_CID_CAMERA_VT_MODE:
			state->vt_mode = ctrl->value;
			err = 0;
			printk("V4L2_CID_CAMERA_VT_MODE [%d] \n", ctrl->value);
			break;
			//add vt mode for read vt settings

		case V4L2_CID_CAMERA_BRIGHTNESS:
			err = s5k6aafx_set_brightness(sd, ctrl);
			printk("V4L2_CID_CAMERA_BRIGHTNESS [%d] \n", ctrl->value);
			break;

		case V4L2_CID_CAMERA_VGA_BLUR:
			err = s5k6aafx_set_blur(sd, ctrl);
			printk("V4L2_CID_CAMERA_VGA_BLUR [%d] \n", ctrl->value);
			break;
			//CID_CAMERA_VGA_BLUR

#if defined (CONFIG_SAMSUNG_P1L) || defined (CONFIG_SAMSUNG_P1N)
		//latin_cam VT Camera Antibanding
		case V4L2_CID_CAMERA_ANTI_BANDING:
			state->anti_banding = ctrl->value;
			printk("V4L2_CID_CAMERA_ANTI_BANDING [%d],[%d]\n",state->anti_banding,ctrl->value);
			err = 0;
			break;
		//hmin84.park - 10.07.06
#endif

		case V4L2_CID_CAMERA_CHECK_DATALINE:
			state->check_dataline = ctrl->value;
			err = 0;
			break;

		case V4L2_CID_CAMERA_CHECK_DATALINE_STOP:
			err = s5k6aafx_check_dataline_stop(sd);
			break;

		case V4L2_CID_CAMERA_FRAME_RATE:
			err = s5k6aafx_set_frame_rate(sd, ctrl);
			state->fps = ctrl->value;
			break;

		case V4L2_CID_CAMERA_APP_CHECK:
			err = 0;
			break;

		case V4L2_CID_CAM_JPEG_QUALITY:
			err = 0;
			break;

		case V4L2_CID_CAMERA_SENSOR_MODE:
			err = 0;
			break;

		case V4L2_CID_CAMERA_CHECK_FLIP:
			state->set_vhflip = ctrl->value;
			err = s5k6aafx_set_flip(sd, ctrl);
			break;

		case V4L2_CID_CAMERA_WHITE_BALANCE:
			err = s5k6aafx_set_white_balance(sd, ctrl);
			break;

		case V4L2_CID_CAMERA_EFFECT:
			err = s5k6aafx_set_effect(sd, ctrl);
			break;

		default:
			dev_err(&client->dev, "%s: no such control\n", __func__);
			break;
	}

	return err;
}

static const struct v4l2_subdev_core_ops s5k6aafx_core_ops = {
	.init = s5k6aafx_init,		/* initializing API */
	.s_config = s5k6aafx_s_config,	/* Fetch platform data */
#if 0
	.queryctrl = s5k6aafx_queryctrl,
	.querymenu = s5k6aafx_querymenu,
#endif
	.g_ctrl = s5k6aafx_g_ctrl,
	.s_ctrl = s5k6aafx_s_ctrl,
};

static const struct v4l2_subdev_video_ops s5k6aafx_video_ops = {
	/*.s_crystal_freq = s5k6aafx_s_crystal_freq,*/
	.g_fmt	= s5k6aafx_g_fmt,
	.s_fmt	= s5k6aafx_s_fmt,
	.s_stream = s5k6aafx_s_stream,
	.enum_framesizes = s5k6aafx_enum_framesizes,
	/*.enum_frameintervals = s5k6aafx_enum_frameintervals,*/
	/*.enum_fmt = s5k6aafx_enum_fmt,*/
	.try_fmt = s5k6aafx_try_fmt,
	.g_parm	= s5k6aafx_g_parm,
	.s_parm	= s5k6aafx_s_parm,
};

static const struct v4l2_subdev_ops s5k6aafx_ops = {
	.core = &s5k6aafx_core_ops,
	.video = &s5k6aafx_video_ops,
};

/*
 * s5k6aafx_probe
 * Fetching platform data is being done with s_config subdev call.
 * In probe routine, we just register subdev device
 */
static int s5k6aafx_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct s5k6aafx_state *state;
	struct v4l2_subdev *sd;

	FUNC_ENTR();

	state = kzalloc(sizeof(struct s5k6aafx_state), GFP_KERNEL);
	if (state == NULL)
		return -ENOMEM;

	state->runmode = S5K6AAFX_RUNMODE_NOTREADY;
	state->set_vhflip = 0;

	sd = &state->sd;
	strcpy(sd->name, S5K6AAFX_DRIVER_NAME);

	/* Registering subdev */
	v4l2_i2c_subdev_init(sd, client, &s5k6aafx_ops);

	dev_info(&client->dev, "s5k6aafx has been probed\n");

	return 0;
}

static int s5k6aafx_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);

	FUNC_ENTR();

	v4l2_device_unregister_subdev(sd);
	kfree(to_state(sd));

	return 0;
}

static const struct i2c_device_id s5k6aafx_id[] = {
	{ S5K6AAFX_DRIVER_NAME, 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, s5k6aafx_id);

static struct v4l2_i2c_driver_data v4l2_i2c_data = {
	.name = S5K6AAFX_DRIVER_NAME,
	.probe = s5k6aafx_probe,
	.remove = s5k6aafx_remove,
	.id_table = s5k6aafx_id,
};

MODULE_DESCRIPTION("S5K6AAFX ISP driver");
MODULE_AUTHOR("Heungjun Kim<riverful.kim@samsung.com>");
MODULE_LICENSE("GPL");

