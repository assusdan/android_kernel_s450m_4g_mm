#include <linux/interrupt.h>
#include <mt_boot_common.h>
#include <linux/i2c.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/rtpm_prio.h>
#include <linux/wait.h>
#include <linux/delay.h>
#include <linux/gpio.h>

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>

//#include "flash_on.h"
#define TPD_PROXIMITY
#define FTS_GESTRUE
//#define TPD_CLOSE_POWER_IN_SLEEP

#include "tpd.h"
#ifdef TPD_PROXIMITY

#define APS_ERR(fmt,arg...)           	//printk("<<proximity>> "fmt"\n",##arg)

#define TPD_PROXIMITY_DEBUG(fmt,arg...) //printk("<<proximity>> "fmt"\n",##arg)

#define TPD_PROXIMITY_DMESG(fmt,arg...) //printk("<<proximity>> "fmt"\n",##arg)

static u8 tpd_proximity_flag 			= 0;

static u8 tpd_proximity_flag_one 		= 0; //add for tpd_proximity by wangdongfang

static u8 tpd_proximity_detect 		= 1;//0-->close ; 1--> far away

#endif
#ifdef FTS_GESTRUE
#include "ft5x0x_getsure.h"
#endif

#include "ft5x0x_util.h"

#ifdef TPD_PROXIMITY
int tpd_read_ps(void)
{
	tpd_proximity_detect;
	return 0;    
}

static int tpd_get_ps_value(void)
{
	return tpd_proximity_detect;
}

static int tpd_enable_ps(int enable)
{
	u8 state;
	int ret = -1;
	
	i2c_smbus_read_i2c_block_data(i2c_client, 0xB0, 1, &state);
	//printk("[proxi_fts]read: 999 0xb0's value is 0x%02X\n", state);

	if (enable){
		state |= 0x01;
		tpd_proximity_flag = 1;
		TPD_PROXIMITY_DEBUG("[proxi_fts]ps function is on\n");	
	}else{
		state &= 0x00;	
		tpd_proximity_flag = 0;
		TPD_PROXIMITY_DEBUG("[proxi_fts]ps function is off\n");
	}
	
	ret = i2c_smbus_write_i2c_block_data(i2c_client, 0xB0, 1, &state);
	TPD_PROXIMITY_DEBUG("[proxi_fts]write: 0xB0's value is 0x%02X\n", state);
	return 0;
}

int tpd_ps_operate(void* self, uint32_t command, void* buff_in, int size_in,

		void* buff_out, int size_out, int* actualout)
{
	int err = 0;
	int value;
	hwm_sensor_data *sensor_data;
	TPD_DEBUG("[proxi_fts]command = 0x%02X\n", command);		
	
	switch (command)
	{
		case SENSOR_DELAY:
			if((buff_in == NULL) || (size_in < sizeof(int)))
			{
				APS_ERR("Set delay parameter error!\n");
				err = -EINVAL;
			}
			// Do nothing
			break;
		case SENSOR_ENABLE:
			if((buff_in == NULL) || (size_in < sizeof(int)))
			{
				APS_ERR("Enable sensor parameter error!\n");
				err = -EINVAL;
			}
			else
			{				
				value = *(int *)buff_in;
				if(value)
				{		
					if((tpd_enable_ps(1) != 0))
					{
						APS_ERR("enable ps fail: %d\n", err); 
						return -1;
					}
				}
				else
				{
					if((tpd_enable_ps(0) != 0))
					{
						APS_ERR("disable ps fail: %d\n", err); 
						return -1;
					}
				}
			}
			break;
		case SENSOR_GET_DATA:
			if((buff_out == NULL) || (size_out< sizeof(hwm_sensor_data)))
			{
				APS_ERR("get sensor data parameter error!\n");
				err = -EINVAL;
			}
			else
			{
				sensor_data = (hwm_sensor_data *)buff_out;				
				if((err = tpd_read_ps()))
				{
					err = -1;;
				}
				else
				{
					sensor_data->values[0] = tpd_get_ps_value();
					TPD_PROXIMITY_DEBUG("huang sensor_data->values[0] 1082 = %d\n", sensor_data->values[0]);
					sensor_data->value_divide = 1;
					sensor_data->status = SENSOR_STATUS_ACCURACY_MEDIUM;
				}					
			}
			break;
		default:
			APS_ERR("proxmy sensor operate function no this parameter %d!\n", command);
			err = -1;
			break;
	}
	return err;	
}
#endif

extern struct tpd_device *tpd;
extern int tpd_v_magnify_x;
extern int tpd_v_magnify_y;

struct i2c_client *i2c_client = NULL;

static int tpd_detect (struct i2c_client *client, struct i2c_board_info *info)
{
	strcpy(info->type, TPD_DEVICE);
	return 0;
}
#define TPD_MAX_RESET_COUNT	3
static int tpd_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	char data;
	int reset_count;
	int retval = 0;
	i2c_client = client;

	TPD_DMESG("mtk_tpd: tpd_probe ft5x0x \n");
	for (reset_count = 0; reset_count < TPD_MAX_RESET_COUNT; ++reset_count)
	{
		ft5x0x_set_rst(false, 5);
		ft5x0x_power(true);
		ft5x0x_set_rst(true, 20);

		if(i2c_smbus_read_i2c_block_data(client, 0x00, 1, &data) >= 0)
		{
			tpd_irq_registration();
			// Extern variable MTK touch driver
		    tpd_load_status = 1;

			TPD_DMESG("Touch Panel Device Probe %s\n", (retval < 0) ? "FAIL" : "PASS");
			return 0;
		}
		TPD_DMESG("I2C transfer error, line: %d\n", __LINE__);
	};

	return -1;
}

static int tpd_remove(struct i2c_client *client)
{
	TPD_DEBUG("TPD removed\n");
	return 0;
}

static const struct i2c_device_id ft5x0x_tpd_id[] = {{"ft5x0x",0},{}};
static const struct of_device_id ft5x0x_dt_match[] = {
	{.compatible = "mediatek,cap_touch"},
	{},
};

MODULE_DEVICE_TABLE(of, ft5x0x_dt_match);

static struct i2c_driver tpd_i2c_driver = {
	.driver = {
		.of_match_table = ft5x0x_dt_match,
		.name = "ft5x0x",
	},
	.probe = tpd_probe,
	.remove = tpd_remove,
	.id_table = ft5x0x_tpd_id,
	.detect = tpd_detect,
};
/********************************************/
static int tpd_local_init(void)
{
#ifdef FTS_GESTRUE
	fts_i2c_Init();
#endif
//	TPD_DMESG("Focaltech FT5x0x I2C Touchscreen Driver (Built %s @ %s)\n", __DATE__, __TIME__);
	if(i2c_add_driver(&tpd_i2c_driver)!=0)
	{
		TPD_DMESG("unable to add i2c driver.\n");
		return -1;
	}

	tpd_button_setting(tpd_dts_data.tpd_key_num,
			tpd_dts_data.tpd_key_local,
			tpd_dts_data.tpd_key_dim_local);

	if (tpd_dts_data.touch_filter.enable)
	{
		tpd_v_magnify_x = tpd_dts_data.touch_filter.VECLOCITY_THRESHOLD[0];
		tpd_v_magnify_y = tpd_dts_data.touch_filter.VECLOCITY_THRESHOLD[1];
	}


	TPD_DMESG("end %s, %d\n", __FUNCTION__, __LINE__);
	tpd_type_cap = 1;

	return 0;
}

static void tpd_resume(struct device *h)
{
	static char data;
#ifdef FTS_GESTRUE
	if (tpd_getsure_resume(i2c_client)) return;
#endif

#ifdef TPD_CLOSE_POWER_IN_SLEEP
   	TPD_DMESG("TPD wake up\n");
	ft5x0x_set_rst(false, 5);
	ft5x0x_power(true);
	ft5x0x_set_rst(true, 20);
#else
//	data = 0x00;
//	i2c_smbus_write_i2c_block_data(i2c_client, 0xd0, 1, &data);
#endif

//	tpd_up(0,0);
//	input_sync(tpd->dev);
	TPD_DMESG("TPD wake up done\n");
}

static void tpd_suspend(struct device *h)
{
	static char data;

#ifdef FTS_GESTRUE
	if (tpd_getsure_suspend(i2c_client)) return;
#endif

	TPD_DEBUG("TPD enter sleep\n");
	data = 0x3;
	i2c_smbus_write_i2c_block_data(i2c_client, 0xA5, 1, &data);  //TP enter sleep mode

#ifdef TPD_CLOSE_POWER_IN_SLEEP
	ft5x0x_power(false);
#endif
}


static struct device_attribute *ft5x0x_attrs[] = {

#ifdef FTS_GESTRUE
	 &dev_attr_tpgesture,
	 &dev_attr_tpgesture_status,
#endif
};

static struct tpd_driver_t tpd_device_driver = {
	.tpd_device_name = "FT5x0x",
	.tpd_local_init = tpd_local_init,
	.suspend = tpd_suspend,
	.resume = tpd_resume,
	.attrs = {
		.attr = ft5x0x_attrs,
		.num  = ARRAY_SIZE(ft5x0x_attrs),
	},
};

/* called when loaded into kernel */
static int __init tpd_driver_init(void)
{
	printk("MediaTek FT5x0x touch panel driver init\n");
	tpd_get_dts_info();
	if(tpd_driver_add(&tpd_device_driver) < 0)
		TPD_DMESG("add FT5x0x driver failed\n");

	return 0;
}

/* should never be called */
static void __exit tpd_driver_exit(void)
{
	TPD_DMESG("MediaTek FT5x0x touch panel driver exit\n");
       	tpd_driver_remove(&tpd_device_driver);
}

module_init(tpd_driver_init);
module_exit(tpd_driver_exit);

