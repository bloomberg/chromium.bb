/****************************************************************************
 * Copyright (C) 2003-2006 by XGI Technology, Taiwan.
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.  IN NO EVENT SHALL
 * XGI AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ***************************************************************************/

#ifndef _XGI_DRV_H_
#define _XGI_DRV_H_

#include "drmP.h"
#include "drm.h"
#include "drm_sman.h"

#define DRIVER_AUTHOR		"Andrea Zhang <andrea_zhang@macrosynergy.com>"

#define DRIVER_NAME		"xgi"
#define DRIVER_DESC		"XGI XP5 / XP10 / XG47"
#define DRIVER_DATE		"20080612"

#define DRIVER_MAJOR		1
#define DRIVER_MINOR		2
#define DRIVER_PATCHLEVEL	0

#include "xgi_cmdlist.h"
#include "xgi_drm.h"

struct xgi_aperture {
	dma_addr_t base;
	unsigned int size;
};

struct xgi_info {
	struct drm_device *dev;

	bool bootstrap_done;

	/* physical characteristics */
	struct xgi_aperture mmio;
	struct xgi_aperture fb;
	struct xgi_aperture pcie;

	struct drm_map *mmio_map;
	struct drm_map *pcie_map;
	struct drm_map *fb_map;

	/* look up table parameters */
	struct drm_ati_pcigart_info gart_info;
	unsigned int lutPageSize;

	struct drm_sman sman;
	bool fb_heap_initialized;
	bool pcie_heap_initialized;

	struct xgi_cmdring_info cmdring;

	DRM_SPINTYPE fence_lock;
	wait_queue_head_t fence_queue;
	unsigned complete_sequence;
	unsigned next_sequence;
};

extern long xgi_compat_ioctl(struct file *filp, unsigned int cmd,
	unsigned long arg);

extern int xgi_fb_heap_init(struct xgi_info * info);

extern int xgi_alloc(struct xgi_info * info, struct xgi_mem_alloc * alloc,
	struct drm_file * filp);

extern int xgi_free(struct xgi_info * info, unsigned int index,
	struct drm_file * filp);

extern int xgi_pcie_heap_init(struct xgi_info * info);

extern void *xgi_find_pcie_virt(struct xgi_info * info, u32 address);

extern void xgi_enable_mmio(struct xgi_info * info);
extern void xgi_disable_mmio(struct xgi_info * info);
extern void xgi_enable_ge(struct xgi_info * info);
extern void xgi_disable_ge(struct xgi_info * info);

/* TTM-style fences.
 */
#ifdef XGI_HAVE_FENCE
extern void xgi_poke_flush(struct drm_device * dev, uint32_t class);
extern int xgi_fence_emit_sequence(struct drm_device * dev, uint32_t class,
	uint32_t flags, uint32_t * sequence, uint32_t * native_type);
extern void xgi_fence_handler(struct drm_device * dev);
extern int xgi_fence_has_irq(struct drm_device *dev, uint32_t class,
	uint32_t flags);
#endif /* XGI_HAVE_FENCE */


/* Non-TTM-style fences.
 */
extern int xgi_set_fence_ioctl(struct drm_device * dev, void * data,
	struct drm_file * filp);
extern int xgi_wait_fence_ioctl(struct drm_device * dev, void * data,
	struct drm_file * filp);

extern int xgi_alloc_ioctl(struct drm_device * dev, void * data,
	struct drm_file * filp);
extern int xgi_free_ioctl(struct drm_device * dev, void * data,
	struct drm_file * filp);
extern int xgi_submit_cmdlist(struct drm_device * dev, void * data,
	struct drm_file * filp);
extern int xgi_state_change_ioctl(struct drm_device * dev, void * data,
	struct drm_file * filp);

#endif
