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

static int nv50_sor_validate_mode(struct nv50_output *output, struct nouveau_hw_mode *mode)
{
	NV50_DEBUG("\n");

	if (mode->clock > 165000) /* no dual link until we figure it out completely */
		return MODE_CLOCK_HIGH;

	if (mode->clock < 25000)
		return MODE_CLOCK_LOW;

	if (output->native_mode->hdisplay > 0 && output->native_mode->vdisplay > 0) {
		if (mode->hdisplay > output->native_mode->hdisplay || mode->vdisplay > output->native_mode->vdisplay)
			return MODE_PANEL;
	}

	return MODE_OK;
}

static int nv50_sor_execute_mode(struct nv50_output *output, bool disconnect)
{
	struct drm_nouveau_private *dev_priv = output->dev->dev_private;
	struct nv50_crtc *crtc = output->crtc;
	struct nouveau_hw_mode *desired_mode = NULL;

	uint32_t offset = nv50_output_or_offset(output) * 0x40;

	uint32_t mode_ctl = NV50_SOR_MODE_CTRL_OFF;

	NV50_DEBUG("or %d\n", nv50_output_or_offset(output));

	if (disconnect) {
		NV50_DEBUG("Disconnecting SOR\n");
		OUT_MODE(NV50_SOR0_MODE_CTRL + offset, mode_ctl);
		return 0;
	}

	desired_mode = (crtc->use_native_mode ? crtc->native_mode : crtc->mode);

	if (output->type == OUTPUT_LVDS) {
		mode_ctl |= NV50_SOR_MODE_CTRL_LVDS;
	} else {
		mode_ctl |= NV50_SOR_MODE_CTRL_TMDS;
		if (desired_mode->clock > 165000)
			mode_ctl |= NV50_SOR_MODE_CTRL_TMDS_DUAL_LINK;
	}

	if (crtc->index == 1)
		mode_ctl |= NV50_SOR_MODE_CTRL_CRTC1;
	else
		mode_ctl |= NV50_SOR_MODE_CTRL_CRTC0;

	if (desired_mode->flags & V_NHSYNC)
		mode_ctl |= NV50_SOR_MODE_CTRL_NHSYNC;

	if (desired_mode->flags & V_NVSYNC)
		mode_ctl |= NV50_SOR_MODE_CTRL_NVSYNC;

	OUT_MODE(NV50_SOR0_MODE_CTRL + offset, mode_ctl);

	return 0;
}

static int nv50_sor_set_clock_mode(struct nv50_output *output)
{
	struct drm_nouveau_private *dev_priv = output->dev->dev_private;
	struct nv50_crtc *crtc = output->crtc;

	uint32_t limit = 165000;
	struct nouveau_hw_mode *hw_mode;

	NV50_DEBUG("or %d\n", nv50_output_or_offset(output));

	/* We don't yet know what to do, if anything at all. */
	if (output->type == OUTPUT_LVDS)
		return 0;

	if (crtc->use_native_mode)
		hw_mode = crtc->native_mode;
	else
		hw_mode = crtc->mode;

	/* 0x70000 was a late addition to nv, mentioned as fixing tmds initialisation on certain gpu's. */
	/* I presume it's some kind of clock setting, but what precisely i do not know. */
	NV_WRITE(NV50_PDISPLAY_SOR_CLK_CLK_CTRL2(nv50_output_or_offset(output)), 0x70000 | ((hw_mode->clock > limit) ? 0x101 : 0));

	return 0;
}

static int nv50_sor_set_power_mode(struct nv50_output *output, int mode)
{
	struct drm_nouveau_private *dev_priv = output->dev->dev_private;
	uint32_t val;
	int or = nv50_output_or_offset(output);

	NV50_DEBUG("or %d\n", nv50_output_or_offset(output));

	/* wait for it to be done */
	while (NV_READ(NV50_PDISPLAY_SOR_REGS_DPMS_CTRL(or)) & NV50_PDISPLAY_SOR_REGS_DPMS_CTRL_PENDING);

	val = NV_READ(NV50_PDISPLAY_SOR_REGS_DPMS_CTRL(or));

	if (mode == DPMSModeOn)
		val |= NV50_PDISPLAY_SOR_REGS_DPMS_CTRL_ON;
	else
		val &= ~NV50_PDISPLAY_SOR_REGS_DPMS_CTRL_ON;

	NV_WRITE(NV50_PDISPLAY_SOR_REGS_DPMS_CTRL(or), val | NV50_PDISPLAY_SOR_REGS_DPMS_CTRL_PENDING);

	return 0;
}

static int nv50_sor_destroy(struct nv50_output *output)
{
	struct drm_device *dev = output->dev;
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	struct nv50_display *display = nv50_get_display(dev);

	NV50_DEBUG("\n");

	if (!display || !output)
		return -EINVAL;

	list_del(&output->item);

	kfree(output->native_mode);
	if (dev_priv->free_output)
		dev_priv->free_output(output);

	return 0;
}

int nv50_sor_create(struct drm_device *dev, int dcb_entry)
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
		case DCB_OUTPUT_TMDS:
			output->type = OUTPUT_TMDS;
			DRM_INFO("Detected a TMDS output\n");
			break;
		case DCB_OUTPUT_LVDS:
			output->type = OUTPUT_LVDS;
			DRM_INFO("Detected a LVDS output\n");
			break;
		default:
			rval = -EINVAL;
			goto out;
	}

	output->dcb_entry = dcb_entry;
	output->bus = entry->bus;

	list_add_tail(&output->item, &display->outputs);

	output->native_mode = kzalloc(sizeof(struct nouveau_hw_mode), GFP_KERNEL);
	if (!output->native_mode) {
		rval = -ENOMEM;
		goto out;
	}

	/* Set function pointers. */
	output->validate_mode = nv50_sor_validate_mode;
	output->execute_mode = nv50_sor_execute_mode;
	output->set_clock_mode = nv50_sor_set_clock_mode;
	output->set_power_mode = nv50_sor_set_power_mode;
	output->detect = NULL;
	output->destroy = nv50_sor_destroy;

	/* Some default state, unknown what it precisely means. */
	if (output->type == OUTPUT_TMDS) {
		NV_WRITE(NV50_PDISPLAY_SOR_REGS_UNK_00C(nv50_output_or_offset(output)), 0x03010700);
		NV_WRITE(NV50_PDISPLAY_SOR_REGS_UNK_010(nv50_output_or_offset(output)), 0x0000152f);
		NV_WRITE(NV50_PDISPLAY_SOR_REGS_UNK_014(nv50_output_or_offset(output)), 0x00000000);
		NV_WRITE(NV50_PDISPLAY_SOR_REGS_UNK_018(nv50_output_or_offset(output)), 0x00245af8);
	}

	return 0;

out:
	if (output->native_mode)
		kfree(output->native_mode);
	if (dev_priv->free_output)
		dev_priv->free_output(output);
	return rval;
}
