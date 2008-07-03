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

#include "nv50_kms_wrapper.h"
#include "drm_crtc_helper.h" /* be careful what you use from this */

/* This file serves as the interface between the common kernel modesetting code and the device dependent implementation. */

/*
 * Get private functions.
 */

struct nv50_kms_priv *nv50_get_kms_priv(struct drm_device *dev)
{
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	return dev_priv->kms_priv;
}

/*
 * Allocation functions.
 */

static void *nv50_kms_alloc_crtc(struct drm_device *dev)
{
	struct nv50_kms_priv *kms_priv = nv50_get_kms_priv(dev);
	struct nv50_kms_crtc *crtc = kzalloc(sizeof(struct nv50_kms_crtc), GFP_KERNEL);

	if (!crtc)
		return NULL;

	list_add_tail(&crtc->item, &kms_priv->crtcs);

	return &(crtc->priv);
}

static void *nv50_kms_alloc_output(struct drm_device *dev)
{
	struct nv50_kms_priv *kms_priv = nv50_get_kms_priv(dev);
	struct nv50_kms_encoder *encoder = kzalloc(sizeof(struct nv50_kms_encoder), GFP_KERNEL);

	if (!encoder)
		return NULL;

	list_add_tail(&encoder->item, &kms_priv->encoders);

	return &(encoder->priv);
}

static void *nv50_kms_alloc_connector(struct drm_device *dev)
{
	struct nv50_kms_priv *kms_priv = nv50_get_kms_priv(dev);
	struct nv50_kms_connector *connector = kzalloc(sizeof(struct nv50_kms_connector), GFP_KERNEL);

	if (!connector)
		return NULL;

	list_add_tail(&connector->item, &kms_priv->connectors);

	return &(connector->priv);
}

static void nv50_kms_free_crtc(void *crtc)
{
	struct nv50_kms_crtc *kms_crtc = from_nv50_crtc(crtc);

	list_del(&kms_crtc->item);

	kfree(kms_crtc);
}

static void nv50_kms_free_output(void *output)
{
	struct nv50_kms_encoder *kms_encoder = from_nv50_output(output);

	list_del(&kms_encoder->item);

	kfree(kms_encoder);
}

static void nv50_kms_free_connector(void *connector)
{
	struct nv50_kms_connector *kms_connector = from_nv50_connector(connector);

	list_del(&kms_connector->item);

	kfree(kms_connector);
}

/*
 * Mode conversion functions.
 */

static struct nouveau_hw_mode *nv50_kms_to_hw_mode(struct drm_display_mode *mode)
{
	struct nouveau_hw_mode *hw_mode = kzalloc(sizeof(struct nouveau_hw_mode), GFP_KERNEL);
	if (!hw_mode)
		return NULL;

	/* create hw values. */
	hw_mode->clock = mode->clock;
	hw_mode->flags = hw_mode->flags;

	hw_mode->hdisplay = mode->hdisplay;
	hw_mode->hsync_start = mode->hsync_start;
	hw_mode->hsync_end = mode->hsync_end;
	hw_mode->htotal = mode->htotal;

	hw_mode->hblank_start = mode->hdisplay + 1;
	hw_mode->hblank_end = mode->htotal;

	hw_mode->vdisplay = mode->vdisplay;
	hw_mode->vsync_start = mode->vsync_start;
	hw_mode->vsync_end = mode->vsync_end;
	hw_mode->vtotal = mode->vtotal;

	hw_mode->vblank_start = mode->vdisplay + 1;
	hw_mode->vblank_end = mode->vtotal;

	return hw_mode;
}

/*
 * State mirroring functions.
 */

static void nv50_kms_mirror_routing(struct drm_device *dev)
{
	struct nv50_display *display = nv50_get_display(dev);
	struct nv50_crtc *crtc = NULL;
	struct nv50_output *output = NULL;
	struct nv50_connector *connector = NULL;
	struct drm_connector *drm_connector = NULL;
	struct drm_crtc *drm_crtc = NULL;

	/* Wipe all previous connections. */
	list_for_each_entry(connector, &display->connectors, item) {
		connector->output = NULL;
	}

	list_for_each_entry(output, &display->outputs, item) {
		output->crtc = NULL;
	}

	list_for_each_entry(drm_connector, &dev->mode_config.connector_list, head) {
		if (drm_connector->encoder) {
			output = to_nv50_output(drm_connector->encoder);
			connector = to_nv50_connector(drm_connector);

			/* hook up output to connector. */
			connector->output = output;

			if (drm_connector->encoder->crtc) {
				crtc = to_nv50_crtc(drm_connector->encoder->crtc);

				/* hook up output to crtc. */
				output->crtc = crtc;
			}
		}
	}

	/* mirror crtc active state */
	list_for_each_entry(drm_crtc, &dev->mode_config.crtc_list, head) {
		crtc = to_nv50_crtc(drm_crtc);

		crtc->enabled = drm_crtc->enabled;
	}
}

/*
 * FB functions.
 */

static void nv50_kms_framebuffer_destroy(struct drm_framebuffer *drm_framebuffer)
{
	drm_framebuffer_cleanup(drm_framebuffer);

	kfree(drm_framebuffer);
}

static const struct drm_framebuffer_funcs nv50_kms_fb_funcs = {
	.destroy = nv50_kms_framebuffer_destroy,
};

/*
 * Mode config functions.
 */

static struct drm_framebuffer *nv50_kms_framebuffer_create(struct drm_device *dev,
						struct drm_file *file_priv, struct drm_mode_fb_cmd *mode_cmd)
{
	struct drm_framebuffer *drm_framebuffer = kzalloc(sizeof(struct drm_framebuffer), GFP_KERNEL);
	if (!drm_framebuffer)
		return NULL;

	drm_framebuffer_init(dev, drm_framebuffer, &nv50_kms_fb_funcs);
	drm_helper_mode_fill_fb_struct(drm_framebuffer, mode_cmd);

	return drm_framebuffer;
}

static int nv50_kms_fb_changed(struct drm_device *dev)
{
	return 0; /* not needed until nouveaufb? */
}

static const struct drm_mode_config_funcs nv50_kms_mode_funcs = {
	.resize_fb = NULL,
	.fb_create = nv50_kms_framebuffer_create,
	.fb_changed = nv50_kms_fb_changed,
};

/*
 * CRTC functions.
 */

static int nv50_kms_crtc_cursor_set(struct drm_crtc *drm_crtc, uint32_t buffer_handle,
			  uint32_t width, uint32_t height)
{
	struct nv50_crtc *crtc = to_nv50_crtc(drm_crtc);
	struct nv50_display *display = nv50_get_display(crtc->dev);
	int rval = 0;

	if (width != 64 || height != 64)
		return -EINVAL;

	/* set bo before doing show cursor */
	if (buffer_handle) {
		rval = crtc->cursor->set_bo(crtc, (drm_handle_t) buffer_handle);
		if (rval != 0)
			goto out;
	}

	crtc->cursor->visible = buffer_handle ? true : false;

	if (buffer_handle) {
		rval = crtc->cursor->show(crtc);
		if (rval != 0)
			goto out;
	} else { /* no handle implies hiding the cursor */
		rval = crtc->cursor->hide(crtc);
		goto out;
	}

	if (rval != 0)
		return rval;

out:
	/* in case this triggers any other cursor changes */
	display->update(display);

	return rval;
}

static int nv50_kms_crtc_cursor_move(struct drm_crtc *drm_crtc, int x, int y)
{
	struct nv50_crtc *crtc = to_nv50_crtc(drm_crtc);

	return crtc->cursor->set_pos(crtc, x, y);
}

void nv50_kms_crtc_gamma_set(struct drm_crtc *drm_crtc, u16 *r, u16 *g, u16 *b,
		uint32_t size)
{
	struct nv50_crtc *crtc = to_nv50_crtc(drm_crtc);

	if (size != 256)
		return;

	crtc->lut->set(crtc, (uint16_t *)r, (uint16_t *)g, (uint16_t *)b);
}

int nv50_kms_crtc_set_config(struct drm_mode_set *set)
{
	int rval = 0, i;
	uint32_t crtc_mask = 0;
	struct drm_device *dev = NULL;
	struct drm_nouveau_private *dev_priv = NULL;
	struct nv50_display *display = NULL;
	struct drm_connector *drm_connector = NULL;
	struct drm_encoder *drm_encoder = NULL;
	struct drm_crtc *drm_crtc = NULL;

	struct nv50_crtc *crtc = NULL;
	struct nv50_output *output = NULL;
	struct nv50_connector *connector = NULL;
	struct nouveau_hw_mode *hw_mode = NULL;
	struct nv50_fb_info fb_info;

	bool blank = false;
	bool switch_fb = false;
	bool modeset = false;

	NV50_DEBUG("\n");

	/*
	 * Supported operations:
	 * - Switch mode.
	 * - Switch framebuffer.
	 * - Blank screen.
	 */

	/* Sanity checking */
	if (!set) {
		DRM_ERROR("Sanity check failed\n");
		goto out;
	}

	if (!set->crtc) {
		DRM_ERROR("Sanity check failed\n");
		goto out;
	}

	if (set->mode) {
		if (set->fb) {
			if (!drm_mode_equal(set->mode, &set->crtc->mode))
				modeset = true;

			if (set->fb != set->crtc->fb)
				switch_fb = true;

			if (set->x != set->crtc->x || set->y != set->crtc->y)
				switch_fb = true;
		}
	} else {
		blank = true;
	}

	if (!set->connectors && modeset) {
		DRM_ERROR("Sanity check failed\n");
		goto out;
	}

	if (!modeset && !switch_fb && !blank) {
		DRM_ERROR("There is nothing to do, bad input.\n");
		goto out;
	}

	/* Basic variable setting */
	dev = set->crtc->dev;
	dev_priv = dev->dev_private;
	display = nv50_get_display(dev);
	crtc = to_nv50_crtc(set->crtc);

	/**
	 * Wiring up the encoders and connectors.
	 */

	if (modeset) {
		/* Mode validation */
		hw_mode = nv50_kms_to_hw_mode(set->mode);

		rval = crtc->validate_mode(crtc, hw_mode);

		if (rval != MODE_OK) {
			DRM_ERROR("Mode not ok\n");
			goto out;
		}

		for (i = 0; i < set->num_connectors; i++) {
			drm_connector = set->connectors[i];
			if (!drm_connector) {
				DRM_ERROR("No connector\n");
				goto out;
			}
			connector = to_nv50_connector(drm_connector);

			output = connector->to_output(connector, connector->digital);
			if (!output) {
				DRM_ERROR("No output\n");
				goto out;
			}

			rval = output->validate_mode(output, hw_mode);
			if (rval != MODE_OK) {
				DRM_ERROR("Mode not ok\n");
				goto out;
			}
		}

		/* Validation done, move on to cleaning of existing structures. */

		/* find encoders that use this crtc. */
		list_for_each_entry(drm_encoder, &dev->mode_config.encoder_list, head) {
			if (drm_encoder->crtc == set->crtc) {
				/* find the connector that goes with it */
				list_for_each_entry(drm_connector, &dev->mode_config.connector_list, head) {
					if (drm_connector->encoder == drm_encoder) {
						drm_connector->encoder =  NULL;
						break;
					}
				}
				drm_encoder->crtc = NULL;
			}
		}

		/* now find if our desired encoders or connectors are in use already. */
		for (i = 0; i < set->num_connectors; i++) {
			drm_connector = set->connectors[i];
			if (!drm_connector) {
				DRM_ERROR("No connector\n");
				goto out;
			}

			if (!drm_connector->encoder)
				continue;

			drm_encoder = drm_connector->encoder;
			drm_connector->encoder = NULL;

			if (!drm_encoder->crtc)
				continue;

			drm_crtc = drm_encoder->crtc;
			drm_encoder->crtc = NULL;

			drm_crtc->enabled = false;
		}

		/* Time to wire up the public encoder, the private one will be handled later. */
		for (i = 0; i < set->num_connectors; i++) {
			drm_connector = set->connectors[i];
			if (!drm_connector) {
				DRM_ERROR("No connector\n");
				goto out;
			}

			output = connector->to_output(connector, connector->digital);
			if (!output) {
				DRM_ERROR("No output\n");
				goto out;
			}

			/* find the encoder public structure that matches out output structure. */
			drm_encoder = to_nv50_kms_encoder(output);

			if (!drm_encoder) {
				DRM_ERROR("No encoder\n");
				goto out;
			}

			drm_encoder->crtc = set->crtc;
			set->crtc->enabled = true;
			drm_connector->encoder = drm_encoder;
		}
	}

	/**
	 * Disable crtc.
	 */

	if (blank) {
		crtc = to_nv50_crtc(set->crtc);

		/* keeping the encoders and connectors attached, so they can be tracked */
		set->crtc->enabled = false;
	}

	/**
	 * All state should now be updated, now onto the real work.
	 */

	/* mirror everything to the private structs */
	nv50_kms_mirror_routing(dev);

	/**
	 * Bind framebuffer.
	 */

	if (switch_fb) {
		crtc = to_nv50_crtc(set->crtc);

		/* set framebuffer */
		set->crtc->fb = set->fb;

		/* set private framebuffer */
		crtc = to_nv50_crtc(set->crtc);
		fb_info.block = find_block_by_handle(dev_priv->fb_heap, set->fb->mm_handle);
		fb_info.width = set->fb->width;
		fb_info.height = set->fb->height;
		fb_info.depth = set->fb->depth;
		fb_info.bpp = set->fb->bits_per_pixel;
		fb_info.pitch = set->fb->pitch;
		fb_info.x = set->x;
		fb_info.y = set->y;

		rval = crtc->fb->bind(crtc, &fb_info);
		if (rval != 0) {
			DRM_ERROR("fb_bind failed\n");
			goto out;
		}
	}

	/* this is !cursor_show */
	if (!crtc->cursor->enabled) {
		rval = crtc->cursor->enable(crtc);
		if (rval != 0) {
			DRM_ERROR("cursor_enable failed\n");
			goto out;
		}
	}

	/**
	 * Blanking.
	 */

	if (blank) {
		crtc = to_nv50_crtc(set->crtc);

		rval = crtc->blank(crtc, true);
		if (rval != 0) {
			DRM_ERROR("blanking failed\n");
			goto out;
		}

		/* detach any outputs that are currently running on this crtc */
		list_for_each_entry(drm_encoder, &dev->mode_config.encoder_list, head) {
			if (drm_encoder->crtc == set->crtc) {
				output = to_nv50_output(drm_encoder);

				rval = output->execute_mode(output, true);
				if (rval != 0) {
					DRM_ERROR("detaching output failed\n");
					goto out;
				}
			}
		}
	}

	/**
	 * Change framebuffer, without changing mode.
	 */

	if (switch_fb && !modeset && !blank) {
		crtc = to_nv50_crtc(set->crtc);

		rval = crtc->blank(crtc, true);
		if (rval != 0) {
			DRM_ERROR("blanking failed\n");
			goto out;
		}

		rval = crtc->set_fb(crtc);
		if (rval != 0) {
			DRM_ERROR("set_fb failed\n");
			goto out;
		}

		/* this also sets the fb offset */
		rval = crtc->blank(crtc, false);
		if (rval != 0) {
			DRM_ERROR("unblanking failed\n");
			goto out;
		}
	}

	/**
	 * Normal modesetting.
	 */

	if (modeset) {
		crtc = to_nv50_crtc(set->crtc);

		/* disconnect unused outputs */
		list_for_each_entry(output, &display->outputs, item) {
			if (output->crtc) {
				crtc_mask |= 1 << output->crtc->index;
			} else {
				rval = output->execute_mode(output, true);
				if (rval != 0) {
					DRM_ERROR("detaching output failed\n");
					goto out;
				}
			}
		}

		/* blank any unused crtcs */
		list_for_each_entry(crtc, &display->crtcs, item) {
			if (!(crtc_mask & (1 << crtc->index)))
				crtc->blank(crtc, true);
		}

		crtc = to_nv50_crtc(set->crtc);

		rval = crtc->set_mode(crtc, hw_mode);
		if (rval != 0) {
			DRM_ERROR("crtc mode set failed\n");
			goto out;
		}

		/* find native mode. */
		list_for_each_entry(output, &display->outputs, item) {
			if (output->crtc != crtc)
				continue;

			*crtc->native_mode = *output->native_mode;
			list_for_each_entry(connector, &display->connectors, item) {
				if (connector->output != output)
					continue;

				crtc->scaling_mode = connector->scaling_mode;
				break;
			}

			if (crtc->scaling_mode == SCALE_PANEL)
				crtc->use_native_mode = false;
			else
				crtc->use_native_mode = true;

			break; /* no use in finding more than one mode */
		}

		rval = crtc->execute_mode(crtc);
		if (rval != 0) {
			DRM_ERROR("crtc execute mode failed\n");
			goto out;
		}

		list_for_each_entry(output, &display->outputs, item) {
			if (output->crtc != crtc)
				continue;

			rval = output->execute_mode(output, false);
			if (rval != 0) {
				DRM_ERROR("output execute mode failed\n");
				goto out;
			}
		}

		rval = crtc->set_scale(crtc);
		if (rval != 0) {
			DRM_ERROR("crtc set scale failed\n");
			goto out;
		}

		/* next line changes crtc, so putting it here is important */
		display->last_crtc = crtc->index;

		/* this is executed immediately */
		list_for_each_entry(output, &display->outputs, item) {
			if (output->crtc != crtc)
				continue;

			rval = output->set_power_mode(output, DPMSModeOn);
			if (rval != 0) {
				DRM_ERROR("output set power mode failed\n");
				goto out;
			}
		}

		/* update dpms state to DPMSModeOn */
		for (i = 0; i < set->num_connectors; i++) {
			drm_connector = set->connectors[i];
			if (!drm_connector) {
				DRM_ERROR("No connector\n");
				goto out;
			}

			rval = drm_connector_property_set_value(drm_connector,
					dev->mode_config.dpms_property,
					DPMSModeOn);
			if (rval != 0) {
				DRM_ERROR("failed to update dpms state\n");
				goto out;
			}
		}
	}

	display->update(display);

	kfree(hw_mode);

	return 0;

out:
	kfree(hw_mode);

	if (rval != 0)
		return rval;
	else
		return -EINVAL;
}

static void nv50_kms_crtc_destroy(struct drm_crtc *drm_crtc)
{
	struct nv50_crtc *crtc = to_nv50_crtc(drm_crtc);

	drm_crtc_cleanup(drm_crtc);

	/* this will even destroy the public structure. */
	crtc->destroy(crtc);
}

static const struct drm_crtc_funcs nv50_kms_crtc_funcs = {
	.save = NULL,
	.restore = NULL,
	.cursor_set = nv50_kms_crtc_cursor_set,
	.cursor_move = nv50_kms_crtc_cursor_move,
	.gamma_set = nv50_kms_crtc_gamma_set,
	.set_config = nv50_kms_crtc_set_config,
	.destroy = nv50_kms_crtc_destroy,
};

static int nv50_kms_crtcs_init(struct drm_device *dev)
{
	struct nv50_display *display = nv50_get_display(dev);
	struct nv50_crtc *crtc = NULL;

	/*
	 * This may look a bit confusing, but:
	 * The internal structure is already allocated and so is the public one.
	 * Just a matter of getting to the memory and register it.
	 */
	list_for_each_entry(crtc, &display->crtcs, item) {
		struct drm_crtc *drm_crtc = to_nv50_kms_crtc(crtc);

		drm_crtc_init(dev, drm_crtc, &nv50_kms_crtc_funcs);
	}

	return 0;
}

/*
 * Encoder functions
 */

static void nv50_kms_encoder_destroy(struct drm_encoder *drm_encoder)
{
	struct nv50_output *output = to_nv50_output(drm_encoder);

	drm_encoder_cleanup(drm_encoder);

	/* this will even destroy the public structure. */
	output->destroy(output);
}

static const struct drm_encoder_funcs nv50_kms_encoder_funcs = {
	.destroy = nv50_kms_encoder_destroy,
};

static int nv50_kms_encoders_init(struct drm_device *dev)
{
	struct nv50_display *display = nv50_get_display(dev);
	struct nv50_output *output = NULL;

	list_for_each_entry(output, &display->outputs, item) {
		struct drm_encoder *drm_encoder = to_nv50_kms_encoder(output);
		uint32_t type = DRM_MODE_ENCODER_NONE;

		switch (output->type) {
			case OUTPUT_DAC:
				type = DRM_MODE_ENCODER_DAC;
				break;
			case OUTPUT_TMDS:
				type = DRM_MODE_ENCODER_TMDS;
				break;
			case OUTPUT_LVDS:
				type = DRM_MODE_ENCODER_LVDS;
				break;
			case OUTPUT_TV:
				type = DRM_MODE_ENCODER_TVDAC;
				break;
			default:
				type = DRM_MODE_ENCODER_NONE;
				break;
		}

		if (type == DRM_MODE_ENCODER_NONE) {
			DRM_ERROR("DRM_MODE_ENCODER_NONE encountered\n");
			continue;
		}

		drm_encoder_init(dev, drm_encoder, &nv50_kms_encoder_funcs, type);

		/* I've never seen possible crtc's restricted. */
		drm_encoder->possible_crtcs = 3;
		drm_encoder->possible_clones = 0;
	}

	return 0;
}

/*
 * Connector functions
 */

void nv50_kms_connector_detect_all(struct drm_device *dev)
{
	struct drm_connector *drm_connector = NULL;

	list_for_each_entry(drm_connector, &dev->mode_config.connector_list, head) {
		drm_connector->funcs->detect(drm_connector);
	}
}

static enum drm_connector_status nv50_kms_connector_detect(struct drm_connector *drm_connector)
{
	struct nv50_connector *connector = to_nv50_connector(drm_connector);
	struct drm_device *dev = drm_connector->dev;
	bool connected;
	int old_status;

	connected = connector->detect(connector);

	old_status = drm_connector->status;

	if (connected)
		drm_connector->status = connector_status_connected;
	else
		drm_connector->status = connector_status_disconnected;

	/* update our modes whenever there is reason to */
	if (old_status != drm_connector->status) {
		drm_connector->funcs->fill_modes(drm_connector, 0, 0);
		/* notify fb of changes */
		dev->mode_config.funcs->fb_changed(dev);
	}

	return drm_connector->status;
}

static void nv50_kms_connector_destroy(struct drm_connector *drm_connector)
{
	struct nv50_connector *connector = to_nv50_connector(drm_connector);

	drm_sysfs_connector_remove(drm_connector);
	drm_connector_cleanup(drm_connector);

	/* this will even destroy the public structure. */
	connector->destroy(connector);
}

/*
 * Detailed mode info for a standard 640x480@60Hz monitor
 */
static struct drm_display_mode std_mode[] = {
	/*{ DRM_MODE("640x480", DRM_MODE_TYPE_DEFAULT, 25200, 640, 656,
		 752, 800, 0, 480, 490, 492, 525, 0,
		 V_NHSYNC | V_NVSYNC) },*/ /* 640x480@60Hz */
	{ DRM_MODE("1280x1024", DRM_MODE_TYPE_DEFAULT, 135000, 1280, 1296,
		   1440, 1688, 0, 1024, 1025, 1028, 1066, 0,
		   V_PHSYNC | V_PVSYNC) }, /* 1280x1024@75Hz */
};

static void nv50_kms_connector_fill_modes(struct drm_connector *drm_connector, uint32_t maxX, uint32_t maxY)
{
	struct nv50_connector *connector = to_nv50_connector(drm_connector);
	int ret = 0;
	bool connected;
	struct drm_display_mode *mode, *t;
	struct edid *edid = NULL;

	NV50_DEBUG("%s\n", drm_get_connector_name(drm_connector));
	/* set all modes to the unverified state */
	list_for_each_entry_safe(mode, t, &drm_connector->modes, head)
		mode->status = MODE_UNVERIFIED;

	connected = connector->detect(connector);

	if (connected)
		drm_connector->status = connector_status_connected;
	else
		drm_connector->status = connector_status_disconnected;

	if (!connected) {
		NV50_DEBUG("%s is disconnected\n", drm_get_connector_name(drm_connector));
		/* TODO set EDID to NULL */
		return;
	}

	/* Not all connnectors have an i2c channel. */
	if (connector->i2c_chan)
		edid = (struct edid *) drm_do_probe_ddc_edid(&connector->i2c_chan->adapter);

	if (edid) {
		drm_mode_connector_update_edid_property(drm_connector, edid);
		ret = drm_add_edid_modes(drm_connector, edid);
		connector->digital = edid->digital; /* cache */
		kfree(edid);
	}

	if (ret) /* number of modes  > 1 */
		drm_mode_connector_list_update(drm_connector);

	if (maxX && maxY)
		drm_mode_validate_size(drm_connector->dev, &drm_connector->modes, maxX, maxY, 0);

	list_for_each_entry_safe(mode, t, &drm_connector->modes, head) {
		if (mode->status == MODE_OK) {
			struct nouveau_hw_mode *hw_mode = nv50_kms_to_hw_mode(mode);
			struct nv50_output *output = connector->to_output(connector, connector->digital);

			mode->status = output->validate_mode(output, hw_mode);
			/* find native mode, TODO: also check if we actually found one */
			if (mode->status == MODE_OK) {
				if (mode->type & DRM_MODE_TYPE_PREFERRED)
					*output->native_mode = *hw_mode;
			}
			kfree(hw_mode);
		}
	}

	/* revalidate now that we have native mode */
	list_for_each_entry_safe(mode, t, &drm_connector->modes, head) {
		if (mode->status == MODE_OK) {
			struct nouveau_hw_mode *hw_mode = nv50_kms_to_hw_mode(mode);
			struct nv50_output *output = connector->to_output(connector, connector->digital);

			mode->status = output->validate_mode(output, hw_mode);
			kfree(hw_mode);
		}
	}

	drm_mode_prune_invalid(drm_connector->dev, &drm_connector->modes, true);

	if (list_empty(&drm_connector->modes)) {
		struct drm_display_mode *stdmode;
		struct nouveau_hw_mode *hw_mode;
		struct nv50_output *output;

		NV50_DEBUG("No valid modes on %s\n", drm_get_connector_name(drm_connector));

		/* Should we do this here ???
		 * When no valid EDID modes are available we end up
		 * here and bailed in the past, now we add a standard
		 * 640x480@60Hz mode and carry on.
		 */
		stdmode = drm_mode_duplicate(drm_connector->dev, &std_mode[0]);
		drm_mode_probed_add(drm_connector, stdmode);
		drm_mode_list_concat(&drm_connector->probed_modes,
				     &drm_connector->modes);

		/* also add it as native mode */
		hw_mode = nv50_kms_to_hw_mode(mode);
		output = connector->to_output(connector, connector->digital);

		if (hw_mode)
			*output->native_mode = *hw_mode;

		DRM_DEBUG("Adding standard 640x480 @ 60Hz to %s\n",
			  drm_get_connector_name(drm_connector));
	}

	drm_mode_sort(&drm_connector->modes);

	NV50_DEBUG("Probed modes for %s\n", drm_get_connector_name(drm_connector));

	list_for_each_entry_safe(mode, t, &drm_connector->modes, head) {
		mode->vrefresh = drm_mode_vrefresh(mode);

		/* is this needed, as it's unused by the driver? */
		drm_mode_set_crtcinfo(mode, CRTC_INTERLACE_HALVE_V);
		drm_mode_debug_printmodeline(mode);
	}
}

static bool nv50_kms_connector_set_property(struct drm_connector *connector,
					struct drm_property *property,
					uint64_t value)
{
	struct drm_device *dev = connector->dev;

	if (property == dev->mode_config.dpms_property && connector->encoder) {
		struct nv50_output *output = to_nv50_output(connector->encoder);

		if (!output->set_power_mode(output, (int) value))
			return true;
		else
			return false;
	}

	return false;
}

static const struct drm_connector_funcs nv50_kms_connector_funcs = {
	.save = NULL,
	.restore = NULL,
	.detect = nv50_kms_connector_detect,
	.destroy = nv50_kms_connector_destroy,
	.fill_modes = nv50_kms_connector_fill_modes,
	.set_property = nv50_kms_connector_set_property
};

static int nv50_kms_connectors_init(struct drm_device *dev)
{
	struct nv50_display *display = nv50_get_display(dev);
	struct nv50_connector *connector = NULL;
	int i;

	list_for_each_entry(connector, &display->connectors, item) {
		struct drm_connector *drm_connector = to_nv50_kms_connector(connector);
		uint32_t type = DRM_MODE_CONNECTOR_Unknown;

		switch (connector->type) {
			case CONNECTOR_VGA:
				type = DRM_MODE_CONNECTOR_VGA;
				break;
			case CONNECTOR_DVI_D:
				type = DRM_MODE_CONNECTOR_DVID;
				break;
			case CONNECTOR_DVI_I:
				type = DRM_MODE_CONNECTOR_DVII;
				break;
			case CONNECTOR_LVDS:
				type = DRM_MODE_CONNECTOR_LVDS;
				break;
			case CONNECTOR_TV:
				type = DRM_MODE_CONNECTOR_SVIDEO;
				break;
			default:
				type = DRM_MODE_CONNECTOR_Unknown;
				break;
		}

		if (type == DRM_MODE_CONNECTOR_Unknown) {
			DRM_ERROR("DRM_MODE_CONNECTOR_Unknown encountered\n");
			continue;
		}

		/* It should be allowed sometimes, but let's be safe for the moment. */
		drm_connector->interlace_allowed = false;
		drm_connector->doublescan_allowed = false;

		drm_connector_init(dev, drm_connector, &nv50_kms_connector_funcs, type);

		/* attach encoders, possibilities are analog + digital */
		for (i = 0; i < 2; i++) {
			struct drm_encoder *drm_encoder = NULL;
			struct nv50_output *output = connector->to_output(connector, i);
			if (!output)
				continue;

			drm_encoder = to_nv50_kms_encoder(output);
			if (!drm_encoder) {
				DRM_ERROR("No struct drm_connector to match struct nv50_output\n");
				continue;
			}

			drm_mode_connector_attach_encoder(drm_connector, drm_encoder);
		}

		drm_sysfs_connector_add(drm_connector);
	}

	return 0;
}

/*
 * Main functions
 */

int nv50_kms_init(struct drm_device *dev)
{
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	struct nv50_kms_priv *kms_priv = kzalloc(sizeof(struct nv50_kms_priv), GFP_KERNEL);
	struct nv50_display *display = NULL;
	int rval = 0;

	if (!kms_priv)
		return -ENOMEM;

	dev_priv->kms_priv = kms_priv;

	/* function pointers */
	/* an allocation interface that deals with the outside world, without polluting the core. */
	dev_priv->alloc_crtc = nv50_kms_alloc_crtc;
	dev_priv->alloc_output = nv50_kms_alloc_output;
	dev_priv->alloc_connector = nv50_kms_alloc_connector;

	dev_priv->free_crtc = nv50_kms_free_crtc;
	dev_priv->free_output = nv50_kms_free_output;
	dev_priv->free_connector = nv50_kms_free_connector;

	/* bios is needed for tables. */
	rval = nouveau_parse_bios(dev);
	if (rval != 0)
		goto out;

	/* init basic kernel modesetting */
	drm_mode_config_init(dev);

	dev->mode_config.min_width = 0;
	dev->mode_config.min_height = 0;

	dev->mode_config.funcs = (void *)&nv50_kms_mode_funcs;

	dev->mode_config.max_width = 8192;
	dev->mode_config.max_height = 8192;

	dev->mode_config.fb_base = dev_priv->fb_phys;

	/* init kms lists */
	INIT_LIST_HEAD(&kms_priv->crtcs);
	INIT_LIST_HEAD(&kms_priv->encoders);
	INIT_LIST_HEAD(&kms_priv->connectors);

	/* init the internal core, must be done first. */
	rval = nv50_display_create(dev);
	if (rval != 0)
		goto out;

	display = nv50_get_display(dev);
	if (!display) {
		rval = -EINVAL;
		goto out;
	}

	/* pre-init now */
	rval = display->pre_init(display);
	if (rval != 0)
		goto out;

	/* init external layer */
	rval = nv50_kms_crtcs_init(dev);
	if (rval != 0)
		goto out;

	rval = nv50_kms_encoders_init(dev);
	if (rval != 0)
		goto out;

	rval = nv50_kms_connectors_init(dev);
	if (rval != 0)
		goto out;

	/* init now, this'll kill the textmode */
	rval = display->init(display);
	if (rval != 0)
		goto out;

	/* process cmdbuffer */
	display->update(display);

	return 0;

out:
	kfree(kms_priv);
	dev_priv->kms_priv = NULL;

	return rval;
}

int nv50_kms_destroy(struct drm_device *dev)
{
	drm_mode_config_cleanup(dev);

	return 0;
}

