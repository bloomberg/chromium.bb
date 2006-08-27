/**************************************************************************
 * 
 * Copyright 2006 Tungsten Graphics, Inc., Bismarck, ND., USA
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
 * Authors: Thomas Hellström <thomas-at-tungstengraphics-dot-com>
 */

#include "drmP.h"

/*
 * Buffer object locking policy:
 * Lock dev->struct_mutex;
 * Increase usage
 * Unlock dev->struct_mutex;
 * Lock buffer->mutex;
 * Do whatever you want;
 * Unlock buffer->mutex;
 * Decrease usage. Call destruction if zero.
 *
 * User object visibility ups usage just once, since it has its own 
 * refcounting.
 *
 * Destruction:
 * lock dev->struct_mutex;
 * Verify that usage is zero. Otherwise unlock and continue.
 * Destroy object.
 * unlock dev->struct_mutex;
 *
 * Mutex and spinlock locking orders:
 * 1.) Buffer mutex
 * 2.) Refer to ttm locking orders.
 */

int drm_fence_buffer_objects(drm_file_t *priv)
{
	drm_device_t *dev = priv->head->dev;
	drm_buffer_manager_t *bm = &dev->bm;

	drm_buffer_object_t *entry, *next;
	uint32_t fence_flags = 0;
	int count = 0;
	drm_fence_object_t *fence;
	int ret;

	mutex_lock(&bm->bm_mutex);

	list_for_each_entry(entry, &bm->unfenced, head) {
                BUG_ON(!entry->unfenced);
		fence_flags |= entry->fence_flags;
		count++;
	}

	if (!count) {
		mutex_unlock(&bm->bm_mutex);
		return 0;
	}

	fence = drm_calloc(1, sizeof(*fence), DRM_MEM_FENCE);

	if (!fence) {
		mutex_unlock(&bm->bm_mutex);
		return -ENOMEM;
	}
	
	ret = drm_fence_object_init(dev, fence_flags, 1, fence);
	if (ret) {
		drm_free(fence, sizeof(*fence), DRM_MEM_FENCE);
		mutex_unlock(&bm->bm_mutex);
		return ret;
	}

	list_for_each_entry_safe(entry, next, &bm->unfenced, head) {
		BUG_ON(entry->fence);
		entry->unfenced = 0;
		entry->fence = fence;
		list_del_init(&entry->head);

		if (!(entry->flags & DRM_BO_FLAG_NO_EVICT)) {
			if (entry->flags & DRM_BO_FLAG_MEM_TT) {
				list_add_tail(&entry->head, &bm->tt_lru);
			} else if (entry->flags & DRM_BO_FLAG_MEM_VRAM) {
				list_add_tail(&entry->head, &bm->vram_lru);
			}
		}
	}

	mutex_lock(&dev->struct_mutex);
	atomic_add(count - 1, &fence->usage);
	mutex_unlock(&dev->struct_mutex);
	mutex_unlock(&bm->bm_mutex);
	return 0;
}

/*
 * bm locked,
 * dev locked.
 */


static int drm_move_tt_to_local(drm_buffer_object_t *buf, int lazy)
{
	drm_device_t *dev = buf->dev;
	drm_buffer_manager_t *bm = &dev->bm;
	int ret = 0;
	
	BUG_ON(!buf->tt);

	if (buf->fence) {
		ret = drm_fence_object_wait(dev, buf->fence, lazy, !lazy, 
					    buf->fence_flags);
		if (ret)
			return ret;
		drm_fence_usage_deref_unlocked(dev, buf->fence);
		buf->fence = NULL;
	}
	
	drm_unbind_ttm_region(buf->ttm_region);
	drm_mm_put_block(&bm->tt_manager, buf->tt);
	buf->tt = NULL;
	buf->flags &= ~(DRM_BO_FLAG_MEM_TT | DRM_BO_FLAG_UNCACHED);
	buf->flags |= DRM_BO_FLAG_MEM_LOCAL;

	return 0;
}


void destroy_buffer_object(drm_device_t *dev, drm_buffer_object_t *bo)
{
	
	drm_buffer_manager_t *bm = &dev->bm;

	BUG_ON(bo->unfenced);

	if (bo->fence) {
		if (!drm_fence_object_signaled(bo->fence, bo->fence_flags)) {
			drm_fence_object_flush(dev, bo->fence, bo->fence_flags);
			list_add_tail(&bo->ddestroy, &bm->ddestroy);
			return;
		} else {
			drm_fence_usage_deref_locked(dev, bo->fence);
			bo->fence = NULL;
		}
	}

	/*
	 * Take away from lru lists.
	 */

	list_del_init(&bo->head);

	if (bo->tt) {
		int ret;
		ret = drm_move_tt_to_local(bo, 0);
		BUG_ON(ret);
	}
	if (bo->vram) {
		drm_mm_put_block(&bm->vram_manager, bo->vram);
		bo->vram = NULL;
	}

	/*
	 * FIXME: Destroy ttm.
	 */

	drm_free(bo, sizeof(*bo), DRM_MEM_BUFOBJ);
}

static int drm_bo_new_flags(uint32_t flags, uint32_t new_mask, uint32_t hint,
			    int init, uint32_t *n_flags)
{
	uint32_t new_flags;
	uint32_t new_props;
	
	if (!(flags & new_mask & DRM_BO_MASK_MEM) || init) {

		/*
		 * We need to move memory. Default preferences are hard-coded
		 * here.
		 */

		new_flags = new_mask & DRM_BO_MASK_MEM;

		if (!new_flags) {
			DRM_ERROR("Invalid buffer object memory flags\n");
			return -EINVAL;
		}
		
		if (new_flags & DRM_BO_FLAG_MEM_LOCAL) {
			if ((hint & DRM_BO_HINT_AVOID_LOCAL) && 
			    new_flags & (DRM_BO_FLAG_MEM_VRAM | DRM_BO_FLAG_MEM_TT)) {
				new_flags &= ~DRM_BO_FLAG_MEM_LOCAL;
			} else {
				new_flags = DRM_BO_FLAG_MEM_LOCAL;
			}
		}
		if (new_flags & DRM_BO_FLAG_MEM_TT) {
			if ((hint & DRM_BO_HINT_PREFER_VRAM) &&
			    new_flags & DRM_BO_FLAG_MEM_VRAM) {
				new_flags = DRM_BO_FLAG_MEM_VRAM;
			} else {
				new_flags = DRM_BO_FLAG_MEM_TT;
			}
		}
	} else {
		new_flags = flags & DRM_BO_MASK_MEM;
	}
	
	new_props = new_mask & (DRM_BO_FLAG_EXE | DRM_BO_FLAG_WRITE |
				DRM_BO_FLAG_READ);

	if (!new_props) {
		DRM_ERROR("Invalid buffer object rwx properties\n");
		return -EINVAL;
	}

	new_flags |= new_mask & ~DRM_BO_MASK_MEM;
	*n_flags = new_flags;
	return 0;
}
		    


#if 0

static int drm_bo_evict(drm_device_t *dev, drm_buffer_object_t *buf, int tt);
{
	int ret;
	if (tt) {
		ret = drm_move_tt_to_local(buf);
	} else {
		ret = drm_move_vram_to_local(buf);
	}
	return ret;
}
		
int drm_bo_alloc_space(drm_device_t *dev, int tt, drm_buffer_object_t *buf)
{
	drm_mm_node_t *node;
	drm_buffer_manager_t *bm = &dev->bm;
	drm_buffer_object_t *bo;
	drm_mm_t *mm = (tt) ? &bm->tt_manager : &bm->vram_manager;

	lru = (tt) ? &bm->tt_lru : &bm->vram_lru;

	do {
		node = drm_mm_search_free(mm, size, 0, 1);
		if (node)
			break;

		if (lru->next == lru) 
			break;

		if (tt) {
			bo = list_entry(lru->next, drm_buffer_object_t, tt_lru);
		} else {
			bo = list_entry(lru->next, drm_buffer_object_t, vram_lru);
		}

		drm_bo_evict(dev, bo, tt);
	} while (1);

	if (!node) {
		DRM_ERROR("Out of aperture space\n");
		return -ENOMEM;
	}

	node = drm_mm_get_block(node, size, 0);
	BUG_ON(!node);
	node->private = (void *)buf;

	if (tt) {
		buf->tt = node;
	} else {
		buf->vram = node;
	}
	return 0;
}
#endif
	

int drm_bo_ioctl(DRM_IOCTL_ARGS)
{
	DRM_DEVICE;
	drm_bo_arg_t arg;
	unsigned long data_ptr;
	(void) dev;
	return 0;
}
	
	
	
		
