/*
 * linux/radeon_i2c.h
 *
 * Original author probably Benjamin Herrenschmidt <benh@kernel.crashing.org>
 * or Kronos <kronos@kronoz.cjb.net>
 * Based on Xfree sources
 * (C) Copyright 2004 Jon Smirl <jonsmirl@gmail.com>
 *
 * This is a GPL licensed file from the Linux kernel, don't add it to the BSD build
 *
 * Radeon I2C support routines
 *
 */

#include <linux/i2c.h>
#include <linux/i2c-id.h>
#include <linux/i2c-algo-bit.h>

struct radeon_i2c_chan {
	drm_device_t 			*dev;
	u32		 		ddc_reg;
	struct i2c_adapter		adapter;
	struct i2c_algo_bit_data	algo;
};

extern int radeon_create_i2c_busses(drm_device_t *dev);
extern void radeon_delete_i2c_busses(drm_device_t *dev);

 
