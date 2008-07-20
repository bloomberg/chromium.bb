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

#include "nv50_connector.h"

static struct nv50_output *nv50_connector_to_output(struct nv50_connector *connector, bool digital)
{
	struct nv50_display *display = nv50_get_display(connector->dev);
	struct nv50_output *output = NULL;
	bool digital_possible = false;
	bool analog_possible = false;

	switch (connector->type) {
		case CONNECTOR_VGA:
		case CONNECTOR_TV:
			analog_possible = true;
			break;
		case CONNECTOR_DVI_I:
			analog_possible = true;
			digital_possible = true;
			break;
		case CONNECTOR_DVI_D:
		case CONNECTOR_LVDS:
			digital_possible = true;
			break;
		default:
			break;
	}

	/* Return early on bad situations. */
	if (!analog_possible && !digital_possible)
		return NULL;

	if (!analog_possible && !digital)
		return NULL;

	if (!digital_possible && digital)
		return NULL;

	list_for_each_entry(output, &display->outputs, item) {
		if (connector->bus != output->bus)
			continue;
		if (digital && output->type == OUTPUT_TMDS)
			return output;
		if (digital && output->type == OUTPUT_LVDS)
			return output;
		if (!digital && output->type == OUTPUT_DAC)
			return output;
		if (!digital && output->type == OUTPUT_TV)
			return output;
	}

	return NULL;
}

static bool nv50_connector_detect(struct nv50_connector *connector)
{
	/* kindly borrrowed from the intel driver, hope it works. */
	uint8_t out_buf[] = { 0x0, 0x0};
	uint8_t buf[2];
	int ret;
	struct i2c_msg msgs[] = {
		{
			.addr = 0x50,
			.flags = 0,
			.len = 1,
			.buf = out_buf,
		},
		{
			.addr = 0x50,
			.flags = I2C_M_RD,
			.len = 1,
			.buf = buf,
		}
	};

	NV50_DEBUG("\n");

	if (!connector->i2c_chan)
		return false;

	ret = i2c_transfer(&connector->i2c_chan->adapter, msgs, 2);
	DRM_INFO("I2C detect returned %d\n", ret);

	if (ret == 2)
		return true;

	return false;
}

static int nv50_connector_destroy(struct nv50_connector *connector)
{
	struct drm_device *dev = connector->dev;
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	struct nv50_display *display = nv50_get_display(dev);

	NV50_DEBUG("\n");

	if (!display || !connector)
		return -EINVAL;

	list_del(&connector->item);

	if (connector->i2c_chan)
		nv50_i2c_channel_destroy(connector->i2c_chan);

	if (dev_priv->free_connector)
		dev_priv->free_connector(connector);

	return 0;
}

int nv50_connector_create(struct drm_device *dev, int bus, int i2c_index, int type)
{
	struct nv50_connector *connector = NULL;
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	struct nv50_display *display = NULL;

	NV50_DEBUG("\n");

	/* This allows the public layer to do it's thing. */
	if (dev_priv->alloc_connector)
		connector = dev_priv->alloc_connector(dev);

	if (!connector)
		return -ENOMEM;

	connector->dev = dev;

	display = nv50_get_display(dev);
	if (!display)
		goto out;

	if (type == CONNECTOR_UNKNOWN)
		goto out;

	list_add_tail(&connector->item, &display->connectors);

	connector->bus = bus;
	connector->type = type;

	switch (type) {
		case CONNECTOR_VGA:
			DRM_INFO("Detected a VGA connector\n");
			break;
		case CONNECTOR_DVI_D:
			DRM_INFO("Detected a DVI-D connector\n");
			break;
		case CONNECTOR_DVI_I:
			DRM_INFO("Detected a DVI-I connector\n");
			break;
		case CONNECTOR_LVDS:
			DRM_INFO("Detected a LVDS connector\n");
			break;
		case CONNECTOR_TV:
			DRM_INFO("Detected a TV connector\n");
			break;
		default:
			DRM_ERROR("Unknown connector, this is not good.\n");
			break;
	}

	/* some reasonable defaults */
	if (type == CONNECTOR_DVI_D || type == CONNECTOR_DVI_I || type == CONNECTOR_LVDS)
		connector->scaling_mode = SCALE_FULLSCREEN;
	else
		connector->scaling_mode = SCALE_NON_GPU;

	connector->use_dithering = false;

	if (i2c_index < 0xf)
		connector->i2c_chan = nv50_i2c_channel_create(dev, i2c_index);

	/* set function pointers */
	connector->detect = nv50_connector_detect;
	connector->destroy = nv50_connector_destroy;
	connector->to_output = nv50_connector_to_output;

	return 0;

out:
	if (dev_priv->free_connector)
		dev_priv->free_connector(connector);

	return -EINVAL;
}
