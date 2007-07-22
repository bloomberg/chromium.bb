
/****************************************************************************
 * Copyright (C) 2003-2006 by XGI Technology, Taiwan.			
 *																			*
 * All Rights Reserved.														*
 *																			*
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the	
 * "Software"), to deal in the Software without restriction, including	
 * without limitation on the rights to use, copy, modify, merge,	
 * publish, distribute, sublicense, and/or sell copies of the Software,	
 * and to permit persons to whom the Software is furnished to do so,	
 * subject to the following conditions:					
 *																			*
 * The above copyright notice and this permission notice (including the	
 * next paragraph) shall be included in all copies or substantial	
 * portions of the Software.						
 *																			*
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,	
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF	
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND		
 * NON-INFRINGEMENT.  IN NO EVENT SHALL XGI AND/OR			
 * ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,		
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,		
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER			
 * DEALINGS IN THE SOFTWARE.												
 ***************************************************************************/

#ifndef _XGI_DRV_H_
#define _XGI_DRV_H_

#include "drmP.h"
#include "drm.h"

#define DRIVER_AUTHOR		"Andrea Zhang <andrea_zhang@macrosynergy.com>"

#define DRIVER_NAME		"xgi"
#define DRIVER_DESC		"XGI XP5 / XP10 / XG47"
#define DRIVER_DATE		"20070721"

#define DRIVER_MAJOR		0
#define DRIVER_MINOR		9
#define DRIVER_PATCHLEVEL	0

#include "xgi_cmdlist.h"
#include "xgi_drm.h"

struct xgi_aperture {
	dma_addr_t base;
	unsigned int size;
};

struct xgi_mem_block {
	struct list_head list;
	unsigned long offset;
	unsigned long size;
	DRMFILE filp;

	unsigned int owner;
};

struct xgi_mem_heap {
	struct list_head free_list;
	struct list_head used_list;
	struct list_head sort_list;
	unsigned long max_freesize;

	bool initialized;
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
	struct drm_dma_handle *lut_handle;
	unsigned int lutPageSize;

	struct xgi_mem_heap fb_heap;
	struct xgi_mem_heap pcie_heap;

	struct semaphore fb_sem;
	struct semaphore pcie_sem;

	struct xgi_cmdring_info cmdring;
};

enum PcieOwner {
	PCIE_2D = 0,
	/*
	   PCIE_3D should not begin with 1,
	   2D alloc pcie memory will use owner 1.
	 */
	PCIE_3D = 11,		/*vetex buf */
	PCIE_3D_CMDLIST = 12,
	PCIE_3D_SCRATCHPAD = 13,
	PCIE_3D_TEXTURE = 14,
	PCIE_INVALID = 0x7fffffff
};


extern struct kmem_cache *xgi_mem_block_cache;
extern struct xgi_mem_block *xgi_mem_alloc(struct xgi_mem_heap * heap,
	unsigned long size, enum PcieOwner owner);
extern int xgi_mem_free(struct xgi_mem_heap * heap, unsigned long offset,
	DRMFILE filp);
extern int xgi_mem_heap_init(struct xgi_mem_heap * heap, unsigned int start,
	unsigned int end);
extern void xgi_mem_heap_cleanup(struct xgi_mem_heap * heap);

extern int xgi_fb_heap_init(struct xgi_info * info);

extern int xgi_fb_alloc(struct xgi_info * info, struct xgi_mem_alloc * alloc,
	DRMFILE filp);

extern int xgi_fb_free(struct xgi_info * info, unsigned long offset,
	DRMFILE filp);

extern int xgi_pcie_heap_init(struct xgi_info * info);
extern void xgi_pcie_lut_cleanup(struct xgi_info * info);

extern int xgi_pcie_alloc(struct xgi_info * info,
			  struct xgi_mem_alloc * alloc, DRMFILE filp);

extern int xgi_pcie_free(struct xgi_info * info, unsigned long offset,
	DRMFILE filp);

extern void *xgi_find_pcie_virt(struct xgi_info * info, u32 address);

extern void xgi_pcie_free_all(struct xgi_info *, DRMFILE);
extern void xgi_fb_free_all(struct xgi_info *, DRMFILE);

extern int xgi_fb_alloc_ioctl(DRM_IOCTL_ARGS);
extern int xgi_fb_free_ioctl(DRM_IOCTL_ARGS);
extern int xgi_pcie_alloc_ioctl(DRM_IOCTL_ARGS);
extern int xgi_pcie_free_ioctl(DRM_IOCTL_ARGS);
extern int xgi_ge_reset_ioctl(DRM_IOCTL_ARGS);
extern int xgi_dump_register_ioctl(DRM_IOCTL_ARGS);
extern int xgi_restore_registers_ioctl(DRM_IOCTL_ARGS);
extern int xgi_submit_cmdlist_ioctl(DRM_IOCTL_ARGS);
extern int xgi_test_rwinkernel_ioctl(DRM_IOCTL_ARGS);
extern int xgi_state_change_ioctl(DRM_IOCTL_ARGS);

#endif
