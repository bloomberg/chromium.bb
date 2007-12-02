/*
 * Copyright © 2007 David Airlie
 * Copyright © 2007 Jerome Glisse
 *
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation on the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NON-INFRINGEMENT.  IN NO EVENT SHALL ATI, VA LINUX SYSTEMS AND/OR
 * THEIR SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
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
#include "radeon_ms.h"

struct radeonfb_par {
	struct drm_device	*dev;
	struct drm_crtc		*crtc;
};

static int radeonfb_setcolreg(unsigned regno, unsigned red,
			       unsigned green, unsigned blue,
			       unsigned transp, struct fb_info *info)
{
	struct radeonfb_par *par = info->par;
	struct drm_crtc *crtc = par->crtc;

	if (regno > 255)
		return 1;
	if (crtc->funcs->gamma_set)
		crtc->funcs->gamma_set(crtc, red, green, blue, regno);
	return 0;
}

static int radeonfb_check_var(struct fb_var_screeninfo *var,
			       struct fb_info *info)
{
        struct radeonfb_par *par = info->par;
	struct drm_framebuffer *fb = par->crtc->fb;

        if (!var->pixclock)
                return -EINVAL;

        /* Need to resize the fb object !!! */
        if (var->xres > fb->width || var->yres > fb->height) {
                DRM_ERROR("Requested width/height is greater than "
			  "current fb object %dx%d > %dx%d\n",
			  var->xres, var->yres, fb->width, fb->height);
                DRM_ERROR("Need resizing code.\n");
                return -EINVAL;
        }

        switch (var->bits_per_pixel) {
        case 16:
		if (var->green.length == 5) {
			var->red.offset = 10;
			var->green.offset = 5;
			var->blue.offset = 0;
			var->red.length = 5;
			var->green.length = 5;
			var->blue.length = 5;
			var->transp.length = 0;
			var->transp.offset = 0;
		} else {
	                var->red.offset = 11;
			var->green.offset = 6;
			var->blue.offset = 0;
			var->red.length = 5;
			var->green.length = 6;
			var->blue.length = 5;
			var->transp.length = 0;
			var->transp.offset = 0;
		}
                break;
	case 32:
                if (var->transp.length) {
			var->red.offset = 16;
			var->green.offset = 8;
			var->blue.offset = 0;
			var->red.length = 8;
			var->green.length = 8;
			var->blue.length = 8;
			var->transp.length = 8;
			var->transp.offset = 24;
		} else {
			var->red.offset = 16;
			var->green.offset = 8;
			var->blue.offset = 0;
			var->red.length = 8;
			var->green.length = 8;
			var->blue.length = 8;
			var->transp.length = 0;
			var->transp.offset = 0;
		}
		break;
        default:
		return -EINVAL; 
        }
	return 0;
}

static int radeonfb_set_par(struct fb_info *info)
{
	struct radeonfb_par *par = info->par;
	struct drm_framebuffer *fb = par->crtc->fb;
	struct drm_device *dev = par->dev;
        struct drm_display_mode *drm_mode;
        struct fb_var_screeninfo *var = &info->var;

        switch (var->bits_per_pixel) {
        case 16:
		fb->depth = (var->green.length == 6) ? 16 : 15;
		break;
        case 32:
		fb->depth = (var->transp.length > 0) ? 32 : 24;
		break;
	default:
		return -EINVAL; 
	}
	fb->bits_per_pixel = var->bits_per_pixel;

	info->fix.line_length = fb->pitch;
	info->fix.smem_len = info->fix.line_length * fb->height;
	info->fix.visual = FB_VISUAL_TRUECOLOR;
	info->screen_size = info->fix.smem_len; /* ??? */

        /* Should we walk the output's modelist or just create our own ???
         * For now, we create and destroy a mode based on the incoming 
         * parameters. But there's commented out code below which scans 
         * the output list too.
         */
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

	drm_mode_debug_printmodeline(dev, drm_mode);

	if (!drm_crtc_set_mode(par->crtc, drm_mode, 0, 0))
		return -EINVAL;

        /* Have to destroy our created mode if we're not searching the mode
         * list for it.
         */
	drm_mode_destroy(dev, drm_mode);

	return 0;
}

static struct fb_ops radeonfb_ops = {
	.owner = THIS_MODULE,
	//	.fb_open = radeonfb_open,
	//	.fb_read = radeonfb_read,
	//	.fb_write = radeonfb_write,
	//	.fb_release = radeonfb_release,
	//	.fb_ioctl = radeonfb_ioctl,
	.fb_check_var = radeonfb_check_var,
	.fb_set_par = radeonfb_set_par,
	.fb_setcolreg = radeonfb_setcolreg,
	.fb_fillrect = cfb_fillrect,
	.fb_copyarea = cfb_copyarea,
	.fb_imageblit = cfb_imageblit,
};

int radeonfb_probe(struct drm_device *dev, struct drm_crtc *crtc)
{
	struct fb_info *info;
	struct radeonfb_par *par;
	struct device *device = &dev->pdev->dev; 
	struct drm_framebuffer *fb;
	struct drm_display_mode *mode = crtc->desired_mode;
	int ret;

	info = framebuffer_alloc(sizeof(struct radeonfb_par), device);
	if (!info){
		DRM_INFO("[radeon_ms] framebuffer_alloc failed\n");
		return -EINVAL;
	}

	fb = drm_framebuffer_create(dev);
	if (!fb) {
		framebuffer_release(info);
		DRM_ERROR("[radeon_ms] failed to allocate fb.\n");
		return -EINVAL;
	}
	crtc->fb = fb;

	fb->width = crtc->desired_mode->hdisplay;
	fb->height = crtc->desired_mode->vdisplay;
	fb->bits_per_pixel = 32;
	fb->pitch = fb->width * ((fb->bits_per_pixel + 1) / 8);
	fb->depth = 24;
	/* one page alignment should be fine for constraint (micro|macro tiling,
	 * bit depth, color buffer offset, ...) */
	ret = drm_buffer_object_create(dev, fb->width * fb->height * 4, 
				       drm_bo_type_kernel,
				       DRM_BO_FLAG_READ |
				       DRM_BO_FLAG_WRITE |
				       DRM_BO_FLAG_NO_EVICT |
				       DRM_BO_FLAG_MEM_VRAM,
				       DRM_BO_HINT_DONT_FENCE,
				       1,
				       0,
				       &fb->bo);
	if (ret || fb->bo == NULL) {
		DRM_ERROR("[radeon_ms] failed to allocate framebuffer\n");
		drm_framebuffer_destroy(fb);
		framebuffer_release(info);
		return -EINVAL;
	}

	fb->offset = fb->bo->offset;
	DRM_INFO("[radeon_ms] framebuffer %dx%d at 0x%08lX\n",
		 fb->width, fb->height, fb->bo->offset);

	fb->fbdev = info;
	par = info->par;
	par->dev = dev;
	par->crtc = crtc;
	info->fbops = &radeonfb_ops;
	strcpy(info->fix.id, "radeonfb");
	info->fix.type = FB_TYPE_PACKED_PIXELS;
	info->fix.visual = FB_VISUAL_TRUECOLOR;
	info->fix.type_aux = 0;
	info->fix.xpanstep = 8;
	info->fix.ypanstep = 1;
	info->fix.ywrapstep = 0;
	info->fix.accel = FB_ACCEL_ATI_RADEON;
	info->fix.type_aux = 0;
	info->fix.mmio_start = 0;
	info->fix.mmio_len = 0;
	info->fix.line_length = fb->pitch;
	info->fix.smem_start = fb->offset + dev->mode_config.fb_base;
	info->fix.smem_len = info->fix.line_length * fb->height;
	info->flags = FBINFO_DEFAULT;
	DRM_INFO("[radeon_ms] fb physical start : 0x%lX\n", info->fix.smem_start);
	DRM_INFO("[radeon_ms] fb physical size  : %d\n", info->fix.smem_len);

	ret = drm_mem_reg_ioremap(dev, &fb->bo->mem, &fb->virtual_base);
	if (ret) {
		DRM_ERROR("error mapping fb: %d\n", ret);
	}

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
		DRM_ERROR("only support 15, 16, 24 or 32bits per pixel "
			  "got %d\n", fb->depth);
		return -EINVAL;
		break;
	}

	if (register_framebuffer(info) < 0) {
		return -EINVAL;
	}

	DRM_INFO("[radeon_ms] fb%d: %s frame buffer device\n", info->node,
		 info->fix.id);
	return 0;
}
EXPORT_SYMBOL(radeonfb_probe);

int radeonfb_remove(struct drm_device *dev, struct drm_crtc *crtc)
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
EXPORT_SYMBOL(radeonfb_remove);
MODULE_LICENSE("GPL");
