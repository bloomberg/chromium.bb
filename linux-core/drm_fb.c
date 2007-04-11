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
struct drmfb_par {
	struct drm_device *dev;
	struct drm_framebuffer *fb;
};

static int drmfb_setcolreg(unsigned regno, unsigned red, unsigned green,
			   unsigned blue, unsigned transp,
			   struct fb_info *info)
{
	struct drmfb_par *par = info->par;
	struct drm_framebuffer *fb = par->fb;
	if (regno > 17)
		return 1;

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

static struct fb_ops drmfb_ops = {
	.owner = THIS_MODULE,
	//	.fb_open = drmfb_open,
	//	.fb_read = drmfb_read,
	//	.fb_write = drmfb_write,
	//	.fb_release = drmfb_release,
	//	.fb_ioctl = drmfb_ioctl,
	.fb_setcolreg = drmfb_setcolreg,
	.fb_fillrect = cfb_fillrect,
	.fb_copyarea = cfb_copyarea,
	.fb_imageblit = cfb_imageblit,
};

int drmfb_probe(struct drm_device *dev, struct drm_framebuffer *fb)
{
	struct fb_info *info;
	struct drmfb_par *par;
	struct device *device = &dev->pdev->dev; 
	struct fb_var_screeninfo *var_info;
	unsigned long base, size;

	info = framebuffer_alloc(sizeof(struct drmfb_par), device);
	if (!info){
		return -EINVAL;
	}

	fb->fbdev = info;
		
	par = info->par;

	par->dev = dev;
	par->fb = fb;

	info->fbops = &drmfb_ops;

	strcpy(info->fix.id, "drmfb");
	info->fix.smem_start = fb->offset + dev->mode_config.fb_base;
	info->fix.smem_len = (8*1024*1024);
	info->fix.type = FB_TYPE_PACKED_PIXELS;
	info->fix.visual = FB_VISUAL_DIRECTCOLOR;
	info->fix.accel = FB_ACCEL_NONE;
	info->fix.type_aux = 0;
	info->fix.mmio_start = 0;
	info->fix.mmio_len = 0;
	info->fix.line_length = fb->pitch * ((fb->bits_per_pixel + 1) / 8);

	info->flags = FBINFO_DEFAULT;

	base = fb->bo->offset + dev->mode_config.fb_base;
	size = (fb->bo->mem.num_pages * PAGE_SIZE);

	DRM_DEBUG("remapping %08X %d\n", base, size);
	fb->virtual_base = ioremap_nocache(base, size);

	info->screen_base = fb->virtual_base;
	info->screen_size = size;
	info->pseudo_palette = fb->pseudo_palette;
	info->var.xres = fb->width;
	info->var.xres_virtual = fb->pitch;
	info->var.yres = fb->height;
	info->var.yres_virtual = fb->height;
	info->var.bits_per_pixel = fb->bits_per_pixel;
	info->var.xoffset = 0;
	info->var.yoffset = 0;
	info->var.activate = FB_ACTIVATE_NOW;
	info->var.height = -1;
	info->var.width = -1;
	info->var.vmode = FB_VMODE_NONINTERLACED;

	DRM_DEBUG("fb depth is %d\n", fb->depth);
	switch(fb->depth) {
	case 8:
	case 15:
	case 16:
		break;
	default:
	case 24:
	case 32:
		info->var.red.offset = 16;
		info->var.green.offset = 8;
		info->var.blue.offset = 0;
		info->var.red.length = info->var.green.length =
			info->var.blue.length = 8;
		if (fb->depth == 32) {
			info->var.transp.offset = 24;
			info->var.transp.length = 8;
		}
		break;
	}

	if (register_framebuffer(info) < 0)
		return -EINVAL;

	printk(KERN_INFO "fb%d: %s frame buffer device\n", info->node,
	       info->fix.id);
	return 0;
}
EXPORT_SYMBOL(drmfb_probe);

int drmfb_remove(struct drm_device *dev, struct drm_framebuffer *fb)
{
	struct fb_info *info = fb->fbdev;
	
	if (info) {
		iounmap(fb->virtual_base);
		unregister_framebuffer(info);
		framebuffer_release(info);
	}
	return 0;
}
EXPORT_SYMBOL(drmfb_remove);
MODULE_LICENSE("GPL");
