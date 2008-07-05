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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/tty.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/fb.h>
#include <linux/init.h>

#include "nv50_fbcon.h"

static int nv50_fbcon_setcolreg(unsigned regno, unsigned red, unsigned green,
			unsigned blue, unsigned transp,
			struct fb_info *info)
{
	struct nv50_fbcon_par *par = info->par;
	struct drm_device *dev = par->dev;
	struct drm_framebuffer *drm_fb; 

	list_for_each_entry(drm_fb, &dev->mode_config.fb_kernel_list, filp_head) {
		if (regno > 255)
			return 1;

		/* TODO: 8 bit support */
		if (regno < 16) {
			switch (drm_fb->depth) {
			case 15:
				drm_fb->pseudo_palette[regno] = ((red & 0xf800) >> 1) |
					((green & 0xf800) >>  6) |
					((blue & 0xf800) >> 11);
				break;
			case 16:
				drm_fb->pseudo_palette[regno] = (red & 0xf800) |
					((green & 0xfc00) >>  5) |
					((blue  & 0xf800) >> 11);
				break;
			case 24:
			case 32:
				drm_fb->pseudo_palette[regno] = ((red & 0xff00) << 8) |
					(green & 0xff00) |
					((blue  & 0xff00) >> 8);
				break;
			}
		}
	}
	return 0;
}

static int nv50_fbcon_check_var(struct fb_var_screeninfo *var, struct fb_info *info)
{
	struct nv50_fbcon_par *par = info->par;
	struct drm_framebuffer *drm_fb = par->fb;
	int depth;

	NV50_DEBUG("\n");

	if (!var || !drm_fb || !info) {
		DRM_ERROR("No var, drm_fb or info\n");
	}

	par->use_preferred_mode = false;

	if (var->pixclock == -1 || !var->pixclock) {
		DRM_INFO("Using preferred mode.\n");
		par->use_preferred_mode = true;
	}

	/* Need to resize the fb object !!! */
	if (var->xres > drm_fb->width || var->yres > drm_fb->height) {
		DRM_ERROR("Requested width/height is greater than current fb object %dx%d > %dx%d\n", var->xres,var->yres, drm_fb->width, drm_fb->height);
		DRM_ERROR("Need resizing code.\n");
		return -EINVAL;
	}

	switch (var->bits_per_pixel) {
	case 16:
		depth = (var->green.length == 6) ? 16 : 15;
		break;
	case 32:
		depth = (var->transp.length > 0) ? 32 : 24;
		break;
	default:
		depth = var->bits_per_pixel;
		break;
	}

	switch (depth) {
	case 15:
		var->red.offset = 10;
		var->green.offset = 5;
		var->blue.offset = 0;
		var->red.length = 5;
		var->green.length = 5;
		var->blue.length = 5;
		var->transp.length = 1;
		var->transp.offset = 15;
		break;
	case 16:
		var->red.offset = 11;
		var->green.offset = 5;
		var->blue.offset = 0;
		var->red.length = 5;
		var->green.length = 6;
		var->blue.length = 5;
		var->transp.length = 0;
		var->transp.offset = 0;
		break;
	case 24:
		var->red.offset = 16;
		var->green.offset = 8;
		var->blue.offset = 0;
		var->red.length = 8;
		var->green.length = 8;
		var->blue.length = 8;
		var->transp.length = 0;
		var->transp.offset = 0;
		break;
	case 32:
		var->red.offset = 16;
		var->green.offset = 8;
		var->blue.offset = 0;
		var->red.length = 8;
		var->green.length = 8;
		var->blue.length = 8;
		var->transp.length = 8;
		var->transp.offset = 24;
		break;
	default:
		DRM_ERROR("Invalid depth %d\n", depth);
		return -EINVAL; 
	}

	return 0;
}

static int nv50_fbcon_set_par(struct fb_info *info)
{
	struct nv50_fbcon_par *par;
	struct drm_framebuffer *drm_fb;
	struct drm_connector *drm_connector;
	struct drm_crtc *drm_crtc;
	struct fb_var_screeninfo *var;
	struct drm_display_mode *drm_mode = NULL, *t;
	struct drm_device *dev;
	int rval;
	bool crtc_used[2] = {false, false};

	NV50_DEBUG("\n");

	if (!info) {
		DRM_ERROR("No fb_info\n");
		return -EINVAL;
	}

	par = info->par;

	if (!par) {
		DRM_ERROR("No nv50_fbcon_par\n");
		return -EINVAL;
	}

	drm_fb = par->fb;
	var = &info->var;
	dev = par->dev;

	if (!drm_fb || !var || !dev) {
		DRM_ERROR("No drm_fb, var or dev\n");
		return -EINVAL;
	}

	par->use_preferred_mode = false;

	if (var->pixclock == -1 || !var->pixclock) {
		DRM_INFO("Using preferred mode.\n");
		par->use_preferred_mode = true;
	}

	/* FB setup */
	switch (var->bits_per_pixel) {
	case 16:
		drm_fb->depth = (var->green.length == 6) ? 16 : 15;
		break;
	case 32:
		drm_fb->depth = (var->transp.length > 0) ? 32 : 24;
		break;
	default:
		drm_fb->depth = var->bits_per_pixel;
		break;
	}

	drm_fb->bits_per_pixel = var->bits_per_pixel;

	info->fix.line_length = drm_fb->pitch;
	info->fix.smem_len = info->fix.line_length * drm_fb->height;
	/* ignoring 8bpp for the moment */
	info->fix.visual = FB_VISUAL_TRUECOLOR;

	info->screen_size = info->fix.smem_len; /* ??? */

	/* create a drm mode */
	if (!par->use_preferred_mode) {
		drm_mode = drm_mode_create(dev);
		drm_mode->hdisplay = var->xres;
		drm_mode->hsync_start = drm_mode->hdisplay + var->right_margin;
		drm_mode->hsync_end = drm_mode->hsync_start + var->hsync_len;
		drm_mode->htotal = drm_mode->hsync_end + var->left_margin;
		drm_mode->vdisplay = var->yres;
		drm_mode->vsync_start = drm_mode->vdisplay + var->lower_margin;
		drm_mode->vsync_end = drm_mode->vsync_start + var->vsync_len;
		drm_mode->vtotal = drm_mode->vsync_end + var->upper_margin;
		drm_mode->clock = PICOS2KHZ(var->pixclock);
		drm_mode->vrefresh = drm_mode_vrefresh(drm_mode);
		drm_mode->flags = 0;
		drm_mode->flags |= var->sync & FB_SYNC_HOR_HIGH_ACT ? V_PHSYNC : V_NHSYNC;
		drm_mode->flags |= var->sync & FB_SYNC_VERT_HIGH_ACT ? V_PVSYNC : V_NVSYNC;

		drm_mode_set_name(drm_mode);
		drm_mode_set_crtcinfo(drm_mode, CRTC_INTERLACE_HALVE_V);
	}

	/* hook up crtc's */
	list_for_each_entry(drm_connector, &dev->mode_config.connector_list, head) {
		enum drm_connector_status status;
		struct drm_mode_set mode_set;
		int crtc_count = 0;

		status = drm_connector->funcs->detect(drm_connector);

		if (status != connector_status_connected)
			continue;

		memset(&mode_set, 0, sizeof(struct drm_mode_set));

		/* set connector */
		mode_set.num_connectors = 1;
		mode_set.connectors = kzalloc(sizeof(struct drm_connector *), GFP_KERNEL);
		if (!mode_set.connectors) {
			rval = -ENOMEM;
			goto out;
		}
		mode_set.connectors[0] = drm_connector;

		/* set fb */
		list_for_each_entry(drm_fb, &dev->mode_config.fb_kernel_list, filp_head) {
			break; /* first entry is the only entry */
		}
		mode_set.fb = drm_fb;

		/* set mode */
		if (par->use_preferred_mode) {
			/* find preferred mode */
			list_for_each_entry_safe(drm_mode, t, &drm_connector->modes, head) {
				if (drm_mode->type & DRM_MODE_TYPE_PREFERRED)
					break;
			}
		}
		mode_set.mode = drm_mode;

		/* choose crtc it already has, if possible */
		if (drm_connector->encoder) {
			struct drm_encoder *drm_encoder = drm_connector->encoder;

			if (drm_encoder->crtc) {
				list_for_each_entry(drm_crtc, &dev->mode_config.crtc_list, head) {
					crtc_count++;

					if (drm_crtc == drm_encoder->crtc) {
						if (!crtc_used[crtc_count]) /* still available? */
							mode_set.crtc = drm_crtc;
						break;
					}
				}
			}
		}

		/* proceed as planned */
		if (mode_set.crtc) {
			mode_set.crtc->funcs->set_config(&mode_set);
			crtc_used[crtc_count] = true;
		}

		if (!mode_set.crtc) {
			crtc_count = 0; /* reset */

			/* choose a "random" crtc */
			list_for_each_entry(drm_crtc, &dev->mode_config.crtc_list, head) {
				if (crtc_used[crtc_count]) {
					crtc_count++;
					continue;
				}

				/* found a crtc */
				mode_set.crtc = drm_crtc;

				break;
			}

			/* proceed as planned */
			if (mode_set.crtc) {
				mode_set.crtc->funcs->set_config(&mode_set);
				crtc_used[crtc_count] = true;
			}
		}

		kfree(mode_set.connectors);
	}

	return 0;

out:
	return rval;
}

static struct fb_ops nv50_fb_ops = {
	.owner = THIS_MODULE,
	//.fb_open = nv50_fb_open,
	//.fb_read = nv50_fb_read,
	//.fb_write = nv50_fb_write,
	//.fb_release = nv50_fb_release,
	//.fb_ioctl = nv50_fb_ioctl,
	.fb_check_var = nv50_fbcon_check_var,
	.fb_set_par = nv50_fbcon_set_par,
	.fb_setcolreg = nv50_fbcon_setcolreg,
	.fb_fillrect = cfb_fillrect,
	.fb_copyarea = cfb_copyarea,
	.fb_imageblit = cfb_imageblit,
	//.fb_pan_display = nv50_fb_pan_display,
};

static int nv50_fbcon_initial_config(struct drm_device *dev)
{
	struct drm_connector *drm_connector;

	struct drm_framebuffer *drm_fb = NULL;
	struct drm_mode_fb_cmd drm_fb_cmd;
	enum drm_connector_status status;
	uint32_t max_width = 0, max_height = 0, pitch = 0;
	struct mem_block *block;
	struct drm_file *file_priv;
	uint32_t flags;
	int rval = 0;

	list_for_each_entry(drm_connector, &dev->mode_config.connector_list, head) {
		status = drm_connector->funcs->detect(drm_connector);

		/* find the framebuffer size */
		if (status == connector_status_connected) {
			struct drm_display_mode *mode, *t;
			list_for_each_entry_safe(mode, t, &drm_connector->modes, head) {
				if (mode->type & DRM_MODE_TYPE_PREFERRED) {
					if (mode->hdisplay > max_width)
						max_width = mode->hdisplay;
					if (mode->vdisplay > max_height)
						max_height = mode->vdisplay;
				}
			}
		}
	}

	/* allocate framebuffer */
	file_priv = kzalloc(sizeof(struct drm_file), GFP_KERNEL);
	if (!file_priv) {
		rval = -ENOMEM;
		goto out;
	}

	pitch = (max_width + 63) & ~63;
	pitch *= 4; /* TODO */

	flags = NOUVEAU_MEM_FB | NOUVEAU_MEM_MAPPED;

	/* Any file_priv should do as it's pointer is used as identification. */
	block = nouveau_mem_alloc(dev, 0, pitch * max_height, flags, file_priv);
	if (!block) {
		rval = -ENOMEM;
		goto out;
	}

	memset(&drm_fb_cmd, 0, sizeof(struct drm_mode_fb_cmd));

	drm_fb_cmd.width = max_width;
	drm_fb_cmd.height = max_height;
	drm_fb_cmd.pitch = pitch;
	drm_fb_cmd.bpp = 32; /* TODO */
	drm_fb_cmd.handle = block->map_handle;
	drm_fb_cmd.depth = 24; /* TODO */

	drm_fb = dev->mode_config.funcs->fb_create(dev, file_priv, &drm_fb_cmd);
	if (!drm_fb) {
		rval = -EINVAL;
		goto out;
	}

	list_add(&drm_fb->filp_head, &dev->mode_config.fb_kernel_list);

	return 0;

out:
	if (file_priv)
		kfree(file_priv);
	if (drm_fb)
		drm_fb->funcs->destroy(drm_fb);

	return rval;
}

/*
 * Single framebuffer, ideally operating in clone mode across various connectors.
 */
int nv50_fbcon_init(struct drm_device *dev)
{
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	struct fb_info *info;
	struct nv50_fbcon_par *par;
	struct device *device = &dev->pdev->dev;
	struct drm_framebuffer *drm_fb;
	struct mem_block *block;
	void __iomem *fb = NULL;
	int rval;

	rval = nv50_fbcon_initial_config(dev);
	if (rval != 0) {
		DRM_ERROR("nv50_fbcon_initial_config failed\n");
		return rval;
	}

	list_for_each_entry(drm_fb, &dev->mode_config.fb_kernel_list, filp_head) {
		break; /* first entry is the only entry */
	}

	if (!drm_fb) {
		DRM_ERROR("no drm_fb found\n");
		return -EINVAL;
	}

	block = find_block_by_handle(dev_priv->fb_heap, drm_fb->mm_handle);
	if (!block) {
		DRM_ERROR("can't find mem_block\n");
		return -EINVAL;
	}

	info = framebuffer_alloc(sizeof(struct nv50_fbcon_par), device);
	if (!info) {
		DRM_ERROR("framebuffer_alloc failed\n");
		return -EINVAL;
	}

	par = info->par;

	strcpy(info->fix.id, "nv50drmfb");
	info->fix.type = FB_TYPE_PACKED_PIXELS;
	info->fix.visual = FB_VISUAL_TRUECOLOR;
	info->fix.type_aux = 0;
	info->fix.xpanstep = 0; /* 1 is doing it in hw */
	info->fix.ypanstep = 0;
	info->fix.ywrapstep = 0;
	info->fix.accel = FB_ACCEL_NONE;
	info->fix.type_aux = 0;

	info->flags = FBINFO_DEFAULT;

	info->fbops = &nv50_fb_ops;

	info->fix.line_length = drm_fb->pitch;
	info->fix.smem_start = dev_priv->fb_phys + block->start;
	info->fix.smem_len = info->fix.line_length * drm_fb->height;

	info->flags = FBINFO_DEFAULT;

	fb = ioremap(dev_priv->fb_phys + block->start, block->size);
	if (!fb) {
		DRM_ERROR("Unable to ioremap framebuffer\n");
		return -EINVAL;
	}

 	info->screen_base = fb;
	info->screen_size = info->fix.smem_len; /* FIXME */

	info->pseudo_palette = drm_fb->pseudo_palette;
	info->var.xres_virtual = drm_fb->width;
	info->var.yres_virtual = drm_fb->height;
	info->var.bits_per_pixel = drm_fb->bits_per_pixel;
	info->var.xoffset = 0;
	info->var.yoffset = 0;
	info->var.activate = FB_ACTIVATE_NOW;
	info->var.height = -1;
	info->var.width = -1;

	/* TODO: improve this */
	info->var.xres = drm_fb->width;
	info->var.yres = drm_fb->height;

	info->fix.mmio_start = drm_get_resource_start(dev, 0);
	info->fix.mmio_len = drm_get_resource_len(dev, 0);

	DRM_DEBUG("fb depth is %d\n", drm_fb->depth);
	DRM_DEBUG("   pitch is %d\n", drm_fb->pitch);

	switch(drm_fb->depth) {
 	case 15:
		info->var.red.offset = 10;
		info->var.green.offset = 5;
		info->var.blue.offset = 0;
		info->var.red.length = 5;
		info->var.green.length = 5;
		info->var.blue.length = 5;
		info->var.transp.offset = 15;
		info->var.transp.length = 1;
		break;
	case 16:
		info->var.red.offset = 11;
		info->var.green.offset = 5;
		info->var.blue.offset = 0;
		info->var.red.length = 5;
		info->var.green.length = 6;
		info->var.blue.length = 5;
		info->var.transp.offset = 0;
 		break;
	case 24:
		info->var.red.offset = 16;
		info->var.green.offset = 8;
		info->var.blue.offset = 0;
		info->var.red.length = 8;
		info->var.green.length = 8;
		info->var.blue.length = 8;
		info->var.transp.offset = 0;
		info->var.transp.length = 0;
		break;
	case 32:
		info->var.red.offset = 16;
		info->var.green.offset = 8;
		info->var.blue.offset = 0;
		info->var.red.length = 8;
		info->var.green.length = 8;
		info->var.blue.length = 8;
		info->var.transp.offset = 24;
		info->var.transp.length = 8;
		break;
	default:
		break;
	}

	drm_fb->fbdev = info;
	par->dev = dev;
	par->fb = drm_fb;

	register_framebuffer(info);

	DRM_INFO("nv50drmfb initialised\n");

	return 0;
}

int nv50_fbcon_destroy(struct drm_device *dev)
{
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	struct drm_framebuffer *drm_fb;
	struct fb_info *info;
	struct mem_block *block;
	struct drm_file *file_priv;

	list_for_each_entry(drm_fb, &dev->mode_config.fb_kernel_list, filp_head) {
		break; /* first entry is the only entry */
	}

	if (!drm_fb) {
		DRM_ERROR("No framebuffer to destroy\n");
		return -EINVAL;
	}

	info = drm_fb->fbdev;
	if (!info) {
		DRM_ERROR("No fb_info\n");
		return -EINVAL;
	}

	unregister_framebuffer(info);

	block = find_block_by_handle(dev_priv->fb_heap, drm_fb->mm_handle);
	if (!block) {
		DRM_ERROR("can't find mem_block\n");
		return -EINVAL;
	}

	/* we need to free this after memory is freed */
	file_priv = block->file_priv;

	/* free memory */
	nouveau_mem_free(dev, block);

	if (file_priv) {
		kfree(file_priv);
		file_priv = NULL;
	}

	framebuffer_release(info);

	return 0;
}
