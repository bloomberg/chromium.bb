/**************************************************************************
 * 
 * Copyright 2007 Dave Airlie
 * All Rights Reserved.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS, AUTHORS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR 
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE 
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 * 
 * 
 **************************************************************************/
/*
 * Authors: Dave Airlie <airlied@linux.ie>
 */

#include "drmP.h"
#include "radeon_drm.h"
#include "radeon_drv.h"

drm_ttm_backend_t *radeon_create_ttm_backend_entry(drm_device_t * dev)
{
	drm_radeon_private_t *dev_priv = dev->dev_private;

	if(dev_priv->flags & RADEON_IS_AGP)
		return drm_agp_init_ttm(dev);
	else
		return ati_pcigart_init_ttm(dev, &dev_priv->gart_info, radeon_gart_flush);
}

int radeon_fence_types(drm_buffer_object_t *bo, uint32_t * class, uint32_t * type)
{
	*class = 0;
	if (bo->mem.flags & (DRM_BO_FLAG_READ | DRM_BO_FLAG_WRITE))
		*type = 3;
	else
		*type = 1;
	return 0;
}

int radeon_invalidate_caches(drm_device_t * dev, uint32_t flags)
{
	drm_radeon_private_t *dev_priv = dev->dev_private;
	RING_LOCALS;

	BEGIN_RING(4);
	RADEON_FLUSH_CACHE();
	RADEON_FLUSH_ZCACHE();
	ADVANCE_RING();
	return 0;
}

uint32_t radeon_evict_mask(drm_buffer_object_t *bo)
{
	switch (bo->mem.mem_type) {
	case DRM_BO_MEM_LOCAL:
	case DRM_BO_MEM_TT:
		return DRM_BO_FLAG_MEM_LOCAL;
	case DRM_BO_MEM_VRAM:
		if (bo->mem.num_pages > 128)
			return DRM_BO_MEM_TT;
		else
			return DRM_BO_MEM_LOCAL;
	default:
		return DRM_BO_FLAG_MEM_TT | DRM_BO_FLAG_CACHED;
	}
}

int radeon_init_mem_type(drm_device_t * dev, uint32_t type,
			 drm_mem_type_manager_t * man)
{
	drm_radeon_private_t *dev_priv = dev->dev_private;

	switch (type) {
	case DRM_BO_MEM_LOCAL:
		man->flags = _DRM_FLAG_MEMTYPE_MAPPABLE |
		    _DRM_FLAG_MEMTYPE_CACHED;
		man->drm_bus_maptype = 0;
		break;
	case DRM_BO_MEM_VRAM:
		man->flags =  _DRM_FLAG_MEMTYPE_FIXED | _DRM_FLAG_MEMTYPE_MAPPABLE | _DRM_FLAG_NEEDS_IOREMAP;
		man->io_addr = NULL;
		man->drm_bus_maptype = _DRM_FRAME_BUFFER;
		man->io_offset = drm_get_resource_start(dev, 0);
		man->io_size = drm_get_resource_len(dev, 0);
		break;
	case DRM_BO_MEM_TT:
		if (dev_priv->flags & RADEON_IS_AGP) {
			if (!(drm_core_has_AGP(dev) && dev->agp)) {
				DRM_ERROR("AGP is not enabled for memory type %u\n",
					  (unsigned)type);
				return -EINVAL;
			}
			man->io_offset = dev->agp->agp_info.aper_base;
			man->io_size = dev->agp->agp_info.aper_size * 1024 * 1024;
			man->io_addr = NULL;
			man->flags = _DRM_FLAG_MEMTYPE_MAPPABLE |
				_DRM_FLAG_MEMTYPE_CSELECT | _DRM_FLAG_NEEDS_IOREMAP;
			man->drm_bus_maptype = _DRM_AGP;
		} else {
			man->io_offset = dev_priv->gart_vm_start;
			man->io_size = dev_priv->gart_size;
			man->io_addr = NULL;
			man->flags = _DRM_FLAG_MEMTYPE_CSELECT | _DRM_FLAG_MEMTYPE_MAPPABLE | _DRM_FLAG_MEMTYPE_CMA;
			man->drm_bus_maptype = _DRM_SCATTER_GATHER;
		}
		break;
	default:
		DRM_ERROR("Unsupported memory type %u\n", (unsigned)type);
		return -EINVAL;
	}
	return 0;
}

static void radeon_emit_copy_blit(drm_device_t * dev,
				  uint32_t src_offset,
				  uint32_t dst_offset,
				  uint32_t pages, int direction)
{
	uint32_t cur_pages;
	uint32_t stride = PAGE_SIZE;
	drm_radeon_private_t *dev_priv = dev->dev_private;
	uint32_t format, height;
	RING_LOCALS;

	if (!dev_priv)
		return;

	/* 32-bit copy format */
	format = RADEON_COLOR_FORMAT_ARGB8888;

	/* radeon limited to 16k stride */
	stride &= 0x3fff;
	while(pages > 0) {
		cur_pages = pages;
		if (cur_pages > 2048)
			cur_pages = 2048;
		pages -= cur_pages;

		/* needs verification */
		BEGIN_RING(7);		
		OUT_RING(CP_PACKET3(RADEON_CNTL_BITBLT_MULTI, 5));
		OUT_RING(RADEON_GMC_SRC_PITCH_OFFSET_CNTL |
			 RADEON_GMC_DST_PITCH_OFFSET_CNTL |
			 RADEON_GMC_BRUSH_NONE |
			 (format << 8) |
			 RADEON_GMC_SRC_DATATYPE_COLOR |
			 RADEON_ROP3_S |
			 RADEON_DP_SRC_SOURCE_MEMORY |
			 RADEON_GMC_CLR_CMP_CNTL_DIS | RADEON_GMC_WR_MSK_DIS);
		if (direction) {
			OUT_RING((stride << 22) | (src_offset >> 10));
			OUT_RING((stride << 22) | (dst_offset >> 10));
		} else {
			OUT_RING((stride << 22) | (dst_offset >> 10));
			OUT_RING((stride << 22) | (src_offset >> 10));
		}
		OUT_RING(0);
		OUT_RING(pages); /* x - y */
		OUT_RING((stride << 16) | cur_pages);
		ADVANCE_RING();
	}

	BEGIN_RING(2);
	RADEON_WAIT_UNTIL_2D_IDLE();
	ADVANCE_RING();

	return;
}

static int radeon_move_blit(drm_buffer_object_t * bo,
			    int evict, int no_wait, drm_bo_mem_reg_t *new_mem)
{
	drm_bo_mem_reg_t *old_mem = &bo->mem;
	int dir = 0;

	if ((old_mem->mem_type == new_mem->mem_type) &&
	    (new_mem->mm_node->start <
	     old_mem->mm_node->start + old_mem->mm_node->size)) {
		dir = 1;
	}

	radeon_emit_copy_blit(bo->dev,
			      old_mem->mm_node->start << PAGE_SHIFT,
			      new_mem->mm_node->start << PAGE_SHIFT,
			      new_mem->num_pages, dir);

	
	return drm_bo_move_accel_cleanup(bo, evict, no_wait, 0,
					 DRM_FENCE_TYPE_EXE |
					 DRM_RADEON_FENCE_TYPE_RW,
					 DRM_RADEON_FENCE_FLAG_FLUSHED, new_mem);
}

static int radeon_move_flip(drm_buffer_object_t * bo,
			    int evict, int no_wait, drm_bo_mem_reg_t * new_mem)
{
	drm_device_t *dev = bo->dev;
	drm_bo_mem_reg_t tmp_mem;
	int ret;

	tmp_mem = *new_mem;
	tmp_mem.mm_node = NULL;
	tmp_mem.mask = DRM_BO_FLAG_MEM_TT |
	    DRM_BO_FLAG_CACHED | DRM_BO_FLAG_FORCE_CACHING;

	ret = drm_bo_mem_space(bo, &tmp_mem, no_wait);
	if (ret)
		return ret;

	ret = drm_bind_ttm(bo->ttm, 1, tmp_mem.mm_node->start);
	if (ret)
		goto out_cleanup;

	ret = radeon_move_blit(bo, 1, no_wait, &tmp_mem);
	if (ret)
		goto out_cleanup;

	ret = drm_bo_move_ttm(bo, evict, no_wait, new_mem);
out_cleanup:
	if (tmp_mem.mm_node) {
		mutex_lock(&dev->struct_mutex);
		if (tmp_mem.mm_node != bo->pinned_node)
			drm_mm_put_block(tmp_mem.mm_node);
		tmp_mem.mm_node = NULL;
		mutex_unlock(&dev->struct_mutex);
	}
	return ret;
}

int radeon_move(drm_buffer_object_t * bo,
		int evict, int no_wait, drm_bo_mem_reg_t * new_mem)
{
	drm_bo_mem_reg_t *old_mem = &bo->mem;

	DRM_DEBUG("\n");
	if (old_mem->mem_type == DRM_BO_MEM_LOCAL) {
		return drm_bo_move_memcpy(bo, evict, no_wait, new_mem);
	} else if (new_mem->mem_type == DRM_BO_MEM_LOCAL) {
		if (radeon_move_flip(bo, evict, no_wait, new_mem))
			return drm_bo_move_memcpy(bo, evict, no_wait, new_mem);
	} else {
		if (radeon_move_blit(bo, evict, no_wait, new_mem))
			return drm_bo_move_memcpy(bo, evict, no_wait, new_mem);
	}
	return 0;
}

