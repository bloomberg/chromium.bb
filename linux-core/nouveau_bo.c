/*
 * Copyright 2007 Dave Airlied
 * All Rights Reserved.
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
 * VA LINUX SYSTEMS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */
/*
 * Authors: Dave Airlied <airlied@linux.ie>
 *	    Ben Skeggs   <darktama@iinet.net.au>
 *	    Jeremy Kolb  <jkolb@brandeis.edu>
 */

#include "drmP.h"
#include "nouveau_drm.h"
#include "nouveau_drv.h"
#include "nouveau_dma.h"

static struct drm_ttm_backend *
nouveau_bo_create_ttm_backend_entry(struct drm_device * dev)
{
	struct drm_nouveau_private *dev_priv = dev->dev_private;

	switch (dev_priv->gart_info.type) {
	case NOUVEAU_GART_AGP:
		return drm_agp_init_ttm(dev);
	case NOUVEAU_GART_SGDMA:
		return nouveau_sgdma_init_ttm(dev);
	default:
		DRM_ERROR("Unknown GART type %d\n", dev_priv->gart_info.type);
		break;
	}

	return NULL;
}

static int
nouveau_bo_fence_type(struct drm_buffer_object *bo,
		      uint32_t *fclass, uint32_t *type)
{
	/* When we get called, *fclass is set to the requested fence class */

	if (bo->mem.mask & (DRM_BO_FLAG_READ | DRM_BO_FLAG_WRITE))
		*type = 3;
	else
		*type = 1;
	return 0;

}

static int
nouveau_bo_invalidate_caches(struct drm_device *dev, uint64_t buffer_flags)
{
	/* We'll do this from user space. */
	return 0;
}

static int
nouveau_bo_init_mem_type(struct drm_device *dev, uint32_t type,
			 struct drm_mem_type_manager *man)
{
	struct drm_nouveau_private *dev_priv = dev->dev_private;

	switch (type) {
	case DRM_BO_MEM_LOCAL:
		man->flags = _DRM_FLAG_MEMTYPE_MAPPABLE |
			     _DRM_FLAG_MEMTYPE_CACHED;
		man->drm_bus_maptype = 0;
		break;
	case DRM_BO_MEM_VRAM:
		man->flags = _DRM_FLAG_MEMTYPE_FIXED |
			     _DRM_FLAG_MEMTYPE_MAPPABLE |
			     _DRM_FLAG_NEEDS_IOREMAP;
		man->io_addr = NULL;
		man->drm_bus_maptype = _DRM_FRAME_BUFFER;
		man->io_offset = drm_get_resource_start(dev, 1);
		man->io_size = drm_get_resource_len(dev, 1);
		if (man->io_size > nouveau_mem_fb_amount(dev))
			man->io_size = nouveau_mem_fb_amount(dev);
		break;
	case DRM_BO_MEM_PRIV0:
		/* Unmappable VRAM */
		man->flags = _DRM_FLAG_MEMTYPE_CMA;
		man->drm_bus_maptype = 0;
		break;
	case DRM_BO_MEM_TT:
		switch (dev_priv->gart_info.type) {
		case NOUVEAU_GART_AGP:
			man->flags = _DRM_FLAG_MEMTYPE_MAPPABLE |
				     _DRM_FLAG_MEMTYPE_CSELECT |
				     _DRM_FLAG_NEEDS_IOREMAP;
			man->drm_bus_maptype = _DRM_AGP;
			break;
		case NOUVEAU_GART_SGDMA:
			man->flags = _DRM_FLAG_MEMTYPE_MAPPABLE |
				     _DRM_FLAG_MEMTYPE_CSELECT |
				     _DRM_FLAG_MEMTYPE_CMA;
			man->drm_bus_maptype = _DRM_SCATTER_GATHER;
			break;
		default:
			DRM_ERROR("Unknown GART type: %d\n",
				  dev_priv->gart_info.type);
			return -EINVAL;
		}

		man->io_offset  = dev_priv->gart_info.aper_base;
		man->io_size    = dev_priv->gart_info.aper_size;
		man->io_addr   = NULL;
		break;
	default:
		DRM_ERROR("Unsupported memory type %u\n", (unsigned)type);
		return -EINVAL;
	}
	return 0;
}

static uint32_t
nouveau_bo_evict_mask(struct drm_buffer_object *bo)
{
	switch (bo->mem.mem_type) {
	case DRM_BO_MEM_LOCAL:
	case DRM_BO_MEM_TT:
		return DRM_BO_FLAG_MEM_LOCAL;
	default:
		return DRM_BO_FLAG_MEM_TT | DRM_BO_FLAG_CACHED;
	}
	return 0;
}

/* GPU-assisted copy using NV_MEMORY_TO_MEMORY_FORMAT, can access
 * DRM_BO_MEM_{VRAM,PRIV0,TT} directly.
 */
static int
nouveau_bo_move_m2mf(struct drm_buffer_object *bo, int evict, int no_wait,
		struct drm_bo_mem_reg *new_mem)
{
	struct drm_device *dev = bo->dev;
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	struct nouveau_drm_channel *dchan = &dev_priv->channel;
	struct drm_bo_mem_reg *old_mem = &bo->mem;
	uint32_t srch, dsth, page_count;

	/* Can happen during init/takedown */
	if (!dchan->chan)
		return -EINVAL;

	srch = old_mem->mem_type == DRM_BO_MEM_TT ? NvDmaTT : NvDmaFB;
	dsth = new_mem->mem_type == DRM_BO_MEM_TT ? NvDmaTT : NvDmaFB;
	if (srch != dchan->m2mf_dma_source || dsth != dchan->m2mf_dma_destin) {
		dchan->m2mf_dma_source = srch;
		dchan->m2mf_dma_destin = dsth;

		BEGIN_RING(NvSubM2MF,
			   NV_MEMORY_TO_MEMORY_FORMAT_SET_DMA_SOURCE, 2);
		OUT_RING  (dchan->m2mf_dma_source);
		OUT_RING  (dchan->m2mf_dma_destin);
	}

	page_count = new_mem->num_pages;
	while (page_count) {
		int line_count = (page_count > 2047) ? 2047 : page_count;

		BEGIN_RING(NvSubM2MF, NV_MEMORY_TO_MEMORY_FORMAT_OFFSET_IN, 8);
		OUT_RING  (old_mem->mm_node->start << PAGE_SHIFT);
		OUT_RING  (new_mem->mm_node->start << PAGE_SHIFT);
		OUT_RING  (PAGE_SIZE); /* src_pitch */
		OUT_RING  (PAGE_SIZE); /* dst_pitch */
		OUT_RING  (PAGE_SIZE); /* line_length */
		OUT_RING  (line_count);
		OUT_RING  ((1<<8)|(1<<0));
		OUT_RING  (0);
		BEGIN_RING(NvSubM2MF, NV_MEMORY_TO_MEMORY_FORMAT_NOP, 1);
		OUT_RING  (0);

		page_count -= line_count;
	}

	return drm_bo_move_accel_cleanup(bo, evict, no_wait, dchan->chan->id,
					 DRM_FENCE_TYPE_EXE, 0, new_mem);
}

static int
nouveau_bo_move(struct drm_buffer_object *bo, int evict, int no_wait,
		struct drm_bo_mem_reg *new_mem)
{
	struct drm_bo_mem_reg *old_mem = &bo->mem;

	if (new_mem->mem_type == DRM_BO_MEM_LOCAL) {
		if (old_mem->mem_type == DRM_BO_MEM_LOCAL)
			return drm_bo_move_memcpy(bo, evict, no_wait, new_mem);
#if 0
		if (!nouveau_bo_move_flipd(bo, evict, no_wait, new_mem))
#endif
			return drm_bo_move_memcpy(bo, evict, no_wait, new_mem);
	}
	else
	if (old_mem->mem_type == DRM_BO_MEM_LOCAL) {
#if 0
		if (nouveau_bo_move_flips(bo, evict, no_wait, new_mem))
#endif
			return drm_bo_move_memcpy(bo, evict, no_wait, new_mem);
	}
	else {
//		if (nouveau_bo_move_m2mf(bo, evict, no_wait, new_mem))
			return drm_bo_move_memcpy(bo, evict, no_wait, new_mem);
	}
	return 0;
}

static void
nouveau_bo_flush_ttm(struct drm_ttm *ttm)
{
}

static uint32_t nouveau_mem_prios[]  = {
	DRM_BO_MEM_PRIV0,
	DRM_BO_MEM_VRAM,
	DRM_BO_MEM_TT,
	DRM_BO_MEM_LOCAL
};
static uint32_t nouveau_busy_prios[] = {
	DRM_BO_MEM_TT,
	DRM_BO_MEM_PRIV0,
	DRM_BO_MEM_VRAM,
	DRM_BO_MEM_LOCAL
};

struct drm_bo_driver nouveau_bo_driver = {
	.mem_type_prio = nouveau_mem_prios,
	.mem_busy_prio = nouveau_busy_prios,
	.num_mem_type_prio = sizeof(nouveau_mem_prios)/sizeof(uint32_t),
	.num_mem_busy_prio = sizeof(nouveau_busy_prios)/sizeof(uint32_t),
	.create_ttm_backend_entry = nouveau_bo_create_ttm_backend_entry,
	.fence_type = nouveau_bo_fence_type,
	.invalidate_caches = nouveau_bo_invalidate_caches,
	.init_mem_type = nouveau_bo_init_mem_type,
	.evict_mask = nouveau_bo_evict_mask,
	.move = nouveau_bo_move,
	.ttm_cache_flush= nouveau_bo_flush_ttm
};

int
nouveau_bo_validate(struct drm_device *dev, void *data,
		    struct drm_file *file_priv)
{
	DRM_ERROR("unimplemented\n");
	return -EINVAL;
}

