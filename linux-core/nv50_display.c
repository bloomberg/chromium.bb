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

#include "nv50_display.h"
#include "nv50_crtc.h"
#include "nv50_output.h"
#include "nv50_connector.h"

static int nv50_display_pre_init(struct nv50_display *display)
{
	struct drm_device *dev = display->dev;
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	int i;
	uint32_t ram_amount;

	NV50_DEBUG("\n");

	NV_WRITE(0x00610184, NV_READ(0x00614004));
	/*
	 * I think the 0x006101XX range is some kind of main control area that enables things.
	 */
	/* CRTC? */
	NV_WRITE(0x00610190 + 0 * 0x10, NV_READ(0x00616100 + 0 * 0x800));
	NV_WRITE(0x00610190 + 1 * 0x10, NV_READ(0x00616100 + 1 * 0x800));
	NV_WRITE(0x00610194 + 0 * 0x10, NV_READ(0x00616104 + 0 * 0x800));
	NV_WRITE(0x00610194 + 1 * 0x10, NV_READ(0x00616104 + 1 * 0x800));
	NV_WRITE(0x00610198 + 0 * 0x10, NV_READ(0x00616108 + 0 * 0x800));
	NV_WRITE(0x00610198 + 1 * 0x10, NV_READ(0x00616108 + 1 * 0x800));
	NV_WRITE(0x0061019c + 0 * 0x10, NV_READ(0x0061610c + 0 * 0x800));
	NV_WRITE(0x0061019c + 1 * 0x10, NV_READ(0x0061610c + 1 * 0x800));
	/* DAC */
	NV_WRITE(0x006101d0 + 0 * 0x4, NV_READ(0x0061a000 + 0 * 0x800));
	NV_WRITE(0x006101d0 + 1 * 0x4, NV_READ(0x0061a000 + 1 * 0x800));
	NV_WRITE(0x006101d0 + 2 * 0x4, NV_READ(0x0061a000 + 2 * 0x800));
	/* SOR */
	NV_WRITE(0x006101e0 + 0 * 0x4, NV_READ(0x0061c000 + 0 * 0x800));
	NV_WRITE(0x006101e0 + 1 * 0x4, NV_READ(0x0061c000 + 1 * 0x800));
	/* Something not yet in use, tv-out maybe. */
	NV_WRITE(0x006101f0 + 0 * 0x4, NV_READ(0x0061e000 + 0 * 0x800));
	NV_WRITE(0x006101f0 + 1 * 0x4, NV_READ(0x0061e000 + 1 * 0x800));
	NV_WRITE(0x006101f0 + 2 * 0x4, NV_READ(0x0061e000 + 2 * 0x800));

	for (i = 0; i < 3; i++) {
		NV_WRITE(NV50_PDISPLAY_DAC_REGS_DPMS_CTRL(i), 0x00550000 | NV50_PDISPLAY_DAC_REGS_DPMS_CTRL_PENDING);
		NV_WRITE(NV50_PDISPLAY_DAC_REGS_CLK_CTRL1(i), 0x00000001);
	}

	/* This used to be in crtc unblank, but seems out of place there. */
	NV_WRITE(NV50_PDISPLAY_UNK_380, 0);
	/* RAM is clamped to 256 MiB. */
	ram_amount = nouveau_mem_fb_amount(display->dev);
	NV50_DEBUG("ram_amount %d\n", ram_amount);
	if (ram_amount > 256*1024*1024)
		ram_amount = 256*1024*1024;
	NV_WRITE(NV50_PDISPLAY_RAM_AMOUNT, ram_amount - 1);
	NV_WRITE(NV50_PDISPLAY_UNK_388, 0x150000);
	NV_WRITE(NV50_PDISPLAY_UNK_38C, 0);

	display->preinit_done = TRUE;

	return 0;
}

static int nv50_display_init(struct nv50_display *display)
{
	struct drm_device *dev = display->dev;
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	uint32_t val;

	NV50_DEBUG("\n");

	/* The precise purpose is unknown, i suspect it has something to do with text mode. */
	if (NV_READ(NV50_PDISPLAY_SUPERVISOR) & 0x100) {
		NV_WRITE(NV50_PDISPLAY_SUPERVISOR, 0x100);
		NV_WRITE(0x006194e8, NV_READ(0x006194e8) & ~1);
		while (NV_READ(0x006194e8) & 2);
	}

	/* taken from nv bug #12637 */
	NV_WRITE(NV50_PDISPLAY_UNK200_CTRL, 0x2b00);
	do {
		val = NV_READ(NV50_PDISPLAY_UNK200_CTRL);
		if ((val & 0x9f0000) == 0x20000)
			NV_WRITE(NV50_PDISPLAY_UNK200_CTRL, val | 0x800000);

		if ((val & 0x3f0000) == 0x30000)
			NV_WRITE(NV50_PDISPLAY_UNK200_CTRL, val | 0x200000);
	} while (val & 0x1e0000);

	NV_WRITE(NV50_PDISPLAY_CTRL_STATE, NV50_PDISPLAY_CTRL_STATE_ENABLE);
	NV_WRITE(NV50_PDISPLAY_UNK200_CTRL, 0x1000b03);
	while (!(NV_READ(NV50_PDISPLAY_UNK200_CTRL) & 0x40000000));

	/* For the moment this is just a wrapper, which should be replaced with a real fifo at some point. */
	OUT_MODE(NV50_UNK84, 0);
	OUT_MODE(NV50_UNK88, 0);
	OUT_MODE(NV50_CRTC0_BLANK_CTRL, NV50_CRTC0_BLANK_CTRL_BLANK);
	OUT_MODE(NV50_CRTC0_UNK800, 0);
	OUT_MODE(NV50_CRTC0_DISPLAY_START, 0);
	OUT_MODE(NV50_CRTC0_UNK82C, 0);

	/* enable clock change interrupts. */
	NV_WRITE(NV50_PDISPLAY_SUPERVISOR_INTR, NV_READ(NV50_PDISPLAY_SUPERVISOR_INTR) | 0x70);

	display->init_done = TRUE;

	return 0;
}

static int nv50_display_disable(struct nv50_display *display)
{
	struct drm_device *dev = display->dev;
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	struct nv50_crtc *crtc = NULL;
	int i;

	NV50_DEBUG("\n");

	list_for_each_entry(crtc, &display->crtcs, head) {
		crtc->blank(crtc, TRUE);
	}

	display->update(display);

	/* Almost like ack'ing a vblank interrupt, maybe in the spirit of cleaning up? */
	list_for_each_entry(crtc, &display->crtcs, head) {
		if (crtc->active) {
			uint32_t mask;

			if (crtc->index == 1)
				mask = NV50_PDISPLAY_SUPERVISOR_CRTC1;
			else
				mask = NV50_PDISPLAY_SUPERVISOR_CRTC0;

			NV_WRITE(NV50_PDISPLAY_SUPERVISOR, mask);
			while (!(NV_READ(NV50_PDISPLAY_SUPERVISOR) & mask));
		}
	}

	NV_WRITE(NV50_PDISPLAY_UNK200_CTRL, 0);
	NV_WRITE(NV50_PDISPLAY_CTRL_STATE, 0);
	while ((NV_READ(NV50_PDISPLAY_UNK200_CTRL) & 0x1e0000) != 0);

	for (i = 0; i < 2; i++) {
		while (NV_READ(NV50_PDISPLAY_SOR_REGS_DPMS_STATE(i)) & NV50_PDISPLAY_SOR_REGS_DPMS_STATE_WAIT);
	}

	/* disable clock change interrupts. */
	NV_WRITE(NV50_PDISPLAY_SUPERVISOR_INTR, NV_READ(NV50_PDISPLAY_SUPERVISOR_INTR) & ~0x70);

	display->init_done = FALSE;

	return 0;
}

static int nv50_display_update(struct nv50_display *display)
{
	struct drm_device *dev = display->dev;
	struct drm_nouveau_private *dev_priv = dev->dev_private;

	NV50_DEBUG("\n");

	OUT_MODE(NV50_UPDATE_DISPLAY, 0);

	return 0;
}

int nv50_display_create(struct drm_device *dev)
{
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	struct nv50_display *display = kzalloc(sizeof(struct nv50_display), GFP_KERNEL);
	int i, type, output_index, bus;
	/* DAC0, DAC1, DAC2, SOR0, SOR1*/
	int or_counter[5] = {0, 0, 0, 0, 0};
	int i2c_index[5] = {0, 0, 0, 0, 0};
	uint32_t bus_mask = 0;
	uint32_t bus_digital = 0, bus_analog = 0;

	NV50_DEBUG("\n");

	if (!display)
		return -ENOMEM;

	INIT_LIST_HEAD(&display->crtcs);
	INIT_LIST_HEAD(&display->outputs);
	INIT_LIST_HEAD(&display->connectors);

	dev_priv->display_priv = display;

	for (i = 0; i < 2; i++) {
		nv50_crtc_create(dev, i);
	}

	/* we setup the outputs up from the BIOS table */
	for (i = 0 ; i < dev_priv->dcb_table.entries; i++) {
		type = dev_priv->dcb_table.entry[i].type;
		output_index = ffs(dev_priv->dcb_table.entry[i].or) - 1;
		bus = dev_priv->dcb_table.entry[i].bus;

		switch (type) {
			case DCB_OUTPUT_TMDS:
			case DCB_OUTPUT_LVDS:
				or_counter[output_index + 3] += 1;
				i2c_index[output_index + 3] = dev_priv->dcb_table.entry[i].i2c_index;
				bus_digital |= (1 << bus);
				nv50_sor_create(dev, i);
				break;
			case DCB_OUTPUT_ANALOG:
				or_counter[output_index] += 1;
				i2c_index[output_index] = dev_priv->dcb_table.entry[i].i2c_index;
				bus_analog |= (1 << bus);
				nv50_dac_create(dev, i);
				break;
			default:
				break;
		}

	}

	/* setup the connectors based on the output tables. */
	for (i = 0 ; i < dev_priv->dcb_table.entries; i++) {
		int connector_type = 0;
		type = dev_priv->dcb_table.entry[i].type;
		bus = dev_priv->dcb_table.entry[i].bus;

		/* already done? */
		if (bus_mask & (1 << bus))
			continue;

		/* only do it for supported outputs */
		if (type != DCB_OUTPUT_ANALOG && type != DCB_OUTPUT_TMDS
			&& type != DCB_OUTPUT_LVDS)
			continue;

		switch (type) {
			case DCB_OUTPUT_TMDS:
			case DCB_OUTPUT_ANALOG:
				if ((bus_digital & (1 << bus)) && (bus_analog & (1 << bus)))
					connector_type = CONNECTOR_DVI_I;
				else if (bus_digital & (1 << bus))
					connector_type = CONNECTOR_DVI_D;
				else if (bus_analog & (1 << bus))
					connector_type = CONNECTOR_VGA;
				break;
			case DCB_OUTPUT_LVDS:
				connector_type = CONNECTOR_LVDS;
				break;
			default:
				connector_type = CONNECTOR_UNKNOWN;
				break;
		}

		if (connector_type == CONNECTOR_UNKNOWN)
			continue;

		nv50_connector_create(dev, bus, dev_priv->dcb_table.entry[i].i2c_index, connector_type);

		bus_mask |= (1 << bus);
	}

	display->dev = dev;

	/* function pointers */
	display->init = nv50_display_init;
	display->pre_init = nv50_display_pre_init;
	display->disable = nv50_display_disable;
	display->update = nv50_display_update;

	return 0;
}

int nv50_display_destroy(struct drm_device *dev)
{
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	struct nv50_display *display = nv50_get_display(dev);
	struct nv50_crtc *crtc = NULL;
	struct nv50_output *output = NULL;
	struct nv50_connector *connector = NULL;

	NV50_DEBUG("\n");

	if (display->init_done)
		display->disable(display);

	list_for_each_entry(connector, &display->connectors, head) {
		connector->destroy(connector);
	}

	list_for_each_entry(output, &display->outputs, head) {
		output->destroy(output);
	}

	list_for_each_entry(crtc, &display->crtcs, head) {
		crtc->destroy(crtc);
	}

	kfree(display);
	dev_priv->display_priv = NULL;

	return 0;
}

/* This can be replaced with a real fifo in the future. */
void nv50_display_command(struct drm_nouveau_private *dev_priv, uint32_t mthd, uint32_t val)
{
	uint32_t counter = 0;

#if 1
	DRM_INFO("mthd 0x%03X val 0x%08X\n", mthd, val);
#endif

	NV_WRITE(NV50_PDISPLAY_CTRL_VAL, val);
	NV_WRITE(NV50_PDISPLAY_CTRL_STATE, NV50_PDISPLAY_CTRL_STATE_PENDING | 0x10000 | mthd | NV50_PDISPLAY_CTRL_STATE_ENABLE);

	while (NV_READ(NV50_PDISPLAY_CTRL_STATE) & NV50_PDISPLAY_CTRL_STATE_PENDING) {
		counter++;
		if (counter > 25000) {
			DRM_ERROR("You probably need a reboot now\n");
			break;
		}
	}
}

struct nv50_display *nv50_get_display(struct drm_device *dev)
{
	struct drm_nouveau_private *dev_priv = dev->dev_private;

	return (struct nv50_display *) dev_priv->display_priv;
}
