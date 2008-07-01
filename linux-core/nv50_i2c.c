/*
 * Copyright (C) 2008 Maarten Maathuis.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE COPYRIGHT OWNER(S) AND/OR ITS SUPPLIERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

/* This is largely a clone from xorg i2c functions, as i had serious trouble getting an i2c_bit_algo adaptor running. */

#include "nv50_i2c.h"

static uint32_t nv50_i2c_port(int index)
{
	uint32_t port = 0;

	switch (index) {
		case 0:
			port = NV50_PCONNECTOR_I2C_PORT_0;
			break;
		case 1:
			port = NV50_PCONNECTOR_I2C_PORT_1;
			break;
		case 2:
			port = NV50_PCONNECTOR_I2C_PORT_2;
			break;
		case 3:
			port = NV50_PCONNECTOR_I2C_PORT_3;
			break;
		case 4:
			port = NV50_PCONNECTOR_I2C_PORT_4;
			break;
		case 5:
			port = NV50_PCONNECTOR_I2C_PORT_5;
			break;
		default:
			break;
	}

	if (!port) {
		DRM_ERROR("Invalid i2c port, returning 0.\n");
		BUG();
	}

	return port;
}

static void nv50_i2c_set_bits(struct nv50_i2c_channel *chan, int clock_high, int data_high)
{
	struct drm_nouveau_private *dev_priv = chan->dev->dev_private;
	uint32_t port = nv50_i2c_port(chan->index);

	if (!port)
		return;

	NV_WRITE(port, 4 | (data_high << 1) | clock_high);
}

static void nv50_i2c_get_bits(struct nv50_i2c_channel *chan, int *clock_high, int *data_high)
{
	struct drm_nouveau_private *dev_priv = chan->dev->dev_private;
	uint32_t port = nv50_i2c_port(chan->index);
	uint32_t val;

	if (!port)
		return;

	val = NV_READ(port);

	if (val & 1)
		*clock_high = 1;
	else
		*clock_high = 0;

	if (val & 2)
		*data_high = 1;
	else
		*data_high = 0;
}

static bool nv50_i2c_raise_clock(struct nv50_i2c_channel *chan, int data)
{
	int i, clock;

	nv50_i2c_set_bits(chan, 1, data);
	udelay(2);

	for (i = 2200; i > 0; i -= 2) {
		nv50_i2c_get_bits(chan, &clock, &data);
		if (clock)
			return TRUE;
		udelay(2);
	}

	printk("a timeout occured in nv50_i2c_raise_clock\n");

	return FALSE;
}

static bool nv50_i2c_start(struct nv50_i2c_channel *chan)
{
	if (!nv50_i2c_raise_clock(chan, 1))
		return FALSE;

	nv50_i2c_set_bits(chan, 1, 0);
	udelay(5);

	nv50_i2c_set_bits(chan, 0, 0);
	udelay(5);

	return TRUE;
}

static void nv50_i2c_stop(struct nv50_i2c_channel *chan)
{
	nv50_i2c_set_bits(chan, 0, 0);
	udelay(2);

	nv50_i2c_set_bits(chan, 1, 0);
	udelay(5);

	nv50_i2c_set_bits(chan, 1, 1);
	udelay(5);
}

static bool nv50_i2c_write_bit(struct nv50_i2c_channel *chan, int data)
{
	bool rval;

	nv50_i2c_set_bits(chan, 0, data);
	udelay(2);

	rval = nv50_i2c_raise_clock(chan, data);
	udelay(5);

	nv50_i2c_set_bits(chan, 0, data);
	udelay(5);

	return rval;
}

static bool nv50_i2c_read_bit(struct nv50_i2c_channel *chan, int *data)
{
	bool rval;
	int clock;

	rval = nv50_i2c_raise_clock(chan, 1);
	udelay(5);

	nv50_i2c_get_bits(chan, &clock, data);
	udelay(5);

	nv50_i2c_set_bits(chan, 0, 1);
	udelay(5);

	return rval;
}

static bool nv50_i2c_write_byte(struct nv50_i2c_channel *chan, uint8_t byte)
{
	bool rval;
	int i, clock, data;

	for (i = 7; i >= 0; i--)
		if (!nv50_i2c_write_bit(chan, (byte >> i) & 1))
			return FALSE;

	nv50_i2c_set_bits(chan, 0, 1);
	udelay(5);

	rval = nv50_i2c_raise_clock(chan, 1);

	if (rval) {
		for (i = 40; i > 0; i -= 2) {
			udelay(2);
			nv50_i2c_get_bits(chan, &clock, &data);
			if (data == 0) 
				break;
		}

		if (i <= 0) {
			printk("a timeout occured in nv50_i2c_write_byte\n");
			rval = FALSE;
		}
	}

	nv50_i2c_set_bits(chan, 0, 1);
	udelay(5);

	return rval;
}

static bool nv50_i2c_read_byte(struct nv50_i2c_channel *chan, uint8_t *byte, bool last)
{
	int i, bit;

	nv50_i2c_set_bits(chan, 0, 1);
	udelay(5);

	*byte = 0;

	for (i = 7; i >= 0; i--) {
		if (nv50_i2c_read_bit(chan, &bit)) {
			if (bit)
				*byte |= (1 << i);
		} else {
			return FALSE;
		}
	}

	if (!nv50_i2c_write_bit(chan, last ? 1 : 0))
		return FALSE;

	return TRUE;
}

/* only 7 bits addresses. */
static bool nv50_i2c_address(struct nv50_i2c_channel *chan, uint8_t address, bool write)
{
	if (nv50_i2c_start(chan)) {
		uint8_t real_addr = (address << 1);
		if (!write)
			real_addr |= 1;

		if (nv50_i2c_write_byte(chan, real_addr))
			return TRUE;

		/* failure, so issue stop */
		nv50_i2c_stop(chan);
	}

	return FALSE;
}

static bool nv50_i2c_read(struct nv50_i2c_channel *chan, uint8_t address, uint8_t *buffer, uint32_t length)
{
	int i, j;
	bool rval, last;

	/* retries */
	for (i = 0; i < 4; i++) {
		rval = nv50_i2c_address(chan, address, FALSE);
		if (!rval)
			return FALSE;

		for (j = 0; j < length; j++) {
			last = false;
			if (j == (length - 1))
				last = true;
			rval = nv50_i2c_read_byte(chan, &buffer[j], last);
			if (!rval) {
				nv50_i2c_stop(chan);
				break;
			}
		}

		nv50_i2c_stop(chan);

		/* done */
		if (rval)
			break;
	}

	if (!rval)
		printk("nv50_i2c_read failed\n");

	return rval;
}

static bool nv50_i2c_write(struct nv50_i2c_channel *chan, uint8_t address, uint8_t *buffer, uint32_t length)
{
	int i, j;
	bool rval;

	/* retries */
	for (i = 0; i < 4; i++) {
		rval = nv50_i2c_address(chan, address, TRUE);
		if (!rval)
			return FALSE;

		for (j = 0; j < length; j++) {
			rval = nv50_i2c_write_byte(chan, buffer[j]);
			if (!rval) {
				break;
			}
		}

		nv50_i2c_stop(chan);

		/* done */
		if (rval)
			break;
	}

	if (!rval)
		printk("nv50_i2c_write failed\n");

	return rval;
}

static int nv50_i2c_xfer(struct i2c_adapter *i2c_adap, struct i2c_msg *msgs, int num)
{
	struct nv50_i2c_channel *chan = i2c_get_adapdata(i2c_adap);
	bool rval;
	int i;

	for (i = 0; i < num; i++) {
		if (msgs[i].flags & I2C_M_RD) { /* read */
			rval = nv50_i2c_read(chan, msgs[i].addr, msgs[i].buf, msgs[i].len);
		} else { /* write */
			rval = nv50_i2c_write(chan, msgs[i].addr, msgs[i].buf, msgs[i].len);
		}

		if (!rval)
			break;
	}

	if (rval)
		return i;
	else
		return -EINVAL;
}

static u32 nv50_i2c_functionality(struct i2c_adapter *adap)
{
	return I2C_FUNC_I2C;
}

static const struct i2c_algorithm nv50_i2c_algo = {
	.master_xfer	= nv50_i2c_xfer,
	.functionality	= nv50_i2c_functionality,
};

static int nv50_i2c_register_bus(struct i2c_adapter *adap)
{
	adap->algo = &nv50_i2c_algo;

	adap->timeout = 40;
	adap->retries = 4;

	return i2c_add_adapter(adap);
}

#define I2C_HW_B_NOUVEAU 0x010030
struct nv50_i2c_channel *nv50_i2c_channel_create(struct drm_device *dev, uint32_t index)
{
	struct nv50_i2c_channel *chan;

	chan = kzalloc(sizeof(struct nv50_i2c_channel), GFP_KERNEL);

	if (!chan)
		goto out;

	DRM_INFO("Creating i2c bus with index %d\n", index);

	chan->dev = dev;
	chan->index = index;
	snprintf(chan->adapter.name, I2C_NAME_SIZE, "nv50 i2c %d", index);
	chan->adapter.owner = THIS_MODULE;
	chan->adapter.id = I2C_HW_B_NOUVEAU;
	chan->adapter.dev.parent = &dev->pdev->dev;

	i2c_set_adapdata(&chan->adapter, chan);

	if (nv50_i2c_register_bus(&chan->adapter))
		goto out;

	return chan;

out:
	kfree(chan);
	return NULL;
}

void nv50_i2c_channel_destroy(struct nv50_i2c_channel *chan)
{
	if (!chan)
		return;

	i2c_del_adapter(&chan->adapter);
	kfree(chan);
}
