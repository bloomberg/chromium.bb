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

#include "nv50_output.h"

static int nv50_dac_validate_mode(struct nv50_output *output, struct nouveau_hw_mode *mode)
{
	NV50_DEBUG("\n");

	if (mode->clock > 400000) 
		return MODE_CLOCK_HIGH;

	if (mode->clock < 25000)
		return MODE_CLOCK_LOW;

	return MODE_OK;
}

static int nv50_dac_execute_mode(struct nv50_output *output, bool disconnect)
{
	struct drm_nouveau_private *dev_priv = output->dev->dev_private;
	struct nv50_crtc *crtc = output->crtc;
	struct nouveau_hw_mode *desired_mode = NULL;

	uint32_t offset = nv50_output_or_offset(output) * 0x80;

	uint32_t mode_ctl = NV50_DAC_MODE_CTRL_OFF;
	uint32_t mode_ctl2 = 0;

	NV50_DEBUG("or %d\n", nv50_output_or_offset(output));

	if (disconnect) {
		NV50_DEBUG("Disconnecting DAC\n");
		OUT_MODE(NV50_DAC0_MODE_CTRL + offset, mode_ctl);
		return 0;
	}

	desired_mode = (crtc->use_native_mode ? crtc->native_mode :
									crtc->mode);

	if (crtc->index == 1)
		mode_ctl |= NV50_DAC_MODE_CTRL_CRTC1;
	else
		mode_ctl |= NV50_DAC_MODE_CTRL_CRTC0;

	/* Lacking a working tv-out, this is not a 100% sure. */
	if (output->type == OUTPUT_DAC) {
		mode_ctl |= 0x40;
	} else if (output->type == OUTPUT_TV) {
		mode_ctl |= 0x100;
	}

	if (desired_mode->flags & V_NHSYNC)
		mode_ctl2 |= NV50_DAC_MODE_CTRL2_NHSYNC;

	if (desired_mode->flags & V_NVSYNC)
		mode_ctl2 |= NV50_DAC_MODE_CTRL2_NVSYNC;

	OUT_MODE(NV50_DAC0_MODE_CTRL + offset, mode_ctl);
	OUT_MODE(NV50_DAC0_MODE_CTRL2 + offset, mode_ctl2);

	return 0;
}

static int nv50_dac_set_clock_mode(struct nv50_output *output)
{
	struct drm_nouveau_private *dev_priv = output->dev->dev_private;

	NV50_DEBUG("or %d\n", nv50_output_or_offset(output));

	NV_WRITE(NV50_PDISPLAY_DAC_CLK_CLK_CTRL2(nv50_output_or_offset(output)),  0);

	return 0;
}

static int nv50_dac_destroy(struct nv50_output *output)
{
	struct drm_device *dev = output->dev;
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	struct nv50_display *display = nv50_get_display(dev);

	NV50_DEBUG("\n");

	if (!display || !output)
		return -EINVAL;

	list_del(&output->head);

	kfree(output->native_mode);
	if (dev_priv->free_output)
		dev_priv->free_output(output);

	return 0;
}

int nv50_dac_create(struct drm_device *dev, int dcb_entry)
{
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	struct nv50_output *output = NULL;
	struct nv50_display *display = NULL;
	struct dcb_entry *entry = NULL;
	int rval = 0;

	NV50_DEBUG("\n");

	/* This allows the public layer to do it's thing. */
	if (dev_priv->alloc_output)
		output = dev_priv->alloc_output(dev);

	if (!output)
		return -ENOMEM;

	output->dev = dev;

	display = nv50_get_display(dev);
	if (!display) {
		rval = -EINVAL;
		goto out;
	}

	entry = &dev_priv->dcb_table.entry[dcb_entry];
	if (!entry) {
		rval = -EINVAL;
		goto out;
	}

	switch (entry->type) {
		case DCB_OUTPUT_ANALOG:
			output->type = OUTPUT_DAC;
			DRM_INFO("Detected a DAC output\n");
			break;
		default:
			rval = -EINVAL;
			goto out;
	}

	output->dcb_entry = dcb_entry;
	output->bus = entry->bus;

	list_add_tail(&output->head, &display->outputs);

	output->native_mode = kzalloc(sizeof(struct nouveau_hw_mode), GFP_KERNEL);
	if (!output->native_mode) {
		rval = -ENOMEM;
		goto out;
	}

	/* Set function pointers. */
	output->validate_mode = nv50_dac_validate_mode;
	output->execute_mode = nv50_dac_execute_mode;
	output->set_clock_mode = nv50_dac_set_clock_mode;
	output->detect = NULL; /* TODO */
	output->destroy = nv50_dac_destroy;

	return 0;

out:
	if (output->native_mode)
		kfree(output->native_mode);
	if (dev_priv->free_output)
		dev_priv->free_output(output);
	return rval;
}

