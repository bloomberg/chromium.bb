/*
 * linux/radeon_i2c.c
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

#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/pci.h>
#include <linux/fb.h>
#include <linux/version.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)

#include <asm/io.h>
#include <video/radeon.h>

#include "drmP.h"
#include "drm.h"
#include "radeon_drm.h"
#include "radeon_drv.h"

static void gpio_setscl(void *data, int state)
{
	struct radeon_i2c_chan *chan = data;
	drm_radeon_private_t *dev_priv = chan->dev->dev_private;
	u32 val;

	val = RADEON_READ(chan->ddc_reg) & ~(VGA_DDC_CLK_OUT_EN);
	if (!state)
		val |= VGA_DDC_CLK_OUT_EN;

	RADEON_WRITE(chan->ddc_reg, val);
	(void)RADEON_READ(chan->ddc_reg);
}

static void gpio_setsda(void *data, int state)
{
	struct radeon_i2c_chan *chan = data;
	drm_radeon_private_t *dev_priv = chan->dev->dev_private;
	u32 val;

	val = RADEON_READ(chan->ddc_reg) & ~(VGA_DDC_DATA_OUT_EN);
	if (!state)
		val |= VGA_DDC_DATA_OUT_EN;

	RADEON_WRITE(chan->ddc_reg, val);
	(void)RADEON_READ(chan->ddc_reg);
}

static int gpio_getscl(void *data)
{
	struct radeon_i2c_chan *chan = data;
	drm_radeon_private_t *dev_priv = chan->dev->dev_private;
	u32 val;

	val = RADEON_READ(chan->ddc_reg);

	return (val & VGA_DDC_CLK_INPUT) ? 1 : 0;
}

static int gpio_getsda(void *data)
{
	struct radeon_i2c_chan *chan = data;
	drm_radeon_private_t *dev_priv = chan->dev->dev_private;
	u32 val;

	val = RADEON_READ(chan->ddc_reg);

	return (val & VGA_DDC_DATA_INPUT) ? 1 : 0;
}

static int setup_i2c_bus(drm_device_t * dev, struct radeon_i2c_chan *chan, const char *name)
{
	int rc;

	chan->dev = dev;
	strcpy(chan->adapter.name, name);
	chan->adapter.owner		= THIS_MODULE;
	chan->adapter.id		= I2C_ALGO_ATI;
	chan->adapter.algo_data		= &chan->algo;
	chan->adapter.dev.parent	= &chan->dev->pdev->dev;
	chan->algo.setsda		= gpio_setsda;
	chan->algo.setscl		= gpio_setscl;
	chan->algo.getsda		= gpio_getsda;
	chan->algo.getscl		= gpio_getscl;
	chan->algo.udelay		= 40;
	chan->algo.timeout		= 20;
	chan->algo.data 		= chan;

	i2c_set_adapdata(&chan->adapter, chan);

	/* Raise SCL and SDA */
	gpio_setsda(chan, 1);
	gpio_setscl(chan, 1);
	udelay(20);

	if ((rc = i2c_bit_add_bus(&chan->adapter))) {
		i2c_set_adapdata(&chan->adapter, NULL);
		chan->dev = NULL;
		DRM_ERROR("Failed to register I2C bus %s.\n", name);
		return rc;
	}
	DRM_DEBUG("I2C bus %s registered.\n", name);
	return 0;
}

int radeon_create_i2c_busses(drm_device_t * dev)
{
	drm_radeon_private_t *dev_priv = dev->dev_private;

	dev_priv->i2c[0].ddc_reg = GPIO_MONID;
	/* Don't return the error from setup. It is not fatal */
	/* if the bus can not be initialized */
	setup_i2c_bus(dev, &dev_priv->i2c[0], "monid");

	dev_priv->i2c[1].ddc_reg = GPIO_DVI_DDC;
	setup_i2c_bus(dev, &dev_priv->i2c[1], "dvi");

	dev_priv->i2c[2].ddc_reg = GPIO_VGA_DDC;
	setup_i2c_bus(dev, &dev_priv->i2c[2], "vga");

	dev_priv->i2c[3].ddc_reg = GPIO_CRT2_DDC;
	setup_i2c_bus(dev, &dev_priv->i2c[3], "crt2");

	return 0;
}

void radeon_delete_i2c_busses(drm_device_t * dev)
{
	drm_radeon_private_t *dev_priv = dev->dev_private;
	int i, ret;

	for (i = 0; i < 4; i++) {
		if (dev_priv->i2c[i].dev) {
			ret = i2c_bit_del_bus(&dev_priv->i2c[i].adapter);
			dev_priv->i2c[i].dev = NULL;
		}
	}
}

#endif
