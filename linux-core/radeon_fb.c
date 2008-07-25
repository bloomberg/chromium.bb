/*
 * Copyright © 2007 David Airlie
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *     David Airlie
 */
    /*
     *  Modularization
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

#include "drmP.h"
#include "drm.h"
#include "drm_crtc.h"
#include "drm_crtc_helper.h"
#include "radeon_drm.h"
#include "radeon_drv.h"

struct radeonfb_par {
	struct drm_device *dev;
	struct drm_display_mode *our_mode;
	struct radeon_framebuffer *radeon_fb;
	int crtc_count;
	/* crtc currently bound to this */
	uint32_t crtc_ids[2];
};
/*
static int
var_to_refresh(const struct fb_var_screeninfo *var)
{
	int xtot = var->xres + var->left_margin + var->right_margin +
		var->hsync_len;
	int ytot = var->yres + var->upper_margin + var->lower_margin +
		var->vsync_len;

	return (1000000000 / var->pixclock * 1000 + 500) / xtot / ytot;
}*/

static int radeonfb_setcolreg(unsigned regno, unsigned red, unsigned green,
			unsigned blue, unsigned transp,
			struct fb_info *info)
{
	struct radeonfb_par *par = info->par;
	struct drm_device *dev = par->dev;
	struct drm_crtc *crtc;
	int i;

	list_for_each_entry(crtc, &dev->mode_config.crtc_list, head) {
		struct radeon_crtc *radeon_crtc = to_radeon_crtc(crtc);
		struct drm_mode_set *modeset = &radeon_crtc->mode_set;
		struct drm_framebuffer *fb = modeset->fb;

		for (i = 0; i < par->crtc_count; i++)
			if (crtc->base.id == par->crtc_ids[i])
				break;

		if (i == par->crtc_count)
			continue;
		

		if (regno > 255)
			return 1;

		if (fb->depth == 8) {
			radeon_crtc_fb_gamma_set(crtc, red, green, blue, regno);
			return 0;
		}

		if (regno < 16) {
			switch (fb->depth) {
			case 15:
				fb->pseudo_palette[regno] = ((red & 0xf800) >> 1) |
					((green & 0xf800) >>  6) |
					((blue & 0xf800) >> 11);
				break;
			case 16:
				fb->pseudo_palette[regno] = (red & 0xf800) |
					((green & 0xfc00) >>  5) |
					((blue  & 0xf800) >> 11);
				break;
			case 24:
			case 32:
				fb->pseudo_palette[regno] = ((red & 0xff00) << 8) |
					(green & 0xff00) |
					((blue  & 0xff00) >> 8);
				break;
			}
		}
	}
	return 0;
}

static int radeonfb_check_var(struct fb_var_screeninfo *var,
			struct fb_info *info)
{
	struct radeonfb_par *par = info->par;
	struct radeon_framebuffer *radeon_fb = par->radeon_fb;
	struct drm_framebuffer *fb = &radeon_fb->base;
	int depth;

	if (var->pixclock == -1 || !var->pixclock)
		return -EINVAL;

	/* Need to resize the fb object !!! */
	if (var->xres > fb->width || var->yres > fb->height) {
		DRM_ERROR("Requested width/height is greater than current fb object %dx%d > %dx%d\n",var->xres,var->yres,fb->width,fb->height);
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
	case 8:
		var->red.offset = 0;
		var->green.offset = 0;
		var->blue.offset = 0;
		var->red.length = 8;
		var->green.length = 8;
		var->blue.length = 8;
		var->transp.length = 0;
		var->transp.offset = 0;
		break;
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
		return -EINVAL; 
	}

	return 0;
}

/* this will let fbcon do the mode init */
/* FIXME: take mode config lock? */
static int radeonfb_set_par(struct fb_info *info)
{
	struct radeonfb_par *par = info->par;
	struct drm_device *dev = par->dev;
	struct fb_var_screeninfo *var = &info->var;
	int i;

	DRM_DEBUG("%d %d\n", var->xres, var->pixclock);

	if (var->pixclock != -1) {

		DRM_ERROR("PIXEL CLCOK SET\n");
#if 0
		struct radeon_framebuffer *radeon_fb = par->radeon_fb;
		struct drm_framebuffer *fb = &radeon_fb->base;
		struct drm_display_mode *drm_mode, *search_mode;
		struct drm_connector *connector = NULL;
		struct drm_device *dev = par->dev;

		int found = 0;

		switch (var->bits_per_pixel) {
		case 16:
			fb->depth = (var->green.length == 6) ? 16 : 15;
			break;
		case 32:
			fb->depth = (var->transp.length > 0) ? 32 : 24;
			break;
		default:
			fb->depth = var->bits_per_pixel;
			break;
		}
		
		fb->bits_per_pixel = var->bits_per_pixel;
		
		info->fix.line_length = fb->pitch;
		info->fix.smem_len = info->fix.line_length * fb->height;
		info->fix.visual = (fb->depth == 8) ? FB_VISUAL_PSEUDOCOLOR : FB_VISUAL_TRUECOLOR;
		
		info->screen_size = info->fix.smem_len; /* ??? */
		/* reuse desired mode if possible */
		/* create a drm mode */
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
		drm_mode->flags |= var->sync & FB_SYNC_HOR_HIGH_ACT ? DRM_MODE_FLAG_PHSYNC : DRM_MODE_FLAG_NHSYNC;
		drm_mode->flags |= var->sync & FB_SYNC_VERT_HIGH_ACT ? DRM_MODE_FLAG_PVSYNC : DRM_MODE_FLAG_NVSYNC;
		
		drm_mode_set_name(drm_mode);
		drm_mode_set_crtcinfo(drm_mode, CRTC_INTERLACE_HALVE_V);
		
		found = 0;
		list_for_each_entry(connector, &dev->mode_config.connector_list, head) {
			if (connector->encoder &&
			    connector->encoder->crtc == par->set.crtc){
				found = 1;
				break;
			}
		}
		
		/* no connector bound, bail */
		if (!found)
			return -EINVAL;
		
		found = 0;
		drm_mode_debug_printmodeline(drm_mode);
		list_for_each_entry(search_mode, &connector->modes, head) {
			drm_mode_debug_printmodeline(search_mode);
			if (drm_mode_equal(drm_mode, search_mode)) {
				drm_mode_destroy(dev, drm_mode);
				drm_mode = search_mode;
				found = 1;
				break;
			}
		}
		
		/* If we didn't find a matching mode that exists on our connector,
		 * create a new attachment for the incoming user specified mode
		 */
		if (!found) {
			if (par->our_mode) {
				/* this also destroys the mode */
				drm_mode_detachmode_crtc(dev, par->our_mode);
			}
			
			par->set.mode = drm_mode;
			par->our_mode = drm_mode;
			drm_mode_debug_printmodeline(drm_mode);
			/* attach mode */
			drm_mode_attachmode_crtc(dev, par->set.crtc, par->set.mode);
		} else {
			par->set.mode = drm_mode;
			if (par->our_mode)
				drm_mode_detachmode_crtc(dev, par->our_mode);
			par->our_mode = NULL;
		}
		return par->set.crtc->funcs->set_config(&par->set);
#endif
		return -EINVAL;
	} else {
		struct drm_crtc *crtc;
		int ret;

		list_for_each_entry(crtc, &dev->mode_config.crtc_list, head) {
			struct radeon_crtc *radeon_crtc = to_radeon_crtc(crtc);

			for (i = 0; i < par->crtc_count; i++)
				if (crtc->base.id == par->crtc_ids[i])
					break;

			if (i == par->crtc_count)
				continue;

			if (crtc->fb == radeon_crtc->mode_set.fb) {
				ret = crtc->funcs->set_config(&radeon_crtc->mode_set);
				if (ret)
					return ret;
			}
		}
		return 0;
	}
}

#if 0
static void radeonfb_copyarea(struct fb_info *info,
			const struct fb_copyarea *region)
{
	struct radeonfb_par *par = info->par;
	struct drm_device *dev = par->dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	u32 src_x1, src_y1, dst_x1, dst_y1, dst_x2, dst_y2, offset;
	u32 cmd, rop_depth_pitch, src_pitch;
	RING_LOCALS;

	cmd = XY_SRC_COPY_BLT_CMD;
	src_x1 = region->sx;
	src_y1 = region->sy;
	dst_x1 = region->dx;
	dst_y1 = region->dy;
	dst_x2 = region->dx + region->width;
	dst_y2 = region->dy + region->height;
	offset = par->fb->offset;
	rop_depth_pitch = BLT_ROP_GXCOPY | par->fb->pitch;
	src_pitch = par->fb->pitch;

	switch (par->fb->bits_per_pixel) {
	case 16:
		rop_depth_pitch |= BLT_DEPTH_16_565;
		break;
	case 32:
		rop_depth_pitch |= BLT_DEPTH_32;
		cmd |= XY_SRC_COPY_BLT_WRITE_ALPHA | XY_SRC_COPY_BLT_WRITE_RGB;
		break;
	}

	BEGIN_LP_RING(8);
	OUT_RING(cmd);
	OUT_RING(rop_depth_pitch);
	OUT_RING((dst_y1 << 16) | (dst_x1 & 0xffff));
	OUT_RING((dst_y2 << 16) | (dst_x2 & 0xffff));
	OUT_RING(offset);
	OUT_RING((src_y1 << 16) | (src_x1 & 0xffff));
	OUT_RING(src_pitch);
	OUT_RING(offset);
	ADVANCE_LP_RING();
}

#define ROUND_UP_TO(x, y)	(((x) + (y) - 1) / (y) * (y))
#define ROUND_DOWN_TO(x, y)	((x) / (y) * (y))

void radeonfb_imageblit(struct fb_info *info, const struct fb_image *image)
{
	struct radeonfb_par *par = info->par;
	struct drm_device *dev = par->dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	u32 cmd, rop_pitch_depth, tmp;
	int nbytes, ndwords, pad;
	u32 dst_x1, dst_y1, dst_x2, dst_y2, offset, bg, fg;
	int dat, ix, iy, iw;
	int i, j;
	RING_LOCALS;

	/* size in bytes of a padded scanline */
	nbytes = ROUND_UP_TO(image->width, 16) / 8;

	/* Total bytes of padded scanline data to write out. */
	nbytes *= image->height;

	/*
	* Check if the glyph data exceeds the immediate mode limit.
	* It would take a large font (1K pixels) to hit this limit.
	*/
	if (nbytes > 128 || image->depth != 1)
		return cfb_imageblit(info, image);

	/* Src data is packaged a dword (32-bit) at a time. */
	ndwords = ROUND_UP_TO(nbytes, 4) / 4;

	/*
	* Ring has to be padded to a quad word. But because the command starts
	with 7 bytes, pad only if there is an even number of ndwords
	*/
	pad = !(ndwords % 2);

	DRM_DEBUG("imageblit %dx%dx%d to (%d,%d)\n", image->width,
		image->height, image->depth, image->dx, image->dy);
	DRM_DEBUG("nbytes: %d, ndwords: %d, pad: %d\n", nbytes, ndwords, pad);

	tmp = (XY_MONO_SRC_COPY_IMM_BLT & 0xff) + ndwords;
	cmd = (XY_MONO_SRC_COPY_IMM_BLT & ~0xff) | tmp;
	offset = par->fb->offset;
	dst_x1 = image->dx;
	dst_y1 = image->dy;
	dst_x2 = image->dx + image->width;
	dst_y2 = image->dy + image->height;
	rop_pitch_depth = BLT_ROP_GXCOPY | par->fb->pitch;

	switch (par->fb->bits_per_pixel) {
	case 8:
		rop_pitch_depth |= BLT_DEPTH_8;
		fg = image->fg_color;
		bg = image->bg_color;
		break;
	case 16:
		rop_pitch_depth |= BLT_DEPTH_16_565;
		fg = par->fb->pseudo_palette[image->fg_color];
		bg = par->fb->pseudo_palette[image->bg_color];
		break;
	case 32:
		rop_pitch_depth |= BLT_DEPTH_32;
		cmd |= XY_SRC_COPY_BLT_WRITE_ALPHA | XY_SRC_COPY_BLT_WRITE_RGB;
		fg = par->fb->pseudo_palette[image->fg_color];
		bg = par->fb->pseudo_palette[image->bg_color];
		break;
	default:
		DRM_ERROR("unknown depth %d\n", par->fb->bits_per_pixel);
		break;
	}
	
	BEGIN_LP_RING(8 + ndwords);
	OUT_RING(cmd);
	OUT_RING(rop_pitch_depth);
	OUT_RING((dst_y1 << 16) | (dst_x1 & 0xffff));
	OUT_RING((dst_y2 << 16) | (dst_x2 & 0xffff));
	OUT_RING(offset);
	OUT_RING(bg);
	OUT_RING(fg);
	ix = iy = 0;
	iw = ROUND_UP_TO(image->width, 8) / 8;
	while (ndwords--) {
		dat = 0;
		for (j = 0; j < 2; ++j) {
			for (i = 0; i < 2; ++i) {
				if (ix != iw || i == 0)
					dat |= image->data[iy*iw + ix++] << (i+j*2)*8;
			}
			if (ix == iw && iy != (image->height - 1)) {
				ix = 0;
				++iy;
			}
		}
		OUT_RING(dat);
	}
	if (pad)
		OUT_RING(MI_NOOP);
	ADVANCE_LP_RING();
}
#endif
static int radeonfb_pan_display(struct fb_var_screeninfo *var,
				struct fb_info *info)
{
	struct radeonfb_par *par = info->par;
	struct drm_device *dev = par->dev;
	struct drm_mode_set *modeset;
	struct drm_crtc *crtc;
	struct radeon_crtc *radeon_crtc;
	int ret = 0;
	int i;

	list_for_each_entry(crtc, &dev->mode_config.crtc_list, head) {
		
		for (i = 0; i < par->crtc_count; i++)
			if (crtc->base.id == par->crtc_ids[i])
				break;

		if (i == par->crtc_count)
			continue;

		radeon_crtc = to_radeon_crtc(crtc);
		modeset = &radeon_crtc->mode_set;

		modeset->x = var->xoffset;
		modeset->y = var->yoffset;

		if (modeset->num_connectors) {
			ret = crtc->funcs->set_config(modeset);
		  
			if (!ret) {
				info->var.xoffset = var->xoffset;
				info->var.yoffset = var->yoffset;
			}
		}
	}

	return ret;
}

static void radeonfb_on(struct fb_info *info)
{
	struct radeonfb_par *par = info->par;
	struct drm_device *dev = par->dev;
	struct drm_crtc *crtc;
	struct drm_encoder *encoder;
	int i;

	/*
	 * For each CRTC in this fb, find all associated encoders
	 * and turn them off, then turn off the CRTC.
	 */
	list_for_each_entry(crtc, &dev->mode_config.crtc_list, head) {
		struct drm_crtc_helper_funcs *crtc_funcs = crtc->helper_private;

		for (i = 0; i < par->crtc_count; i++)
			if (crtc->base.id == par->crtc_ids[i])
				break;

		crtc_funcs->dpms(crtc, DRM_MODE_DPMS_ON);

		/* Found a CRTC on this fb, now find encoders */
		list_for_each_entry(encoder, &dev->mode_config.encoder_list, head) {
			if (encoder->crtc == crtc) {
				struct drm_encoder_helper_funcs *encoder_funcs;
				encoder_funcs = encoder->helper_private;
				encoder_funcs->dpms(encoder, DRM_MODE_DPMS_ON);
			}
		}
	}
}

static void radeonfb_off(struct fb_info *info, int dpms_mode)
{
	struct radeonfb_par *par = info->par;
	struct drm_device *dev = par->dev;
	struct drm_crtc *crtc;
	struct drm_encoder *encoder;
	int i;

	/*
	 * For each CRTC in this fb, find all associated encoders
	 * and turn them off, then turn off the CRTC.
	 */
	list_for_each_entry(crtc, &dev->mode_config.crtc_list, head) {
		struct drm_crtc_helper_funcs *crtc_funcs = crtc->helper_private;

		for (i = 0; i < par->crtc_count; i++)
			if (crtc->base.id == par->crtc_ids[i])
				break;

		/* Found a CRTC on this fb, now find encoders */
		list_for_each_entry(encoder, &dev->mode_config.encoder_list, head) {
			if (encoder->crtc == crtc) {
				struct drm_encoder_helper_funcs *encoder_funcs;
				encoder_funcs = encoder->helper_private;
				encoder_funcs->dpms(encoder, dpms_mode);
			}
		}
		if (dpms_mode == DRM_MODE_DPMS_OFF)
			crtc_funcs->dpms(crtc, dpms_mode);
	}
}

int radeonfb_blank(int blank, struct fb_info *info)
{
	switch (blank) {
	case FB_BLANK_UNBLANK:
		radeonfb_on(info);
		break;
	case FB_BLANK_NORMAL:
		radeonfb_off(info, DRM_MODE_DPMS_STANDBY);
		break;
	case FB_BLANK_HSYNC_SUSPEND:
		radeonfb_off(info, DRM_MODE_DPMS_STANDBY);
		break;
	case FB_BLANK_VSYNC_SUSPEND:
		radeonfb_off(info, DRM_MODE_DPMS_SUSPEND);
		break;
	case FB_BLANK_POWERDOWN:
		radeonfb_off(info, DRM_MODE_DPMS_OFF);
		break;
	}
	return 0;
}

static struct fb_ops radeonfb_ops = {
	.owner = THIS_MODULE,
	//.fb_open = radeonfb_open,
	//.fb_read = radeonfb_read,
	//.fb_write = radeonfb_write,
	//.fb_release = radeonfb_release,
	//.fb_ioctl = radeonfb_ioctl,
	.fb_check_var = radeonfb_check_var,
	.fb_set_par = radeonfb_set_par,
	.fb_setcolreg = radeonfb_setcolreg,
	.fb_fillrect = cfb_fillrect,
	.fb_copyarea = cfb_copyarea, //radeonfb_copyarea,
	.fb_imageblit = cfb_imageblit, //radeonfb_imageblit,
	.fb_pan_display = radeonfb_pan_display,
	.fb_blank = radeonfb_blank,
};

/**
 * Curretly it is assumed that the old framebuffer is reused.
 *
 * LOCKING
 * caller should hold the mode config lock.
 *
 */
int radeonfb_resize(struct drm_device *dev, struct drm_crtc *crtc)
{
	struct fb_info *info;
	struct drm_framebuffer *fb;
	struct drm_display_mode *mode = crtc->desired_mode;

	fb = crtc->fb;
	if (!fb)
		return 1;

	info = fb->fbdev;
	if (!info)
		return 1;

	if (!mode)
		return 1;

	info->var.xres = mode->hdisplay;
	info->var.right_margin = mode->hsync_start - mode->hdisplay;
	info->var.hsync_len = mode->hsync_end - mode->hsync_start;
	info->var.left_margin = mode->htotal - mode->hsync_end;
	info->var.yres = mode->vdisplay;
	info->var.lower_margin = mode->vsync_start - mode->vdisplay;
	info->var.vsync_len = mode->vsync_end - mode->vsync_start;
	info->var.upper_margin = mode->vtotal - mode->vsync_end;
	info->var.pixclock = 10000000 / mode->htotal * 1000 / mode->vtotal * 100;
	/* avoid overflow */
	info->var.pixclock = info->var.pixclock * 1000 / mode->vrefresh;

	return 0;
}
EXPORT_SYMBOL(radeonfb_resize);

static struct drm_mode_set panic_mode;

int radeonfb_panic(struct notifier_block *n, unsigned long ununsed,
		  void *panic_str)
{
	DRM_ERROR("panic occurred, switching back to text console\n");
	drm_crtc_helper_set_config(&panic_mode);

	return 0;
}
EXPORT_SYMBOL(radeonfb_panic);
 
static struct notifier_block paniced = {
	.notifier_call = radeonfb_panic,
};

static int radeon_align_pitch(struct drm_device *dev, int width, int bpp)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;
	int aligned = width;
	int align_large = (radeon_is_avivo(dev_priv));
	int pitch_mask = 0;

	switch(bpp / 8) {
	case 1: pitch_mask = align_large ? 255 : 127; break;
	case 2: pitch_mask = align_large ? 127 : 31; break;
	case 3: 
	case 4: pitch_mask = align_large ? 63 : 15; break;
	}

	aligned += pitch_mask;
	aligned &= ~pitch_mask;
	return aligned;
}

int radeonfb_create(struct drm_device *dev, uint32_t fb_width, uint32_t fb_height, 
		   uint32_t surface_width, uint32_t surface_height,
		   struct radeon_framebuffer **radeon_fb_p)
{
	struct fb_info *info;
	struct radeonfb_par *par;
	struct drm_framebuffer *fb;
	struct radeon_framebuffer *radeon_fb;
	struct drm_mode_fb_cmd mode_cmd;
	struct drm_gem_object *fbo = NULL;
	struct drm_radeon_gem_object *obj_priv;
	struct device *device = &dev->pdev->dev; 
	int size, aligned_size, ret;

	mode_cmd.width = surface_width;/* crtc->desired_mode->hdisplay; */
	mode_cmd.height = surface_height;/* crtc->desired_mode->vdisplay; */
	
	mode_cmd.bpp = 32;
	/* need to align pitch with crtc limits */
	mode_cmd.pitch = radeon_align_pitch(dev, mode_cmd.width, mode_cmd.bpp) * ((mode_cmd.bpp + 1) / 8);
	mode_cmd.depth = 24;

	size = mode_cmd.pitch * mode_cmd.height;
	aligned_size = ALIGN(size, PAGE_SIZE);

	fbo = radeon_gem_object_alloc(dev, aligned_size, 1, RADEON_GEM_DOMAIN_VRAM);
	if (!fbo) {
		printk(KERN_ERR "failed to allocate framebuffer\n");
		ret = -ENOMEM;
		goto out;
	}
	obj_priv = fbo->driver_private;

	ret = radeon_gem_object_pin(fbo, PAGE_SIZE);
	if (ret) {
		DRM_ERROR("failed to pin fb: %d\n", ret);
		mutex_lock(&dev->struct_mutex);
		goto out_unref;
	}

	mutex_lock(&dev->struct_mutex);
	fb = radeon_user_framebuffer_create(dev, NULL, &mode_cmd);
	if (!fb) {
		DRM_ERROR("failed to allocate fb.\n");
		ret = -ENOMEM;
		goto out_unref;
	}

	list_add(&fb->filp_head, &dev->mode_config.fb_kernel_list);

	radeon_fb = to_radeon_framebuffer(fb);
	*radeon_fb_p = radeon_fb;

	radeon_fb->obj = fbo;

	info = framebuffer_alloc(sizeof(struct radeonfb_par), device);
	if (!info) {
		ret = -ENOMEM;
		goto out_unref;
	}

	par = info->par;

	strcpy(info->fix.id, "radeondrmfb");
	info->fix.type = FB_TYPE_PACKED_PIXELS;
	info->fix.visual = FB_VISUAL_TRUECOLOR;
	info->fix.type_aux = 0;
	info->fix.xpanstep = 1; /* doing it in hw */
	info->fix.ypanstep = 1; /* doing it in hw */
	info->fix.ywrapstep = 0;
	info->fix.accel = FB_ACCEL_I830;
	info->fix.type_aux = 0;

	info->flags = FBINFO_DEFAULT;

	info->fbops = &radeonfb_ops;

	info->fix.line_length = fb->pitch;
	info->fix.smem_start = dev->mode_config.fb_base + obj_priv->bo->offset;
	info->fix.smem_len = size;

	info->flags = FBINFO_DEFAULT;

	ret = drm_bo_kmap(obj_priv->bo, 0, PAGE_ALIGN(size) >> PAGE_SHIFT,
			  &radeon_fb->kmap_obj);
	info->screen_base = radeon_fb->kmap_obj.virtual;
	if (!info->screen_base) {
		ret = -ENOSPC;
		goto out_unref;
	}
	info->screen_size = size;

	memset(info->screen_base, 0, size);

	info->pseudo_palette = fb->pseudo_palette;
	info->var.xres_virtual = fb->width;
	info->var.yres_virtual = fb->height;
	info->var.bits_per_pixel = fb->bits_per_pixel;
	info->var.xoffset = 0;
	info->var.yoffset = 0;
	info->var.activate = FB_ACTIVATE_NOW;
	info->var.height = -1;
	info->var.width = -1;

	info->var.xres = fb_width;
	info->var.yres = fb_height;

	info->fix.mmio_start = pci_resource_start(dev->pdev, 2);
	info->fix.mmio_len = pci_resource_len(dev->pdev, 2);

	info->pixmap.size = 64*1024;
	info->pixmap.buf_align = 8;
	info->pixmap.access_align = 32;
	info->pixmap.flags = FB_PIXMAP_SYSTEM;
	info->pixmap.scan_align = 1;

	DRM_DEBUG("fb depth is %d\n", fb->depth);
	DRM_DEBUG("   pitch is %d\n", fb->pitch);
	switch(fb->depth) {
	case 8:
		info->var.red.offset = 0;
		info->var.green.offset = 0;
		info->var.blue.offset = 0;
		info->var.red.length = 8; /* 8bit DAC */
		info->var.green.length = 8;
		info->var.blue.length = 8;
		info->var.transp.offset = 0;
		info->var.transp.length = 0;
		break;
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

	fb->fbdev = info;

	par->radeon_fb = radeon_fb;
	par->dev = dev;

	/* To allow resizeing without swapping buffers */
	printk("allocated %p %dx%d fb: 0x%08x, bo %p\n", dev, radeon_fb->base.width,
	       radeon_fb->base.height, obj_priv->bo->offset, fbo);

	mutex_unlock(&dev->struct_mutex);
	return 0;

out_unref:
	drm_gem_object_unreference(fbo);
	mutex_unlock(&dev->struct_mutex);
out:
	return ret;
}

static int radeonfb_multi_fb_probe_crtc(struct drm_device *dev, struct drm_crtc *crtc)
{
	struct radeon_crtc *radeon_crtc = to_radeon_crtc(crtc);
	struct radeon_framebuffer *radeon_fb;
	struct drm_framebuffer *fb;
	struct drm_connector *connector;
	struct fb_info *info;
	struct radeonfb_par *par;
	struct drm_mode_set *modeset;
	unsigned int width, height;
	int new_fb = 0;
	int ret, i, conn_count;

	if (!drm_helper_crtc_in_use(crtc))
		return 0;

	if (!crtc->desired_mode)
		return 0;

	width = crtc->desired_mode->hdisplay;
	height = crtc->desired_mode->vdisplay;

	/* is there an fb bound to this crtc already */
	if (!radeon_crtc->mode_set.fb) {
		ret = radeonfb_create(dev, width, height, width, height, &radeon_fb);
		if (ret)
			return -EINVAL;
		new_fb = 1;
	} else {
		fb = radeon_crtc->mode_set.fb;
		radeon_fb = to_radeon_framebuffer(fb);
		if ((radeon_fb->base.width < width) || (radeon_fb->base.height < height))
			return -EINVAL;
	}
	
	info = radeon_fb->base.fbdev;
	par = info->par;

	modeset = &radeon_crtc->mode_set;
	modeset->fb = &radeon_fb->base;
	conn_count = 0;
	list_for_each_entry(connector, &dev->mode_config.connector_list, head) {
		if (connector->encoder)
			if (connector->encoder->crtc == modeset->crtc) {
				modeset->connectors[conn_count] = connector;
				conn_count++;
				if (conn_count > RADEONFB_CONN_LIMIT)
					BUG();
			}
	}
	
	for (i = conn_count; i < RADEONFB_CONN_LIMIT; i++)
		modeset->connectors[i] = NULL;

	par->crtc_ids[0] = crtc->base.id;

	modeset->num_connectors = conn_count;
	if (modeset->mode != modeset->crtc->desired_mode)
		modeset->mode = modeset->crtc->desired_mode;

	par->crtc_count = 1;

	if (new_fb) {
		info->var.pixclock = -1;
		if (register_framebuffer(info) < 0)
			return -EINVAL;
	} else
		radeonfb_set_par(info);

	printk(KERN_INFO "fb%d: %s frame buffer device\n", info->node,
	       info->fix.id);

	/* Switch back to kernel console on panic */
	panic_mode = *modeset;
	atomic_notifier_chain_register(&panic_notifier_list, &paniced);
	printk(KERN_INFO "registered panic notifier\n");

	return 0;
}

static int radeonfb_multi_fb_probe(struct drm_device *dev)
{

	struct drm_crtc *crtc;
	int ret = 0;

	list_for_each_entry(crtc, &dev->mode_config.crtc_list, head) {
		ret = radeonfb_multi_fb_probe_crtc(dev, crtc);
		if (ret)
			return ret;
	}
	return ret;
}

static int radeonfb_single_fb_probe(struct drm_device *dev)
{
	struct drm_crtc *crtc;
	struct drm_connector *connector;
	unsigned int fb_width = (unsigned)-1, fb_height = (unsigned)-1;
	unsigned int surface_width = 0, surface_height = 0;
	int new_fb = 0;
	int crtc_count = 0;
	int ret, i, conn_count = 0;
	struct radeon_framebuffer *radeon_fb;
	struct fb_info *info;
	struct radeonfb_par *par;
	struct drm_mode_set *modeset = NULL;

	DRM_DEBUG("\n");
	/* first up get a count of crtcs now in use and new min/maxes width/heights */
	list_for_each_entry(crtc, &dev->mode_config.crtc_list, head) {
		if (drm_helper_crtc_in_use(crtc)) {
			if (crtc->desired_mode) {
				if (crtc->desired_mode->hdisplay < fb_width)
					fb_width = crtc->desired_mode->hdisplay;
				
				if (crtc->desired_mode->vdisplay < fb_height)
					fb_height = crtc->desired_mode->vdisplay;
				
				if (crtc->desired_mode->hdisplay > surface_width)
					surface_width = crtc->desired_mode->hdisplay;
				
				if (crtc->desired_mode->vdisplay > surface_height)
					surface_height = crtc->desired_mode->vdisplay;

			}
		crtc_count++;
		}
	}

	if (crtc_count == 0 || fb_width == -1 || fb_height == -1) {
		/* hmm everyone went away - assume VGA cable just fell out
		   and will come back later. */
		return 0;
	}

	/* do we have an fb already? */
	if (list_empty(&dev->mode_config.fb_kernel_list)) {
		/* create an fb if we don't have one */
		ret = radeonfb_create(dev, fb_width, fb_height, surface_width, surface_height, &radeon_fb);
		if (ret)
			return -EINVAL;
		new_fb = 1;
	} else {
		struct drm_framebuffer *fb;
		fb = list_first_entry(&dev->mode_config.fb_kernel_list, struct drm_framebuffer, filp_head);
		radeon_fb = to_radeon_framebuffer(fb);

		/* if someone hotplugs something bigger than we have already allocated, we are pwned.
		   As really we can't resize an fbdev that is in the wild currently due to fbdev
		   not really being designed for the lower layers moving stuff around under it.
		   - so in the grand style of things - punt. */
		if ((fb->width < surface_width) || (fb->height < surface_height)) {
			DRM_ERROR("Framebuffer not large enough to scale console onto.\n");
			return -EINVAL;
		}
	}

	info = radeon_fb->base.fbdev;
	par = info->par;

	crtc_count = 0;
	/* okay we need to setup new connector sets in the crtcs */
	list_for_each_entry(crtc, &dev->mode_config.crtc_list, head) {
		struct radeon_crtc *radeon_crtc = to_radeon_crtc(crtc);
		modeset = &radeon_crtc->mode_set;
		modeset->fb = &radeon_fb->base;
		conn_count = 0;
		list_for_each_entry(connector, &dev->mode_config.connector_list, head) {
			if (connector->encoder)
				if(connector->encoder->crtc == modeset->crtc) {
					modeset->connectors[conn_count] = connector;
					conn_count++;
					if (conn_count > RADEONFB_CONN_LIMIT)
						BUG();
				}
		}

		for (i = conn_count; i < RADEONFB_CONN_LIMIT; i++)
			modeset->connectors[i] = NULL;

		
		par->crtc_ids[crtc_count++] = crtc->base.id;

		modeset->num_connectors = conn_count;
		if (modeset->mode != modeset->crtc->desired_mode)
			modeset->mode = modeset->crtc->desired_mode;
	}
	par->crtc_count = crtc_count;

	if (new_fb) {
		info->var.pixclock = -1;
		if (register_framebuffer(info) < 0)
			return -EINVAL;
	} else
		radeonfb_set_par(info);
		
	printk(KERN_INFO "fb%d: %s frame buffer device\n", info->node,
	       info->fix.id);

	/* Switch back to kernel console on panic */
	panic_mode = *modeset;
	atomic_notifier_chain_register(&panic_notifier_list, &paniced);
	printk(KERN_INFO "registered panic notifier\n");

	return 0;
}

int radeonfb_probe(struct drm_device *dev)
{
	int ret;

	DRM_DEBUG("\n");

	/* something has changed in the lower levels of hell - deal with it 
	   here */

	/* two modes : a) 1 fb to rule all crtcs.
	               b) one fb per crtc.
	   two actions 1) new connected device
 	               2) device removed.
	   case a/1 : if the fb surface isn't big enough - resize the surface fb.
	              if the fb size isn't big enough - resize fb into surface.
		      if everything big enough configure the new crtc/etc.
	   case a/2 : undo the configuration
	              possibly resize down the fb to fit the new configuration.
           case b/1 : see if it is on a new crtc - setup a new fb and add it.
	   case b/2 : teardown the new fb.
	*/

	/* mode a first */
	/* search for an fb */
	//	if (radeon_fbpercrtc == 1) {
	//		ret = radeonfb_multi_fb_probe(dev);
	//	} else {
	ret = radeonfb_single_fb_probe(dev);
		//	}

	return ret;
}
EXPORT_SYMBOL(radeonfb_probe);

int radeonfb_remove(struct drm_device *dev, struct drm_framebuffer *fb)
{
	struct fb_info *info;
	struct radeon_framebuffer *radeon_fb = to_radeon_framebuffer(fb);

	if (!fb)
		return -EINVAL;

	info = fb->fbdev;
	
	if (info) {
		unregister_framebuffer(info);
		drm_bo_kunmap(&radeon_fb->kmap_obj);
		mutex_lock(&dev->struct_mutex);
		drm_gem_object_unreference(radeon_fb->obj);
		mutex_unlock(&dev->struct_mutex);
		framebuffer_release(info);
	}

	atomic_notifier_chain_unregister(&panic_notifier_list, &paniced);
	memset(&panic_mode, 0, sizeof(struct drm_mode_set));
	return 0;
}
EXPORT_SYMBOL(radeonfb_remove);
MODULE_LICENSE("GPL");
