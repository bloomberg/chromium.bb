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
#include "i915_drm.h"
#include "i915_drv.h"

struct intelfb_par {
	struct drm_device *dev;
	struct drm_crtc *crtc;
        struct drm_display_mode *fb_mode;
};

static int
var_to_refresh(const struct fb_var_screeninfo *var)
{
	int xtot = var->xres + var->left_margin + var->right_margin +
		   var->hsync_len;
	int ytot = var->yres + var->upper_margin + var->lower_margin +
		   var->vsync_len;

	return (1000000000 / var->pixclock * 1000 + 500) / xtot / ytot;
}

static int intelfb_setcolreg(unsigned regno, unsigned red, unsigned green,
			   unsigned blue, unsigned transp,
			   struct fb_info *info)
{
	struct intelfb_par *par = info->par;
	struct drm_framebuffer *fb = par->crtc->fb;
	struct drm_crtc *crtc = par->crtc;

	if (regno > 255)
		return 1;

	if (fb->depth == 8) {
		if (crtc->funcs->gamma_set)
			crtc->funcs->gamma_set(crtc, red, green, blue, regno);
		return 0;
	}

	if (regno < 16) {
		switch (fb->depth) {
		case 15:
			fb->pseudo_palette[regno] = ((red & 0xf800) >>  1) |
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

	return 0;
}

static int intelfb_check_var(struct fb_var_screeninfo *var,
			     struct fb_info *info)
{
        struct intelfb_par *par = info->par;
        struct drm_device *dev = par->dev;
	struct drm_framebuffer *fb = par->crtc->fb;
        struct drm_output *output;
        int depth, found = 0;

        if (!var->pixclock)
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
                var->green.offset = 6;
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

#if 0
        /* Here we walk the output mode list and look for modes. If we haven't
         * got it, then bail. Not very nice, so this is disabled.
         * In the set_par code, we create our mode based on the incoming
         * parameters. Nicer, but may not be desired by some.
         */
        list_for_each_entry(output, &dev->mode_config.output_list, head) {
                if (output->crtc == par->crtc)
                        break;
        }
    
        list_for_each_entry(drm_mode, &output->modes, head) {
                if (drm_mode->hdisplay == var->xres &&
                    drm_mode->vdisplay == var->yres &&
                    (((PICOS2KHZ(var->pixclock))/1000) >= ((drm_mode->clock/1000)-1)) &&
                    (((PICOS2KHZ(var->pixclock))/1000) <= ((drm_mode->clock/1000)+1))) {
			found = 1;
			break;
		}
	}
 
        if (!found)
                return -EINVAL;
#endif

	return 0;
}

/* this will let fbcon do the mode init */
/* FIXME: take mode config lock? */
static int intelfb_set_par(struct fb_info *info)
{
	struct intelfb_par *par = info->par;
	struct drm_framebuffer *fb = par->crtc->fb;
	struct drm_device *dev = par->dev;
        struct drm_display_mode *drm_mode;
        struct drm_output *output;
        struct fb_var_screeninfo *var = &info->var;
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

        /* Should we walk the output's modelist or just create our own ???
         * For now, we create and destroy a mode based on the incoming 
         * parameters. But there's commented out code below which scans 
         * the output list too.
         */
#if 0
        list_for_each_entry(output, &dev->mode_config.output_list, head) {
                if (output->crtc == par->crtc)
                        break;
        }
    
        list_for_each_entry(drm_mode, &output->modes, head) {
                if (drm_mode->hdisplay == var->xres &&
                    drm_mode->vdisplay == var->yres &&
                    (((PICOS2KHZ(var->pixclock))/1000) >= ((drm_mode->clock/1000)-1)) &&
                    (((PICOS2KHZ(var->pixclock))/1000) <= ((drm_mode->clock/1000)+1))) {
			found = 1;
			break;
		}
        }

	if (!found) {
		DRM_ERROR("Couldn't find a mode for requested %dx%d-%d\n",
			var->xres,var->yres,var_to_refresh(var));
		return -EINVAL;
	}
#else
	
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
        drm_mode_set_name(drm_mode);
	drm_mode_set_crtcinfo(drm_mode, CRTC_INTERLACE_HALVE_V);
#endif

	drm_mode_addmode(dev, drm_mode);
	if (par->fb_mode)
		drm_mode_rmmode(dev, par->fb_mode);
	
	par->fb_mode = drm_mode;
	drm_mode_debug_printmodeline(dev, drm_mode);

        if (!drm_crtc_set_mode(par->crtc, drm_mode, 0, 0))
                return -EINVAL;

	return 0;
}

#if 0
static void intelfb_copyarea(struct fb_info *info,
			     const struct fb_copyarea *region)
{
        struct intelfb_par *par = info->par;
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

void intelfb_imageblit(struct fb_info *info, const struct fb_image *image)
{
        struct intelfb_par *par = info->par;
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

static struct fb_ops intelfb_ops = {
	.owner = THIS_MODULE,
	//	.fb_open = intelfb_open,
	//	.fb_read = intelfb_read,
	//	.fb_write = intelfb_write,
	//	.fb_release = intelfb_release,
	//	.fb_ioctl = intelfb_ioctl,
	.fb_check_var = intelfb_check_var,
	.fb_set_par = intelfb_set_par,
	.fb_setcolreg = intelfb_setcolreg,
	.fb_fillrect = cfb_fillrect,
	.fb_copyarea = cfb_copyarea, //intelfb_copyarea,
	.fb_imageblit = cfb_imageblit, //intelfb_imageblit,
};

int intelfb_probe(struct drm_device *dev, struct drm_crtc *crtc)
{
	struct fb_info *info;
	struct intelfb_par *par;
	struct device *device = &dev->pdev->dev; 
	struct drm_framebuffer *fb;
	struct drm_display_mode *mode = crtc->desired_mode;
	struct drm_buffer_object *fbo = NULL;
	int ret;

	info = framebuffer_alloc(sizeof(struct intelfb_par), device);
	if (!info){
		return -EINVAL;
	}

	fb = drm_framebuffer_create(dev);
	if (!fb) {
		framebuffer_release(info);
		DRM_ERROR("failed to allocate fb.\n");
		return -EINVAL;
	}
	crtc->fb = fb;

	fb->width = crtc->desired_mode->hdisplay;
	fb->height = crtc->desired_mode->vdisplay;

	fb->bits_per_pixel = 32;
	fb->pitch = fb->width * ((fb->bits_per_pixel + 1) / 8);
	fb->depth = 24;
	ret = drm_buffer_object_create(dev, fb->width * fb->height * 4, 
				       drm_bo_type_kernel,
				       DRM_BO_FLAG_READ |
				       DRM_BO_FLAG_WRITE |
				       DRM_BO_FLAG_MEM_TT |
				       DRM_BO_FLAG_MEM_VRAM |
				       DRM_BO_FLAG_NO_EVICT,
				       DRM_BO_HINT_DONT_FENCE, 0, 0,
				       &fbo);
	if (ret || !fbo) {
		printk(KERN_ERR "failed to allocate framebuffer\n");
		drm_framebuffer_destroy(fb);
		framebuffer_release(info);
		return -EINVAL;
	}

	fb->offset = fbo->offset;
	fb->bo = fbo;
	printk("allocated %dx%d fb: 0x%08lx, bo %p\n", fb->width,
		       fb->height, fbo->offset, fbo);


	fb->fbdev = info;
		
	par = info->par;

	par->dev = dev;
	par->crtc = crtc;

	info->fbops = &intelfb_ops;

	strcpy(info->fix.id, "intelfb");
	info->fix.type = FB_TYPE_PACKED_PIXELS;
	info->fix.visual = FB_VISUAL_TRUECOLOR;
	info->fix.type_aux = 0;
	info->fix.xpanstep = 8;
	info->fix.ypanstep = 1;
	info->fix.ywrapstep = 0;
	info->fix.accel = FB_ACCEL_I830;
	info->fix.type_aux = 0;
	info->fix.mmio_start = 0;
	info->fix.mmio_len = 0;
	info->fix.line_length = fb->pitch;
	info->fix.smem_start = fb->offset + dev->mode_config.fb_base;
	info->fix.smem_len = info->fix.line_length * fb->height;

	info->flags = FBINFO_DEFAULT;

	ret = drm_mem_reg_ioremap(dev, &fb->bo->mem, &fb->virtual_base);
	if (ret)
		DRM_ERROR("error mapping fb: %d\n", ret);

	info->screen_base = fb->virtual_base;
	info->screen_size = info->fix.smem_len; /* FIXME */
	info->pseudo_palette = fb->pseudo_palette;
	info->var.xres_virtual = fb->width;
	info->var.yres_virtual = fb->height;
	info->var.bits_per_pixel = fb->bits_per_pixel;
	info->var.xoffset = 0;
	info->var.yoffset = 0;
	info->var.activate = FB_ACTIVATE_NOW;
	info->var.height = -1;
	info->var.width = -1;
	info->var.vmode = FB_VMODE_NONINTERLACED;

        info->var.xres = mode->hdisplay;
        info->var.right_margin = mode->hsync_start - mode->hdisplay;
        info->var.hsync_len = mode->hsync_end - mode->hsync_start;
        info->var.left_margin = mode->htotal - mode->hsync_end;
        info->var.yres = mode->vdisplay;
        info->var.lower_margin = mode->vsync_start - mode->vdisplay;
        info->var.vsync_len = mode->vsync_end - mode->vsync_start;
	info->var.upper_margin = mode->vtotal - mode->vsync_end;
        info->var.pixclock = 10000000 / mode->htotal * 1000 /
		mode->vtotal * 100;
	/* avoid overflow */
	info->var.pixclock = info->var.pixclock * 1000 / mode->vrefresh;

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
                info->var.red.length = info->var.green.length =
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
                info->var.red.length = info->var.green.length =
                        info->var.blue.length = 8;
                info->var.transp.offset = 0;
                info->var.transp.length = 0;
                break;
	case 32:
		info->var.red.offset = 16;
		info->var.green.offset = 8;
		info->var.blue.offset = 0;
		info->var.red.length = info->var.green.length =
			info->var.blue.length = 8;
		info->var.transp.offset = 24;
		info->var.transp.length = 8;
		break;
	default:
		break;
	}

	if (register_framebuffer(info) < 0)
		return -EINVAL;

	printk(KERN_INFO "fb%d: %s frame buffer device\n", info->node,
	       info->fix.id);
	return 0;
}
EXPORT_SYMBOL(intelfb_probe);

int intelfb_remove(struct drm_device *dev, struct drm_crtc *crtc)
{
	struct drm_framebuffer *fb = crtc->fb;
	struct fb_info *info = fb->fbdev;
	
	if (info) {
		unregister_framebuffer(info);
		framebuffer_release(info);
		drm_mem_reg_iounmap(dev, &fb->bo->mem, fb->virtual_base);
		drm_bo_usage_deref_unlocked(&fb->bo);
		drm_framebuffer_destroy(fb);
	}
	return 0;
}
EXPORT_SYMBOL(intelfb_remove);
MODULE_LICENSE("GPL");
