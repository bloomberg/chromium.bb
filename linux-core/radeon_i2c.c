/*
 * Copyright 2007-8 Advanced Micro Devices, Inc.
 * Copyright 2008 Red Hat Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors: Dave Airlie
 *          Alex Deucher
 */
#include "drmP.h"
#include "radeon_drm.h"
#include "radeon_drv.h"


static int get_clock(void *i2c_priv)
{
	struct radeon_i2c_chan *i2c = i2c_priv;
	struct drm_radeon_private *dev_priv = i2c->dev->dev_private;
	struct radeon_i2c_bus_rec *rec = &i2c->rec;
	uint32_t val;

	val = RADEON_READ(rec->get_clk_reg);
	val &= rec->get_clk_mask;

	return (val != 0);
}


static int get_data(void *i2c_priv)
{
	struct radeon_i2c_chan *i2c = i2c_priv;
	struct drm_radeon_private *dev_priv = i2c->dev->dev_private;
	struct radeon_i2c_bus_rec *rec = &i2c->rec;
	uint32_t val;

	val = RADEON_READ(rec->get_data_reg);
	val &= rec->get_data_mask;
	return (val != 0);
}

static void set_clock(void *i2c_priv, int clock)
{
	struct radeon_i2c_chan *i2c = i2c_priv;
	struct drm_radeon_private *dev_priv = i2c->dev->dev_private;
	struct radeon_i2c_bus_rec *rec = &i2c->rec;
	uint32_t val;

	val = RADEON_READ(rec->put_clk_reg) & (uint32_t)~(rec->put_clk_mask);
	val |= clock ? 0 : rec->put_clk_mask;
	RADEON_WRITE(rec->put_clk_reg, val);
}

static void set_data(void *i2c_priv, int data)
{
	struct radeon_i2c_chan *i2c = i2c_priv;
	struct drm_radeon_private *dev_priv = i2c->dev->dev_private;
	struct radeon_i2c_bus_rec *rec = &i2c->rec;
	uint32_t val;

	val = RADEON_READ(rec->put_data_reg) & (uint32_t)~(rec->put_data_mask);
	val |= data ? 0 : rec->put_data_mask;
	RADEON_WRITE(rec->put_data_reg, val);
}

struct radeon_i2c_chan *radeon_i2c_create(struct drm_device *dev,
					  struct radeon_i2c_bus_rec *rec,
					  const char *name)
{
	struct radeon_i2c_chan *i2c;
	int ret;

	i2c = drm_calloc(1, sizeof(struct radeon_i2c_chan), DRM_MEM_DRIVER);
	if (i2c == NULL)
		return NULL;
	
	i2c->adapter.owner = THIS_MODULE;
	i2c->adapter.id = I2C_HW_B_RADEON;
	i2c->adapter.algo_data = &i2c->algo;
	i2c->dev = dev;
	i2c->algo.setsda = set_data;
	i2c->algo.setscl = set_clock;
	i2c->algo.getsda = get_data;
	i2c->algo.getscl = get_clock;
	i2c->algo.udelay = 20;
	i2c->algo.timeout = usecs_to_jiffies(2200);
	i2c->algo.data = i2c;
	i2c->rec = *rec;
	i2c_set_adapdata(&i2c->adapter, i2c);

	ret = i2c_bit_add_bus(&i2c->adapter);
	if (ret) {
		DRM_INFO("Failed to register i2c %s\n", name);
		goto out_free;
	}

	return i2c;
out_free:
	drm_free(i2c, sizeof(struct radeon_i2c_chan), DRM_MEM_DRIVER);
	return NULL;
	
}

void radeon_i2c_destroy(struct radeon_i2c_chan *i2c)
{
	if (!i2c)
		return;

	i2c_del_adapter(&i2c->adapter);
	drm_free(i2c, sizeof(struct radeon_i2c_chan), DRM_MEM_DRIVER);
}

struct drm_encoder *radeon_best_encoder(struct drm_connector *connector)
{
	return NULL;
}
