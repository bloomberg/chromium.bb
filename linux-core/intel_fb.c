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
#include "i915_drm.h"
#include "i915_drv.h"

struct intelfb_par {
	struct drm_device *dev;
	struct drm_framebuffer *fb;
};

static int intelfb_setcolreg(unsigned regno, unsigned red, unsigned green,
			   unsigned blue, unsigned transp,
			   struct fb_info *info)
{
	struct intelfb_par *par = info->par;
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

/* this will let fbcon do the mode init */
static int intelfb_set_par(struct fb_info *info)
{
	struct intelfb_par *par = info->par;
	struct drm_device *dev = par->dev;

	drm_set_desired_modes(dev);
	return 0;
}

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

static struct fb_ops intelfb_ops = {
	.owner = THIS_MODULE,
	//	.fb_open = intelfb_open,
	//	.fb_read = intelfb_read,
	//	.fb_write = intelfb_write,
	//	.fb_release = intelfb_release,
	//	.fb_ioctl = intelfb_ioctl,
	.fb_set_par = intelfb_set_par,
	.fb_setcolreg = intelfb_setcolreg,
	.fb_fillrect = cfb_fillrect,
	.fb_copyarea = cfb_copyarea, //intelfb_copyarea,
	.fb_imageblit = intelfb_imageblit,
};

int intelfb_probe(struct drm_device *dev, struct drm_framebuffer *fb)
{
	struct fb_info *info;
	struct intelfb_par *par;
	struct device *device = &dev->pdev->dev; 
	int ret;

	info = framebuffer_alloc(sizeof(struct intelfb_par), device);
	if (!info){
		return -EINVAL;
	}

	fb->fbdev = info;
		
	par = info->par;

	par->dev = dev;
	par->fb = fb;

	info->fbops = &intelfb_ops;

	strcpy(info->fix.id, "intelfb");
	info->fix.smem_start = fb->offset + dev->mode_config.fb_base;
	info->fix.smem_len = fb->bo->mem.size;
	info->fix.type = FB_TYPE_PACKED_PIXELS;
	info->fix.type_aux = 0;
	info->fix.xpanstep = 8;
	info->fix.ypanstep = 1;
	info->fix.ywrapstep = 0;
	info->fix.visual = FB_VISUAL_DIRECTCOLOR;
	info->fix.accel = FB_ACCEL_I830;
	info->fix.type_aux = 0;
	info->fix.mmio_start = 0;
	info->fix.mmio_len = 0;
	info->fix.line_length = fb->pitch;

	info->flags = FBINFO_DEFAULT;

	ret = drm_mem_reg_ioremap(dev, &fb->bo->mem, &fb->virtual_base);
	if (ret)
		DRM_ERROR("error mapping fb: %d\n", ret);

	info->screen_base = fb->virtual_base;
	info->screen_size = fb->bo->mem.size;
	info->pseudo_palette = fb->pseudo_palette;
	info->var.xres = fb->width;
	info->var.xres_virtual = fb->width;
	info->var.yres = fb->height;
	info->var.yres_virtual = fb->height;
	info->var.bits_per_pixel = fb->bits_per_pixel;
	info->var.xoffset = 0;
	info->var.yoffset = 0;
	info->var.activate = FB_ACTIVATE_NOW;
	info->var.height = -1;
	info->var.width = -1;
	info->var.vmode = FB_VMODE_NONINTERLACED;

	info->pixmap.size = 64*1024;
	info->pixmap.buf_align = 8;
	info->pixmap.access_align = 32;
	info->pixmap.flags = FB_PIXMAP_SYSTEM;
	info->pixmap.scan_align = 1;

	DRM_DEBUG("fb depth is %d\n", fb->depth);
	DRM_DEBUG("   pitch is %d\n", fb->pitch);
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
EXPORT_SYMBOL(intelfb_probe);

int intelfb_remove(struct drm_device *dev, struct drm_framebuffer *fb)
{
	struct fb_info *info = fb->fbdev;
	
	if (info) {
		unregister_framebuffer(info);
		framebuffer_release(info);
		drm_mem_reg_iounmap(dev, &fb->bo->mem, fb->virtual_base);
	}
	return 0;
}
EXPORT_SYMBOL(intelfb_remove);
MODULE_LICENSE("GPL");
