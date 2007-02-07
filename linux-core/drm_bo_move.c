/**************************************************************************
 * 
 * Copyright 2007 Tungsten Graphics, Inc., Cedar Park, TX., USA
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
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS, AUTHORS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR 
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE 
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 **************************************************************************/
/*
 * Authors: Thomas Hellström <thomas-at-tungstengraphics-dot-com>
 */

#include "drmP.h"

int drm_bo_move_ttm(drm_device_t *dev,
		    drm_ttm_t *ttm, 
		    int evict,
		    int no_wait,
		    drm_bo_mem_reg_t *old_mem,
		    drm_bo_mem_reg_t *new_mem)
{
	uint32_t save_flags = old_mem->flags;
	uint32_t save_mask = old_mem->mask;
	int ret;

	if (old_mem->mem_type == DRM_BO_MEM_TT) {

		if (evict)
			drm_ttm_evict(ttm);
		else
			drm_ttm_unbind(ttm);

		mutex_lock(&dev->struct_mutex);
		drm_mm_put_block(old_mem->mm_node);
		mutex_unlock(&dev->struct_mutex);
		save_flags |= DRM_BO_FLAG_CACHED;

	} else {

		ret = drm_bind_ttm(ttm, 
				   new_mem->flags & DRM_BO_FLAG_BIND_CACHED, 
				   new_mem->mm_node->start);
		if (ret)
			return ret;

		if (!(new_mem->flags & DRM_BO_FLAG_BIND_CACHED)) {
			save_flags &= ~DRM_BO_FLAG_CACHED;
		}

	}

	*old_mem = *new_mem;
	new_mem->mm_node = NULL;
	old_mem->mask = save_mask;
	DRM_MASK_VAL(save_flags, new_mem->flags, DRM_BO_MASK_MEM);
	return 0;
}
