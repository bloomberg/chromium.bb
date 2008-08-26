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

struct drm_ttm_backend *radeon_create_ttm_backend_entry(struct drm_device * dev)
{
	drm_radeon_private_t *dev_priv = dev->dev_private;

	if (dev_priv->flags & RADEON_IS_AGP)
		return drm_agp_init_ttm(dev);
	else
		return ati_pcigart_init_ttm(dev, &dev_priv->gart_info, radeon_gart_flush);
}

int radeon_fence_types(struct drm_buffer_object *bo, uint32_t * class, uint32_t * type)
{
	*class = 0;
	*type = 1;
	return 0;
}

int radeon_invalidate_caches(struct drm_device * dev, uint64_t flags)
{
	drm_radeon_private_t *dev_priv = dev->dev_private;
	RING_LOCALS;

	if (!dev_priv->cp_running)
		return 0;

	BEGIN_RING(6);
	RADEON_PURGE_CACHE();
	RADEON_PURGE_ZCACHE();
	RADEON_WAIT_UNTIL_3D_IDLE();
	ADVANCE_RING();
	COMMIT_RING();
	return 0;
}

int radeon_init_mem_type(struct drm_device * dev, uint32_t type,
			 struct drm_mem_type_manager * man)
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

void radeon_emit_copy_blit(struct drm_device * dev,
			   uint32_t src_offset,
			   uint32_t dst_offset,
			   uint32_t pages)
{
	uint32_t cur_pages;
	uint32_t stride_bytes = PAGE_SIZE;
	drm_radeon_private_t *dev_priv = dev->dev_private;
	uint32_t format, pitch;
	const uint32_t clip = (0x1fff) | (0x1fff << 16);
	uint32_t stride_pixels;
	RING_LOCALS;

	if (!dev_priv)
		return;

	/* 32-bit copy format */
	format = RADEON_COLOR_FORMAT_ARGB8888;

	/* radeon limited to 16k stride */
	stride_bytes &= 0x3fff;
	/* radeon pitch is /64 */
	pitch = stride_bytes / 64;

	stride_pixels = stride_bytes / 4;

	while(pages > 0) {
		cur_pages = pages;
		if (cur_pages > 8191)
			cur_pages = 8191;
		pages -= cur_pages;

		/* pages are in Y direction - height
 		   page width in X direction - width */
		BEGIN_RING(10);
		OUT_RING(CP_PACKET3(RADEON_CNTL_BITBLT_MULTI, 8));
		OUT_RING(RADEON_GMC_SRC_PITCH_OFFSET_CNTL |
			 RADEON_GMC_DST_PITCH_OFFSET_CNTL |
			 RADEON_GMC_SRC_CLIPPING | RADEON_GMC_DST_CLIPPING |
			 RADEON_GMC_BRUSH_NONE |
			 (format << 8) |
			 RADEON_GMC_SRC_DATATYPE_COLOR |
			 RADEON_ROP3_S |
			 RADEON_DP_SRC_SOURCE_MEMORY |
			 RADEON_GMC_CLR_CMP_CNTL_DIS | RADEON_GMC_WR_MSK_DIS);
		OUT_RING((pitch << 22) | (src_offset >> 10));
		OUT_RING((pitch << 22) | (dst_offset >> 10));
		OUT_RING(clip); // SRC _SC BOT_RITE
		OUT_RING(0);   // SC_TOP_LEFT
		OUT_RING(clip); // SC_BOT_RITE

		OUT_RING(pages);
		OUT_RING(pages); /* x - y */
		OUT_RING(cur_pages | (stride_pixels << 16));
		ADVANCE_RING();
	}

	BEGIN_RING(4);
	OUT_RING(CP_PACKET0(RADEON_RB2D_DSTCACHE_CTLSTAT, 0));
	OUT_RING(RADEON_RB2D_DC_FLUSH_ALL);
	RADEON_WAIT_UNTIL_2D_IDLE();
	ADVANCE_RING();

	COMMIT_RING();
	return;
}

int radeon_move_blit(struct drm_buffer_object * bo,
			    int evict, int no_wait, struct drm_bo_mem_reg *new_mem,
			    struct drm_bo_mem_reg *old_mem)
{
	struct drm_device *dev = bo->dev;
	drm_radeon_private_t *dev_priv = dev->dev_private;
	uint32_t old_start, new_start;

	old_start = old_mem->mm_node->start << PAGE_SHIFT;
	new_start = new_mem->mm_node->start << PAGE_SHIFT;

	if (old_mem->mem_type == DRM_BO_MEM_VRAM)
		old_start += dev_priv->fb_location;
	if (old_mem->mem_type == DRM_BO_MEM_TT)
		old_start += dev_priv->gart_vm_start;

	if (new_mem->mem_type == DRM_BO_MEM_VRAM)
		new_start += dev_priv->fb_location;
	if (new_mem->mem_type == DRM_BO_MEM_TT)
		new_start += dev_priv->gart_vm_start;

	radeon_emit_copy_blit(bo->dev,
			      old_start,
			      new_start,
			      new_mem->num_pages);

	/* invalidate the chip caches */

	return drm_bo_move_accel_cleanup(bo, evict, no_wait, 0,
					 DRM_FENCE_TYPE_EXE, 0,
					 new_mem);
}

void radeon_emit_solid_fill(struct drm_device * dev,
			    uint32_t dst_offset,
			    uint32_t pages, uint8_t value)
{
	uint32_t cur_pages;
	uint32_t stride_bytes = PAGE_SIZE;
	drm_radeon_private_t *dev_priv = dev->dev_private;
	uint32_t format, pitch;
	const uint32_t clip = (0x1fff) | (0x1fff << 16);
	uint32_t stride_pixels;
	RING_LOCALS;

	if (!dev_priv)
		return;

	/* 32-bit copy format */
	format = RADEON_COLOR_FORMAT_ARGB8888;

	/* radeon limited to 16k stride */
	stride_bytes &= 0x3fff;
	/* radeon pitch is /64 */
	pitch = stride_bytes / 64;

	stride_pixels = stride_bytes / 4;

	while(pages > 0) {
		cur_pages = pages;
		if (cur_pages > 8191)
			cur_pages = 8191;
		pages -= cur_pages;

		/* pages are in Y direction - height
 		   page width in X direction - width */
		BEGIN_RING(8);
		OUT_RING(CP_PACKET3(RADEON_CNTL_PAINT_MULTI, 6));
		OUT_RING(RADEON_GMC_DST_PITCH_OFFSET_CNTL |
			 RADEON_GMC_DST_CLIPPING |
			 RADEON_GMC_BRUSH_SOLID_COLOR |
			 (format << 8) |
			 RADEON_ROP3_S |
			 RADEON_GMC_CLR_CMP_CNTL_DIS | RADEON_GMC_WR_MSK_DIS);
		OUT_RING((pitch << 22) | (dst_offset >> 10)); // PITCH
		OUT_RING(0);   // SC_TOP_LEFT // DST CLIPPING
		OUT_RING(clip); // SC_BOT_RITE

		OUT_RING(0); // COLOR

		OUT_RING(pages); /* x - y */
		OUT_RING(cur_pages | (stride_pixels << 16));
		ADVANCE_RING();
	}

	BEGIN_RING(4);
	OUT_RING(CP_PACKET0(RADEON_RB2D_DSTCACHE_CTLSTAT, 0));
	OUT_RING(RADEON_RB2D_DC_FLUSH_ALL);
	RADEON_WAIT_UNTIL_2D_IDLE();
	ADVANCE_RING();

	COMMIT_RING();
	return;
}

int radeon_move_zero_fill(struct drm_buffer_object * bo,
			  int evict, int no_wait, struct drm_bo_mem_reg *new_mem)
{
	struct drm_device *dev = bo->dev;
	drm_radeon_private_t *dev_priv = dev->dev_private;
	uint32_t new_start;

	new_start = new_mem->mm_node->start << PAGE_SHIFT;

	if (new_mem->mem_type == DRM_BO_MEM_VRAM)
		new_start += dev_priv->fb_location;

	radeon_emit_solid_fill(bo->dev,
			       new_start,
			       new_mem->num_pages, 0);

	/* invalidate the chip caches */

	return drm_bo_move_accel_cleanup(bo, 1, no_wait, 0,
					 DRM_FENCE_TYPE_EXE, 0,
					 new_mem);
}

static int radeon_move_flip(struct drm_buffer_object * bo,
			    int evict, int no_wait, struct drm_bo_mem_reg * new_mem)
{
	struct drm_device *dev = bo->dev;
	struct drm_bo_mem_reg tmp_mem;
	int ret;

	tmp_mem = *new_mem;
	tmp_mem.mm_node = NULL;
	tmp_mem.proposed_flags = DRM_BO_FLAG_MEM_TT;

	ret = drm_bo_mem_space(bo, &tmp_mem, no_wait);
	if (ret)
		return ret;

	ret = drm_ttm_bind(bo->ttm, &tmp_mem);
	if (ret)
		goto out_cleanup;

	ret = radeon_move_blit(bo, 1, no_wait, &tmp_mem, &bo->mem);
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

static int radeon_move_vram(struct drm_buffer_object * bo, 
                            int evict, int no_wait, struct drm_bo_mem_reg * new_mem) 
{ 
	struct drm_device *dev = bo->dev; 
	struct drm_bo_mem_reg tmp_mem;
	struct drm_bo_mem_reg *old_mem = &bo->mem;
	int ret; 
	bool was_local = false;
 
	/* old - LOCAL memory node bo->mem
	   tmp - TT type memory node
	   new - VRAM memory node */

	tmp_mem = *old_mem;
	tmp_mem.mm_node = NULL; 

	if (old_mem->mem_type == DRM_BO_MEM_LOCAL) {
		tmp_mem.proposed_flags = DRM_BO_FLAG_MEM_TT; 
 
		ret = drm_bo_mem_space(bo, &tmp_mem, no_wait); 
		if (ret) 
			return ret; 
 	}

	if (!bo->ttm) {
		ret = drm_bo_add_ttm(bo);
		if (ret)
			goto out_cleanup;
	}

	if (old_mem->mem_type == DRM_BO_MEM_LOCAL) {
		ret = drm_bo_move_ttm(bo, evict, no_wait, &tmp_mem);
		if (ret)
			return ret;
	}

	ret = radeon_move_blit(bo, 1, no_wait, new_mem, &bo->mem);
	if (ret) 
		goto out_cleanup; 
 
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

int radeon_move(struct drm_buffer_object * bo,
		int evict, int no_wait, struct drm_bo_mem_reg *new_mem)
{
	struct drm_device *dev = bo->dev;
	struct drm_bo_mem_reg *old_mem = &bo->mem;
	drm_radeon_private_t *dev_priv = dev->dev_private;

    	if (!dev_priv->cp_running)
		goto fallback;
	
	if (bo->mem.flags & DRM_BO_FLAG_CLEAN) /* need to implement solid fill */
	{
		if (radeon_move_zero_fill(bo, evict, no_wait, new_mem))
			return drm_bo_move_zero(bo, evict, no_wait, new_mem);
		return 0; 
	}

	if (new_mem->mem_type == DRM_BO_MEM_VRAM) {
		if (radeon_move_vram(bo, evict, no_wait, new_mem))
			goto fallback;
	} else if (new_mem->mem_type == DRM_BO_MEM_LOCAL){ 
		if (radeon_move_flip(bo, evict, no_wait, new_mem))
			goto fallback;
	} else {
		if (radeon_move_flip(bo, evict, no_wait, new_mem))
			goto fallback;
	}
	return 0;
fallback:
	if (bo->mem.flags & DRM_BO_FLAG_CLEAN)
		return drm_bo_move_zero(bo, evict, no_wait, new_mem);
	else
		return drm_bo_move_memcpy(bo, evict, no_wait, new_mem);
}


/*
 * i915_evict_flags:
 *
 * @bo: the buffer object to be evicted
 *
 * Return the bo flags for a buffer which is not mapped to the hardware.
 * These will be placed in proposed_flags so that when the move is
 * finished, they'll end up in bo->mem.flags
 */
uint64_t radeon_evict_flags(struct drm_buffer_object *bo)
{
	switch (bo->mem.mem_type) {
	case DRM_BO_MEM_LOCAL:
	case DRM_BO_MEM_TT:
		return DRM_BO_FLAG_MEM_LOCAL;
	default:
		return DRM_BO_FLAG_MEM_TT;
	}
}
