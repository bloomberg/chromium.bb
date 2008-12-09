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
#include "drm_crtc.h"

struct drmfb_par {
	struct drm_device *dev;
	struct drm_crtc *crtc;
};

static int drmfb_setcolreg(unsigned regno, unsigned red, unsigned green,
			   unsigned blue, unsigned transp,
			   struct fb_info *info)
{
	struct drmfb_par *par = info->par;
	struct drm_framebuffer *fb = par->crtc->fb;
	struct drm_crtc *crtc = par->crtc;

	if (regno > 255)
		return 1;

	if (fb->depth == 8) {
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

static int drmfb_check_var(struct fb_var_screeninfo *var, struct fb_info *info)
{
	struct drmfb_par *par = info->par;
	struct drm_device *dev = par->dev;
	struct drm_framebuffer *fb = par->crtc->fb;
	struct drm_display_mode *drm_mode;
	struct drm_output *output;
	int depth;

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
		    drm_mode->clock != 0)
		    break;
	}

	if (!drm_mode)
		return -EINVAL;
#endif

	return 0;
}

/* this will let fbcon do the mode init */
static int drmfb_set_par(struct fb_info *info)
{
	struct drmfb_par *par = info->par;
	struct drm_framebuffer *fb = par->crtc->fb;
	struct drm_device *dev = par->dev;
	struct drm_display_mode *drm_mode;
	struct fb_var_screeninfo *var = &info->var;
	struct drm_output *output;

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
		    drm_mode->clock != 0)
		    break;
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

	if (!drm_crtc_set_mode(par->crtc, drm_mode, 0, 0))
		return -EINVAL;

	/* Have to destroy our created mode if we're not searching the mode
	 * list for it.
	 */
#if 1 
	drm_mode_destroy(dev, drm_mode);
#endif

	return 0;
}

static struct fb_ops drmfb_ops = {
	.owner = THIS_MODULE,
	//	.fb_open = drmfb_open,
	//	.fb_read = drmfb_read,
	//	.fb_write = drmfb_write,
	//	.fb_release = drmfb_release,
	//	.fb_ioctl = drmfb_ioctl,
	.fb_check_var = drmfb_check_var,
	.fb_set_par = drmfb_set_par,
	.fb_setcolreg = drmfb_setcolreg,
	.fb_fillrect = cfb_fillrect,
	.fb_copyarea = cfb_copyarea,
	.fb_imageblit = cfb_imageblit,
};

int drmfb_probe(struct drm_device *dev, struct drm_crtc *crtc)
{
	struct fb_info *info;
	struct drm_framebuffer *fb = crtc->fb;
	struct drmfb_par *par;
	struct device *device = &dev->pdev->dev; 
	struct drm_display_mode *mode = crtc->desired_mode;
	int ret;

	info = framebuffer_alloc(sizeof(struct drmfb_par), device);
	if (!info)
		return -ENOMEM;

	fb->fbdev = info;
		
	par = info->par;

	par->dev = dev;
	par->crtc = crtc;

	info->fbops = &drmfb_ops;

	strcpy(info->fix.id, "drmfb");
	info->fix.type = FB_TYPE_PACKED_PIXELS;
	info->fix.visual = FB_VISUAL_TRUECOLOR;
	info->fix.accel = FB_ACCEL_NONE;
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
	info->screen_size = info->fix.smem_len; /* ??? */
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

	DRM_DEBUG("fb depth is %d\n", fb->depth);
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
		info->var.transp.length = 0;
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

	if (register_framebuffer(info) < 0) {
		unregister_framebuffer(info);
		return -EINVAL;
	}

	printk(KERN_INFO "fb%d: %s frame buffer device\n", info->node,
	       info->fix.id);
	return 0;
}
EXPORT_SYMBOL(drmfb_probe);

int drmfb_remove(struct drm_device *dev, struct drm_crtc *crtc)
{
	struct fb_info *info = fb->fbdev;
	struct drm_framebuffer *fb = crtc->fb;
	
	if (info) {
		drm_mem_reg_iounmap(dev, &fb->bo->mem, fb->virtual_base);
		unregister_framebuffer(info);
		framebuffer_release(info);
	}
	return 0;
}
EXPORT_SYMBOL(drmfb_remove);
MODULE_LICENSE("GPL");
