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

#define DRM_FLAG_MASKED(_old, _new, _mask) {\
(_old) ^= (((_old) ^ (_new)) & (_mask)); \
}

/*
 * bo locked.
 */

static int drm_move_tt_to_local(drm_buffer_object_t * buf, int evict)
{
	drm_device_t *dev = buf->dev;
	drm_buffer_manager_t *bm = &dev->bm;

	BUG_ON(!buf->tt);

	mutex_lock(&dev->struct_mutex);
	if (evict)
		drm_evict_ttm(buf->ttm);
	else
		drm_unbind_ttm(buf->ttm);
	drm_mm_put_block(&bm->tt_manager, buf->tt);
	buf->tt = NULL;

	buf->flags &= ~DRM_BO_MASK_MEM;
	buf->flags |= DRM_BO_FLAG_MEM_LOCAL | DRM_BO_FLAG_CACHED;
	mutex_unlock(&dev->struct_mutex);

	return 0;
}

/*
 * Lock dev->struct_mutex
 */

static void drm_bo_destroy_locked(drm_device_t * dev, drm_buffer_object_t * bo)
{

	drm_buffer_manager_t *bm = &dev->bm;

	DRM_FLAG_MASKED(bo->priv_flags, 0, _DRM_BO_FLAG_UNFENCED);

	/*
	 * Somone might try to access us through the still active BM lists.
	 */

	if (atomic_read(&bo->usage) != 0) 
		return;
	if (!list_empty(&bo->ddestroy))
		return;

	if (bo->fence) {
		if (!drm_fence_object_signaled(bo->fence, bo->fence_type)) {

			drm_fence_object_flush(dev, bo->fence, bo->fence_type);
			list_add_tail(&bo->ddestroy, &bm->ddestroy);
			schedule_delayed_work(&bm->wq,
					      ((DRM_HZ / 100) <
					       1) ? 1 : DRM_HZ / 100);
			return;
		} else {
			drm_fence_usage_deref_locked(dev, bo->fence);
			bo->fence = NULL;
		}
	}
	/*
	 * Take away from lru lists.
	 */

	list_del_init(&bo->tt_lru);
	list_del_init(&bo->vram_lru);

	if (bo->tt) {

		/*
		 * This temporarily unlocks struct_mutex. 
		 */

		drm_unbind_ttm(bo->ttm);
		drm_mm_put_block(&bm->tt_manager, bo->tt);
		bo->tt = NULL;
	}
	if (bo->vram) {
		drm_mm_put_block(&bm->vram_manager, bo->vram);
		bo->vram = NULL;
	}
	if (bo->ttm_object) {
		drm_ttm_object_deref_locked(dev, bo->ttm_object);
	}
	atomic_dec(&bm->count);
	drm_free(bo, sizeof(*bo), DRM_MEM_BUFOBJ);
}

static void drm_bo_delayed_delete(drm_device_t * dev)
{
	drm_buffer_manager_t *bm = &dev->bm;

	drm_buffer_object_t *entry, *nentry;
	struct list_head *list, *next;
	drm_fence_object_t *fence;

	mutex_lock(&dev->struct_mutex);
	if (!bm->initialized) 
	    goto out;

	list = bm->ddestroy.next;
	list_for_each_safe(list, next, &bm->ddestroy) {
		entry = list_entry(list, drm_buffer_object_t, ddestroy);
		nentry = NULL;

		/*
		 * Another process may claim access to this entry through the
		 * lru lists. In that case, just skip it.
		 */

		if (atomic_read(&entry->usage) != 0)
			continue;
			
		/*
		 * Since we're the only users, No need to take the 
		 * bo->mutex to watch the fence.
		 */

		fence = entry->fence;
		if (fence && drm_fence_object_signaled(fence,
						       entry->fence_type)) {
			drm_fence_usage_deref_locked(dev, fence);
			entry->fence = NULL;
		}

		if (!entry->fence) {

			/*
			 * Protect the "next" entry against destruction in case
			 * drm_bo_destroy_locked temporarily releases the
			 * struct_mutex;
			 */
			
			nentry = NULL;		
			if (next != &bm->ddestroy) {
				nentry = list_entry(next, drm_buffer_object_t, 
						    ddestroy);
				atomic_inc(&nentry->usage);
			}
			DRM_DEBUG("Destroying delayed buffer object\n");
			list_del_init(&entry->ddestroy);
			drm_bo_destroy_locked(dev, entry);
			if (next != &bm->ddestroy)
				atomic_dec(&nentry->usage);
		} 
	}
  out:
	mutex_unlock(&dev->struct_mutex);
}


static void drm_bo_delayed_workqueue(void *data)
{
	drm_device_t *dev = (drm_device_t *) data;
	drm_buffer_manager_t *bm = &dev->bm;

	DRM_DEBUG("Delayed delete Worker\n");

	drm_bo_delayed_delete(dev);
	mutex_lock(&dev->struct_mutex);
	if (!list_empty(&bm->ddestroy)) {
		schedule_delayed_work(&bm->wq,
				      ((DRM_HZ / 100) < 1) ? 1 : DRM_HZ / 100);
	}
	mutex_unlock(&dev->struct_mutex);
}

void drm_bo_usage_deref_locked(drm_device_t * dev, drm_buffer_object_t * bo)
{
	if (atomic_dec_and_test(&bo->usage)) {
		drm_bo_destroy_locked(dev, bo);
	}
}

static void drm_bo_base_deref_locked(drm_file_t * priv, drm_user_object_t * uo)
{
	drm_bo_usage_deref_locked(priv->head->dev,
				  drm_user_object_entry(uo, drm_buffer_object_t,
							base));
}

void drm_bo_usage_deref_unlocked(drm_device_t * dev, drm_buffer_object_t * bo)
{
	if (atomic_dec_and_test(&bo->usage)) {
		mutex_lock(&dev->struct_mutex);
		if (atomic_read(&bo->usage) == 0)
			drm_bo_destroy_locked(dev, bo);
		mutex_unlock(&dev->struct_mutex);
	}
}

/*
 * Note. The caller has to register (if applicable) 
 * and deregister fence object usage.
 */

int drm_fence_buffer_objects(drm_file_t * priv,
			     struct list_head *list,
			     uint32_t fence_flags,
			     drm_fence_object_t * fence,
			     drm_fence_object_t ** used_fence)
{
	drm_device_t *dev = priv->head->dev;
	drm_buffer_manager_t *bm = &dev->bm;

	drm_buffer_object_t *entry;
	uint32_t fence_type = 0;
	int count = 0;
	int ret = 0;
	struct list_head f_list, *l;
	struct list_head *q;

	mutex_lock(&dev->struct_mutex);

	if (!list)
		list = &bm->unfenced;

	list_for_each_entry(entry, list, tt_lru) {
		BUG_ON(!(entry->priv_flags & _DRM_BO_FLAG_UNFENCED));
		fence_type |= entry->fence_type;
		if (entry->fence_class != 0) {
			DRM_ERROR("Fence class %d is not implemented yet.\n",
				  entry->fence_class);
			ret = -EINVAL;
			goto out;
		}
		count++;
	}

	if (!count) {
		DRM_ERROR("No buffers to fence\n");
		ret = -EINVAL;
		goto out;
	}

	/*
	 * Transfer to a local list before we release the dev->struct_mutex;
	 * This is so we don't get any new unfenced objects while fencing 
	 * the ones we already have..
	 */

	list_add_tail(&f_list, list);
	list_del_init(list);

	if (fence) {
		if ((fence_type & fence->type) != fence_type) {
			DRM_ERROR("Given fence doesn't match buffers "
				  "on unfenced list.\n");
			ret = -EINVAL;
			goto out;
		}
	} else {
		mutex_unlock(&dev->struct_mutex);
		ret = drm_fence_object_create(dev, fence_type, 
					      fence_flags | DRM_FENCE_FLAG_EMIT, 
					      &fence);
		mutex_lock(&dev->struct_mutex);
		if (ret)
			goto out;
	}

	count = 0;
	l = f_list.next;
	while (l != &f_list) {
		entry = list_entry(l, drm_buffer_object_t, tt_lru);
		atomic_inc(&entry->usage);
		mutex_unlock(&dev->struct_mutex);
		mutex_lock(&entry->mutex);
		mutex_lock(&dev->struct_mutex);
		list_del_init(l);
		list_del_init(&entry->vram_lru);
		if (entry->priv_flags & _DRM_BO_FLAG_UNFENCED) {
			count++;
			if (entry->fence)
				drm_fence_usage_deref_locked(dev, entry->fence);
			entry->fence = fence;
			DRM_FLAG_MASKED(entry->priv_flags, 0,
					_DRM_BO_FLAG_UNFENCED);
			DRM_WAKEUP(&entry->event_queue);
			if (entry->flags & DRM_BO_FLAG_MEM_TT) {
				q = (entry->flags & DRM_BO_FLAG_NO_EVICT) ?
					&bm->tt_pinned : &bm->tt_lru;
				list_add_tail(&entry->tt_lru, q);
			} else if (entry->flags & DRM_BO_FLAG_MEM_VRAM) {
				q = (entry->flags & DRM_BO_FLAG_NO_EVICT) ?
					&bm->vram_pinned : &bm->vram_lru;
				list_add_tail(&entry->vram_lru, q);
			} else {
				list_add_tail(&entry->tt_lru, &bm->other);
			} 	
		}
		mutex_unlock(&entry->mutex);
		drm_bo_usage_deref_locked(dev, entry);
		l = f_list.next;
	}
	atomic_add(count, &fence->usage);
	DRM_DEBUG("Fenced %d buffers\n", count);
      out:
	mutex_unlock(&dev->struct_mutex);
	*used_fence = fence;
	return ret;
}

EXPORT_SYMBOL(drm_fence_buffer_objects);

/*
 * Call bo->mutex locked.
 * Wait until the buffer is idle.
 */

static int drm_bo_wait(drm_buffer_object_t * bo, int lazy, int ignore_signals,
		       int no_wait)
{

	drm_fence_object_t *fence = bo->fence;
	int ret;

	if (fence) {
		drm_device_t *dev = bo->dev;
		if (drm_fence_object_signaled(fence, bo->fence_type)) {
			drm_fence_usage_deref_unlocked(dev, fence);
			bo->fence = NULL;
			return 0;
		}
		if (no_wait) {
			return -EBUSY;
		}
		ret =
		    drm_fence_object_wait(dev, fence, lazy, ignore_signals,
					  bo->fence_type);
		if (ret) 
		    return ret;
		
		drm_fence_usage_deref_unlocked(dev, fence);
		bo->fence = NULL;

	}
	return 0;
}

/*
 * bo->mutex locked 
 */

static int drm_bo_evict(drm_buffer_object_t * bo, int tt, int no_wait)
{
	int ret = 0;
	drm_device_t *dev = bo->dev;
	drm_buffer_manager_t *bm = &dev->bm;
	/*
	 * Someone might have modified the buffer before we took the buffer mutex.
	 */

	if (bo->priv_flags & _DRM_BO_FLAG_UNFENCED)
		goto out;
	if (tt && !bo->tt)
		goto out;
	if (!tt && !bo->vram)
		goto out;

	ret = drm_bo_wait(bo, 0, 0, no_wait);
	if (ret) {
		if (ret != -EAGAIN)
			DRM_ERROR("Failed to expire fence before "
				  "buffer eviction.\n");
		goto out;
	}

	if (tt) {
		ret = drm_move_tt_to_local(bo, 1);
	}
#if 0
	else {
		ret = drm_move_vram_to_local(bo);
	}
#endif
	mutex_lock(&dev->struct_mutex);
	list_del_init((tt) ? &bo->tt_lru : &bo->vram_lru);
	if (list_empty((tt) ? &bo->vram_lru : &bo->tt_lru))
		list_add_tail(&bo->tt_lru, &bm->other);
	mutex_unlock(&dev->struct_mutex);
	DRM_FLAG_MASKED(bo->priv_flags, _DRM_BO_FLAG_EVICTED,
			_DRM_BO_FLAG_EVICTED);
      out:
	return ret;
}

/*
 * buf->mutex locked.
 */

int drm_bo_alloc_space(drm_buffer_object_t * buf, int tt, int no_wait)
{
	drm_device_t *dev = buf->dev;
	drm_mm_node_t *node;
	drm_buffer_manager_t *bm = &dev->bm;
	drm_buffer_object_t *bo;
	drm_mm_t *mm = (tt) ? &bm->tt_manager : &bm->vram_manager;
	struct list_head *lru;
	unsigned long size = buf->num_pages;
	int ret;

	mutex_lock(&dev->struct_mutex);
	do {
		node = drm_mm_search_free(mm, size, 0, 1);
		if (node)
			break;

		lru = (tt) ? &bm->tt_lru : &bm->vram_lru;
		if (lru->next == lru)
			break;

		if (tt) {
			bo = list_entry(lru->next, drm_buffer_object_t, tt_lru);
		} else {
			bo = list_entry(lru->next, drm_buffer_object_t, vram_lru);
		}

		atomic_inc(&bo->usage);
		mutex_unlock(&dev->struct_mutex);
		mutex_lock(&bo->mutex);
		ret = drm_bo_evict(bo, tt, no_wait);
		mutex_unlock(&bo->mutex);
		drm_bo_usage_deref_unlocked(dev, bo);
		if (ret)
			return ret;
		mutex_lock(&dev->struct_mutex);
	} while (1);

	if (!node) {
		DRM_ERROR("Out of videoram / aperture space\n");
		mutex_unlock(&dev->struct_mutex);
		return -ENOMEM;
	}

	node = drm_mm_get_block(node, size, 0);
	mutex_unlock(&dev->struct_mutex);
	BUG_ON(!node);
	node->private = (void *)buf;

	if (tt) {
		buf->tt = node;
	} else {
		buf->vram = node;
	}
	buf->offset = node->start * PAGE_SIZE;
	return 0;
}

static int drm_move_local_to_tt(drm_buffer_object_t * bo, int no_wait)
{
	drm_device_t *dev = bo->dev;
	drm_buffer_manager_t *bm = &dev->bm;
	drm_ttm_backend_t *be;
	int ret;

	BUG_ON(bo->tt);
	ret = drm_bo_alloc_space(bo, 1, no_wait);

	if (ret)
		return ret;
	DRM_DEBUG("Flipping in to AGP 0x%08lx\n", bo->tt->start);
	mutex_lock(&dev->struct_mutex);
	ret = drm_bind_ttm(bo->ttm, bo->tt->start);
	if (ret) {
		drm_mm_put_block(&bm->tt_manager, bo->tt);
	}
	mutex_unlock(&dev->struct_mutex);
	if (ret)
		return ret;

	be = bo->ttm->be;
	if (be->needs_cache_adjust(be))
		bo->flags &= ~DRM_BO_FLAG_CACHED;
	bo->flags &= ~DRM_BO_MASK_MEM;
	bo->flags |= DRM_BO_FLAG_MEM_TT;

	if (bo->priv_flags & _DRM_BO_FLAG_EVICTED) {
		ret = dev->driver->bo_driver->invalidate_caches(dev, bo->flags);
		if (ret)
			DRM_ERROR("Could not flush read caches\n");
	}
	DRM_FLAG_MASKED(bo->priv_flags, 0, _DRM_BO_FLAG_EVICTED);

	return 0;
}

static int drm_bo_new_flags(drm_device_t * dev,
			    uint32_t flags, uint32_t new_mask, uint32_t hint,
			    int init, uint32_t * n_flags, uint32_t * n_mask)
{
	uint32_t new_flags = 0;
	uint32_t new_props;
	drm_bo_driver_t *driver = dev->driver->bo_driver;
	drm_buffer_manager_t *bm = &dev->bm;

	/*
	 * First adjust the mask. 
	 */

	if (!bm->use_vram)
		new_mask &= ~DRM_BO_FLAG_MEM_VRAM;
	if (!bm->use_tt)
		new_mask &= ~DRM_BO_FLAG_MEM_TT;

	if (new_mask & DRM_BO_FLAG_BIND_CACHED) {
		if (((new_mask & DRM_BO_FLAG_MEM_TT) && !driver->cached_tt) &&
		    ((new_mask & DRM_BO_FLAG_MEM_VRAM)
		     && !driver->cached_vram)) {
			new_mask &= ~DRM_BO_FLAG_BIND_CACHED;
		} else {
			if (!driver->cached_tt)
				new_flags &= DRM_BO_FLAG_MEM_TT;
			if (!driver->cached_vram)
				new_flags &= DRM_BO_FLAG_MEM_VRAM;
		}
	}

	if ((new_mask & DRM_BO_FLAG_READ_CACHED) &&
	    !(new_mask & DRM_BO_FLAG_BIND_CACHED)) {
		if ((new_mask & DRM_BO_FLAG_NO_EVICT) &&
		    !(new_mask & DRM_BO_FLAG_MEM_LOCAL)) {
			DRM_ERROR
			    ("Cannot read cached from a pinned VRAM / TT buffer\n");
			return -EINVAL;
		}
	}

	/*
	 * Determine new memory location:
	 */

	if (!(flags & new_mask & DRM_BO_MASK_MEM) || init) {

		new_flags = new_mask & DRM_BO_MASK_MEM;

		if (!new_flags) {
			DRM_ERROR("Invalid buffer object memory flags\n");
			return -EINVAL;
		}

		if (new_flags & DRM_BO_FLAG_MEM_LOCAL) {
			if ((hint & DRM_BO_HINT_AVOID_LOCAL) &&
			    new_flags & (DRM_BO_FLAG_MEM_VRAM |
					 DRM_BO_FLAG_MEM_TT)) {
				new_flags &= ~DRM_BO_FLAG_MEM_LOCAL;
			} else {
				new_flags = DRM_BO_FLAG_MEM_LOCAL;
			}
		}
		if (new_flags & DRM_BO_FLAG_MEM_TT) {
			if ((new_mask & DRM_BO_FLAG_PREFER_VRAM) &&
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

	if (((flags ^ new_flags) & DRM_BO_FLAG_BIND_CACHED) &&
	    (new_flags & DRM_BO_FLAG_NO_EVICT) &&
	    (flags & (DRM_BO_FLAG_MEM_TT | DRM_BO_FLAG_MEM_VRAM))) {
		if (!(flags & DRM_BO_FLAG_CACHED)) {
			DRM_ERROR
			    ("Cannot change caching policy of pinned buffer\n");
			return -EINVAL;
		} else {
			new_flags &= ~DRM_BO_FLAG_CACHED;
		}
	}

	*n_flags = new_flags;
	*n_mask = new_mask;
	return 0;
}

/*
 * Call dev->struct_mutex locked.
 */

drm_buffer_object_t *drm_lookup_buffer_object(drm_file_t * priv,
					      uint32_t handle, int check_owner)
{
	drm_user_object_t *uo;
	drm_buffer_object_t *bo;

	uo = drm_lookup_user_object(priv, handle);

	if (!uo || (uo->type != drm_buffer_type)) {
		DRM_ERROR("Could not find buffer object 0x%08x\n", handle);
		return NULL;
	}

	if (check_owner && priv != uo->owner) {
		if (!drm_lookup_ref_object(priv, uo, _DRM_REF_USE))
			return NULL;
	}

	bo = drm_user_object_entry(uo, drm_buffer_object_t, base);
	atomic_inc(&bo->usage);
	return bo;
}

/*
 * Call bo->mutex locked.
 * Returns 1 if the buffer is currently rendered to or from. 0 otherwise.
 * Doesn't do any fence flushing as opposed to the drm_bo_busy function.
 */

static int drm_bo_quick_busy(drm_buffer_object_t * bo)
{
	drm_fence_object_t *fence = bo->fence;

	BUG_ON(bo->priv_flags & _DRM_BO_FLAG_UNFENCED);
	if (fence) {
		drm_device_t *dev = bo->dev;
		if (drm_fence_object_signaled(fence, bo->fence_type)) {
			drm_fence_usage_deref_unlocked(dev, fence);
			bo->fence = NULL;
			return 0;
		}
		return 1;
	}
	return 0;
}

/*
 * Call bo->mutex locked.
 * Returns 1 if the buffer is currently rendered to or from. 0 otherwise.
 */

static int drm_bo_busy(drm_buffer_object_t * bo)
{
	drm_fence_object_t *fence = bo->fence;

	BUG_ON(bo->priv_flags & _DRM_BO_FLAG_UNFENCED);
	if (fence) {
		drm_device_t *dev = bo->dev;
		if (drm_fence_object_signaled(fence, bo->fence_type)) {
			drm_fence_usage_deref_unlocked(dev, fence);
			bo->fence = NULL;
			return 0;
		}
		drm_fence_object_flush(dev, fence, DRM_FENCE_TYPE_EXE);
		if (drm_fence_object_signaled(fence, bo->fence_type)) {
			drm_fence_usage_deref_unlocked(dev, fence);
			bo->fence = NULL;
			return 0;
		}
		return 1;
	}
	return 0;
}

static int drm_bo_read_cached(drm_buffer_object_t * bo)
{
	int ret = 0;

	BUG_ON(bo->priv_flags & _DRM_BO_FLAG_UNFENCED);
	if (bo->vram) 
		ret = drm_bo_evict(bo, 0, 1);
	if (ret)
		return ret;
	if (bo->tt)
		ret = drm_bo_evict(bo, 1, 1);	
	return ret;
}

/*
 * Wait until a buffer is unmapped.
 */

static int drm_bo_wait_unmapped(drm_buffer_object_t * bo, int no_wait)
{
	int ret = 0;

	if ((atomic_read(&bo->mapped) >= 0) && no_wait)
		return -EBUSY;

	DRM_WAIT_ON(ret, bo->event_queue, 3 * DRM_HZ,
		    atomic_read(&bo->mapped) == -1);

	if (ret == -EINTR)
		ret = -EAGAIN;

	return ret;
}

static int drm_bo_check_unfenced(drm_buffer_object_t * bo)
{
	int ret;

	mutex_lock(&bo->mutex);
	ret = (bo->priv_flags & _DRM_BO_FLAG_UNFENCED);
	mutex_unlock(&bo->mutex);
	return ret;
}

/*
 * Wait until a buffer, scheduled to be fenced moves off the unfenced list.
 * Until then, we cannot really do anything with it except delete it.
 * The unfenced list is a PITA, and the operations
 * 1) validating
 * 2) submitting commands
 * 3) fencing
 * Should really be an atomic operation. 
 * We now "solve" this problem by keeping
 * the buffer "unfenced" after validating, but before fencing.
 */

static int drm_bo_wait_unfenced(drm_buffer_object_t * bo, int no_wait,
				int eagain_if_wait)
{
	int ret = (bo->priv_flags & _DRM_BO_FLAG_UNFENCED);
	unsigned long _end = jiffies + 3 * DRM_HZ;

	if (ret && no_wait)
		return -EBUSY;
	else if (!ret)
		return 0;

	do {
		mutex_unlock(&bo->mutex);
		DRM_WAIT_ON(ret, bo->event_queue, 3 * DRM_HZ,
			    !drm_bo_check_unfenced(bo));
		mutex_lock(&bo->mutex);
		if (ret == -EINTR)
			return -EAGAIN;
		if (ret) {
			DRM_ERROR
			    ("Error waiting for buffer to become fenced\n");
			return ret;
		}
		ret = (bo->priv_flags & _DRM_BO_FLAG_UNFENCED);
	} while (ret && !time_after_eq(jiffies, _end));
	if (ret) {
		DRM_ERROR("Timeout waiting for buffer to become fenced\n");
		return ret;
	}
	if (eagain_if_wait)
		return -EAGAIN;

	return 0;
}

/*
 * Fill in the ioctl reply argument with buffer info.
 * Bo locked. 
 */

static void drm_bo_fill_rep_arg(drm_buffer_object_t * bo,
				drm_bo_arg_reply_t * rep)
{
	rep->handle = bo->base.hash.key;
	rep->flags = bo->flags;
	rep->size = bo->num_pages * PAGE_SIZE;
	rep->offset = bo->offset;

	if (bo->ttm_object) {
		rep->arg_handle = bo->ttm_object->map_list.user_token;
	} else {
		rep->arg_handle = 0;
	}

	rep->mask = bo->mask;
	rep->buffer_start = bo->buffer_start;
	rep->fence_flags = bo->fence_type;
	rep->rep_flags = 0;

	if ((bo->priv_flags & _DRM_BO_FLAG_UNFENCED) || drm_bo_quick_busy(bo)) {
		DRM_FLAG_MASKED(rep->rep_flags, DRM_BO_REP_BUSY,
				DRM_BO_REP_BUSY);
	}
}

/*
 * Wait for buffer idle and register that we've mapped the buffer.
 * Mapping is registered as a drm_ref_object with type _DRM_REF_TYPE1, 
 * so that if the client dies, the mapping is automatically 
 * unregistered.
 */

static int drm_buffer_object_map(drm_file_t * priv, uint32_t handle,
				 uint32_t map_flags, unsigned hint,
				 drm_bo_arg_reply_t * rep)
{
	drm_buffer_object_t *bo;
	drm_device_t *dev = priv->head->dev;
	int ret = 0;
	int no_wait = hint & DRM_BO_HINT_DONT_BLOCK;

	mutex_lock(&dev->struct_mutex);
	bo = drm_lookup_buffer_object(priv, handle, 1);
	mutex_unlock(&dev->struct_mutex);

	if (!bo)
		return -EINVAL;

	mutex_lock(&bo->mutex);
	if (!(hint & DRM_BO_HINT_ALLOW_UNFENCED_MAP)) {
		ret = drm_bo_wait_unfenced(bo, no_wait, 0);
		if (ret)
			goto out;
	}

	/*
	 * If this returns true, we are currently unmapped.
	 * We need to do this test, because unmapping can
	 * be done without the bo->mutex held.
	 */

	while (1) {
		if (atomic_inc_and_test(&bo->mapped)) {
			if (no_wait && drm_bo_busy(bo)) {
				atomic_dec(&bo->mapped);
				ret = -EBUSY;
				goto out;
			}
			ret = drm_bo_wait(bo, 0, 0, no_wait);
			if (ret) {
				atomic_dec(&bo->mapped);
				goto out;
			}

			if ((map_flags & DRM_BO_FLAG_READ) &&
			    (bo->flags & DRM_BO_FLAG_READ_CACHED) &&
			    (!(bo->flags & DRM_BO_FLAG_CACHED))) {
				drm_bo_read_cached(bo);
			}
			break;
		} else if ((map_flags & DRM_BO_FLAG_READ) &&
			   (bo->flags & DRM_BO_FLAG_READ_CACHED) &&
			   (!(bo->flags & DRM_BO_FLAG_CACHED))) {

			/*
			 * We are already mapped with different flags.
			 * need to wait for unmap.
			 */

			ret = drm_bo_wait_unmapped(bo, no_wait);
			if (ret)
				goto out;

			continue;
		}
		break;
	}

	mutex_lock(&dev->struct_mutex);
	ret = drm_add_ref_object(priv, &bo->base, _DRM_REF_TYPE1);
	mutex_unlock(&dev->struct_mutex);
	if (ret) {
		if (atomic_add_negative(-1, &bo->mapped))
			DRM_WAKEUP(&bo->event_queue);

	} else
		drm_bo_fill_rep_arg(bo, rep);
      out:
	mutex_unlock(&bo->mutex);
	drm_bo_usage_deref_unlocked(dev, bo);
	return ret;
}

static int drm_buffer_object_unmap(drm_file_t * priv, uint32_t handle)
{
	drm_device_t *dev = priv->head->dev;
	drm_buffer_object_t *bo;
	drm_ref_object_t *ro;
	int ret = 0;

	mutex_lock(&dev->struct_mutex);

	bo = drm_lookup_buffer_object(priv, handle, 1);
	if (!bo) {
		ret = -EINVAL;
		goto out;
	}

	ro = drm_lookup_ref_object(priv, &bo->base, _DRM_REF_TYPE1);
	if (!ro) {
		ret = -EINVAL;
		goto out;
	}

	drm_remove_ref_object(priv, ro);
	drm_bo_usage_deref_locked(dev, bo);
      out:
	mutex_unlock(&dev->struct_mutex);
	return ret;
}

/*
 * Call struct-sem locked.
 */

static void drm_buffer_user_object_unmap(drm_file_t * priv,
					 drm_user_object_t * uo,
					 drm_ref_t action)
{
	drm_buffer_object_t *bo =
	    drm_user_object_entry(uo, drm_buffer_object_t, base);

	/*
	 * We DON'T want to take the bo->lock here, because we want to
	 * hold it when we wait for unmapped buffer.
	 */

	BUG_ON(action != _DRM_REF_TYPE1);

	if (atomic_add_negative(-1, &bo->mapped))
		DRM_WAKEUP(&bo->event_queue);
}

/*
 * bo->mutex locked. 
 */

static int drm_bo_move_buffer(drm_buffer_object_t * bo, uint32_t new_flags,
			      int no_wait)
{
	int ret = 0;

	/*
	 * Flush outstanding fences.
	 */
	drm_bo_busy(bo);

	/*
	 * Make sure we're not mapped.
	 */

	ret = drm_bo_wait_unmapped(bo, no_wait);
	if (ret)
		return ret;

	/*
	 * Wait for outstanding fences.
	 */

	ret = drm_bo_wait(bo, 0, 0, no_wait);

	if (ret == -EINTR)
		return -EAGAIN;
	if (ret)
		return ret;

	if (new_flags & DRM_BO_FLAG_MEM_TT) {
		ret = drm_move_local_to_tt(bo, no_wait);
		if (ret)
			return ret;
	} else {
		drm_move_tt_to_local(bo, 0);
	}

	return 0;
}

/*
 * bo locked.
 */

static int drm_buffer_object_validate(drm_buffer_object_t * bo,
				      uint32_t new_flags,
				      int move_unfenced, int no_wait)
{
	drm_device_t *dev = bo->dev;
	drm_buffer_manager_t *bm = &dev->bm;
	uint32_t flag_diff = (new_flags ^ bo->flags);
	drm_bo_driver_t *driver = dev->driver->bo_driver;

	int ret;

	if (new_flags & DRM_BO_FLAG_MEM_VRAM) {
		DRM_ERROR("Vram support not implemented yet\n");
		return -EINVAL;
	}

	DRM_DEBUG("New flags 0x%08x, Old flags 0x%08x\n", new_flags, bo->flags);
	ret = driver->fence_type(new_flags, &bo->fence_class, &bo->fence_type);
	if (ret) {
		DRM_ERROR("Driver did not support given buffer permissions\n");
		return ret;
	}

	/*
	 * Check whether we need to move buffer.
	 */

	if ((bo->type != drm_bo_type_fake) && (flag_diff & DRM_BO_MASK_MEM)) {
		if (bo->type == drm_bo_type_user) {
			DRM_ERROR("User buffers are not implemented yet.\n");
			return -EINVAL;
		}
		ret = drm_bo_move_buffer(bo, new_flags, no_wait);
		if (ret) {
			if (ret != -EAGAIN)
				DRM_ERROR("Failed moving buffer.\n");
			return ret;
		}
	}

	if (move_unfenced) {

		/*
		 * Place on unfenced list.
		 */

		DRM_FLAG_MASKED(bo->priv_flags, _DRM_BO_FLAG_UNFENCED,
				_DRM_BO_FLAG_UNFENCED);
		mutex_lock(&dev->struct_mutex);
		list_del(&bo->tt_lru);
		list_add_tail(&bo->tt_lru, &bm->unfenced);
		list_del_init(&bo->vram_lru);
		mutex_unlock(&dev->struct_mutex);
	} else {
		struct list_head *q;

		mutex_lock(&dev->struct_mutex);
		list_del_init(&bo->tt_lru);
		list_del_init(&bo->vram_lru);

		if (new_flags & DRM_BO_FLAG_MEM_TT) {
			q = (new_flags & DRM_BO_FLAG_NO_EVICT) ?
				&bm->tt_pinned : &bm->tt_lru;
			list_add_tail(&bo->tt_lru, q);
		} else if (new_flags & DRM_BO_FLAG_MEM_VRAM) {
			q = (new_flags & DRM_BO_FLAG_NO_EVICT) ?
				&bm->vram_pinned : &bm->vram_lru;
			list_add_tail(&bo->vram_lru, q);
		} else {
			list_add_tail(&bo->tt_lru, &bm->other);
		} 	
		mutex_unlock(&dev->struct_mutex);
		DRM_FLAG_MASKED(bo->flags, new_flags, DRM_BO_FLAG_NO_EVICT);
	}

	bo->flags = new_flags;
	return 0;
}

static int drm_bo_handle_validate(drm_file_t * priv, uint32_t handle,
				  uint32_t flags, uint32_t mask, uint32_t hint,
				  drm_bo_arg_reply_t * rep)
{
	drm_buffer_object_t *bo;
	drm_device_t *dev = priv->head->dev;
	int ret;
	int no_wait = hint & DRM_BO_HINT_DONT_BLOCK;
	uint32_t new_flags;

	bo = drm_lookup_buffer_object(priv, handle, 1);
	if (!bo) {
		return -EINVAL;
	}

	mutex_lock(&bo->mutex);
	ret = drm_bo_wait_unfenced(bo, no_wait, 0);

	if (ret)
		goto out;

	ret = drm_bo_new_flags(dev, bo->flags,
			       (flags & mask) | (bo->mask & ~mask), hint,
			       0, &new_flags, &bo->mask);

	if (ret)
		goto out;

	ret =
	    drm_buffer_object_validate(bo, new_flags,
				       !(hint & DRM_BO_HINT_DONT_FENCE),
				       no_wait);
	drm_bo_fill_rep_arg(bo, rep);

      out:

	mutex_unlock(&bo->mutex);
	drm_bo_usage_deref_unlocked(dev, bo);
	return ret;
}

static int drm_bo_handle_info(drm_file_t * priv, uint32_t handle,
			      drm_bo_arg_reply_t * rep)
{
	drm_buffer_object_t *bo;

	bo = drm_lookup_buffer_object(priv, handle, 1);
	if (!bo) {
		return -EINVAL;
	}
	mutex_lock(&bo->mutex);
	if (!(bo->priv_flags & _DRM_BO_FLAG_UNFENCED))
		(void)drm_bo_busy(bo);
	drm_bo_fill_rep_arg(bo, rep);
	mutex_unlock(&bo->mutex);
	drm_bo_usage_deref_unlocked(bo->dev, bo);
	return 0;
}

static int drm_bo_handle_wait(drm_file_t * priv, uint32_t handle,
			      uint32_t hint, drm_bo_arg_reply_t * rep)
{
	drm_buffer_object_t *bo;
	int no_wait = hint & DRM_BO_HINT_DONT_BLOCK;
	int ret;

	bo = drm_lookup_buffer_object(priv, handle, 1);
	if (!bo) {
		return -EINVAL;
	}

	mutex_lock(&bo->mutex);
	ret = drm_bo_wait_unfenced(bo, no_wait, 0);
	if (ret)
		goto out;
	ret = drm_bo_wait(bo, hint & DRM_BO_HINT_WAIT_LAZY, 0, no_wait);
	if (ret)
		goto out;

	drm_bo_fill_rep_arg(bo, rep);

      out:
	mutex_unlock(&bo->mutex);
	drm_bo_usage_deref_unlocked(bo->dev, bo);
	return ret;
}

/*
 * Call bo->mutex locked.
 */

static int drm_bo_add_ttm(drm_file_t * priv, drm_buffer_object_t * bo)
{
	drm_device_t *dev = bo->dev;
	drm_ttm_object_t *to = NULL;
	int ret = 0;
	uint32_t ttm_flags = 0;

	bo->ttm_object = NULL;
	bo->ttm = NULL;

	switch (bo->type) {
	case drm_bo_type_dc:
		mutex_lock(&dev->struct_mutex);
		ret = drm_ttm_object_create(dev, bo->num_pages * PAGE_SIZE,
					    bo->mask & DRM_BO_FLAG_BIND_CACHED,
					    ttm_flags, &to);
		mutex_unlock(&dev->struct_mutex);
		break;
	case drm_bo_type_user:
	case drm_bo_type_fake:
		break;
	default:
		DRM_ERROR("Illegal buffer object type\n");
		ret = -EINVAL;
		break;
	}

	if (ret) {
		return ret;
	}

	if (to) {
		bo->ttm_object = to;
		bo->ttm = drm_ttm_from_object(to);
	}
	return ret;
}

int drm_buffer_object_create(drm_file_t * priv,
			     unsigned long size,
			     drm_bo_type_t type,
			     uint32_t mask,
			     uint32_t hint,
			     unsigned long buffer_start,
			     drm_buffer_object_t ** buf_obj)
{
	drm_device_t *dev = priv->head->dev;
	drm_buffer_manager_t *bm = &dev->bm;
	drm_buffer_object_t *bo;
	int ret = 0;
	uint32_t new_flags;
	unsigned long num_pages;

	drm_bo_delayed_delete(dev);

	if ((buffer_start & ~PAGE_MASK) && (type != drm_bo_type_fake)) {
		DRM_ERROR("Invalid buffer object start.\n");
		return -EINVAL;
	}
	num_pages = (size + PAGE_SIZE - 1) >> PAGE_SHIFT;
	if (num_pages == 0) {
		DRM_ERROR("Illegal buffer object size.\n");
		return -EINVAL;
	}

	bo = drm_calloc(1, sizeof(*bo), DRM_MEM_BUFOBJ);

	if (!bo)
		return -ENOMEM;

	mutex_init(&bo->mutex);
	mutex_lock(&bo->mutex);

	atomic_set(&bo->usage, 1);
	atomic_set(&bo->mapped, -1);
	DRM_INIT_WAITQUEUE(&bo->event_queue);
	INIT_LIST_HEAD(&bo->tt_lru);
	INIT_LIST_HEAD(&bo->vram_lru);
	INIT_LIST_HEAD(&bo->ddestroy);
	list_add_tail(&bo->tt_lru, &bm->other);
	bo->dev = dev;
	bo->type = type;
	bo->num_pages = num_pages;
	bo->vram = NULL;
	bo->tt = NULL;
	if (bo->type == drm_bo_type_fake) {
		bo->offset = buffer_start;
		bo->buffer_start = 0;
	} else {
		bo->buffer_start = buffer_start;
	}
	bo->priv_flags = 0;
	bo->flags = DRM_BO_FLAG_MEM_LOCAL | DRM_BO_FLAG_CACHED;
	ret = drm_bo_new_flags(dev, bo->flags, mask, hint,
			       1, &new_flags, &bo->mask);
	if (ret)
		goto out_err;
	ret = drm_bo_add_ttm(priv, bo);
	if (ret)
		goto out_err;

	ret = drm_buffer_object_validate(bo, new_flags, 0,
					 hint & DRM_BO_HINT_DONT_BLOCK);
	if (ret)
		goto out_err;

	mutex_unlock(&bo->mutex);
	*buf_obj = bo;
	atomic_inc(&bm->count);
	return 0;

      out_err:
	mutex_unlock(&bo->mutex);
	drm_free(bo, sizeof(*bo), DRM_MEM_BUFOBJ);
	return ret;
}

static int drm_bo_add_user_object(drm_file_t * priv, drm_buffer_object_t * bo,
				  int shareable)
{
	drm_device_t *dev = priv->head->dev;
	int ret;

	mutex_lock(&dev->struct_mutex);
	ret = drm_add_user_object(priv, &bo->base, shareable);
	if (ret)
		goto out;

	bo->base.remove = drm_bo_base_deref_locked;
	bo->base.type = drm_buffer_type;
	bo->base.ref_struct_locked = NULL;
	bo->base.unref = drm_buffer_user_object_unmap;

      out:
	mutex_unlock(&dev->struct_mutex);
	return ret;
}

static int drm_bo_lock_test(drm_device_t * dev, struct file *filp)
{
	LOCK_TEST_WITH_RETURN(dev, filp);
	return 0;
}

int drm_bo_ioctl(DRM_IOCTL_ARGS)
{
	DRM_DEVICE;
	drm_bo_arg_t arg;
	drm_bo_arg_request_t *req = &arg.d.req;
	drm_bo_arg_reply_t rep;
	unsigned long next;
	drm_user_object_t *uo;
	drm_buffer_object_t *entry;

	if (!dev->bm.initialized) {
		DRM_ERROR("Buffer object manager is not initialized.\n");
		return -EINVAL;
	}

	do {
		DRM_COPY_FROM_USER_IOCTL(arg, (void __user *)data, sizeof(arg));

		if (arg.handled) {
			data = arg.next;
			continue;
		}

		rep.ret = 0;
		switch (req->op) {
		case drm_bo_create:
			rep.ret =
			    drm_buffer_object_create(priv, req->size,
						     req->type,
						     req->mask,
						     req->hint,
						     req->buffer_start, &entry);
			if (rep.ret)
				break;

			rep.ret =
			    drm_bo_add_user_object(priv, entry,
						   req->
						   mask &
						   DRM_BO_FLAG_SHAREABLE);
			if (rep.ret)
				drm_bo_usage_deref_unlocked(dev, entry);

			if (rep.ret)
				break;

			mutex_lock(&entry->mutex);
			drm_bo_fill_rep_arg(entry, &rep);
			mutex_unlock(&entry->mutex);
			break;
		case drm_bo_unmap:
			rep.ret = drm_buffer_object_unmap(priv, req->handle);
			break;
		case drm_bo_map:
			rep.ret = drm_buffer_object_map(priv, req->handle,
							req->mask,
							req->hint, &rep);
			break;
		case drm_bo_destroy:
			mutex_lock(&dev->struct_mutex);
			uo = drm_lookup_user_object(priv, req->handle);
			if (!uo || (uo->type != drm_buffer_type)
			    || uo->owner != priv) {
				mutex_unlock(&dev->struct_mutex);
				rep.ret = -EINVAL;
				break;
			}
			rep.ret = drm_remove_user_object(priv, uo);
			mutex_unlock(&dev->struct_mutex);
			break;
		case drm_bo_reference:
			rep.ret = drm_user_object_ref(priv, req->handle,
						      drm_buffer_type, &uo);
			if (rep.ret)
				break;
			mutex_lock(&dev->struct_mutex);
			uo = drm_lookup_user_object(priv, req->handle);
			entry =
			    drm_user_object_entry(uo, drm_buffer_object_t,
						  base);
			atomic_dec(&entry->usage);
			mutex_unlock(&dev->struct_mutex);
			mutex_lock(&entry->mutex);
			drm_bo_fill_rep_arg(entry, &rep);
			mutex_unlock(&entry->mutex);
			break;
		case drm_bo_unreference:
			rep.ret = drm_user_object_unref(priv, req->handle,
							drm_buffer_type);
			break;
		case drm_bo_validate:
			rep.ret = drm_bo_lock_test(dev, filp);

			if (rep.ret)
				break;
			rep.ret =
			    drm_bo_handle_validate(priv, req->handle, req->mask,
						   req->arg_handle, req->hint,
						   &rep);
			break;
		case drm_bo_fence:
			rep.ret = drm_bo_lock_test(dev, filp);
			if (rep.ret)
				break;
			 /**/ break;
		case drm_bo_info:
			rep.ret = drm_bo_handle_info(priv, req->handle, &rep);
			break;
		case drm_bo_wait_idle:
			rep.ret = drm_bo_handle_wait(priv, req->handle,
						     req->hint, &rep);
			break;
		case drm_bo_ref_fence:
			rep.ret = -EINVAL;
			DRM_ERROR("Function is not implemented yet.\n");
		default:
			rep.ret = -EINVAL;
		}
		next = arg.next;

		/*
		 * A signal interrupted us. Make sure the ioctl is restartable.
		 */

		if (rep.ret == -EAGAIN)
			return -EAGAIN;

		arg.handled = 1;
		arg.d.rep = rep;
		DRM_COPY_TO_USER_IOCTL((void __user *)data, arg, sizeof(arg));
		data = next;
	} while (data);
	return 0;
}

/*
 * dev->struct_sem locked.
 */

static void drm_bo_force_list_clean(drm_device_t *dev,
				    struct list_head *head, int tt)
{
	struct list_head *l;
	drm_buffer_object_t *entry;
	int nice_mode = 1;
	int ret;

	l = head->next;
	while (l != head) {
		if (tt) {
			entry = list_entry(l, drm_buffer_object_t,
					   tt_lru);
		} else {
			entry = list_entry(l, drm_buffer_object_t,
					   vram_lru);
		}
			
		atomic_inc(&entry->usage);
		mutex_unlock(&dev->struct_mutex);
		mutex_lock(&entry->mutex);

		/*
		 * Expire the fence.
		 */

		if (entry->fence) {
			if (nice_mode) {
				unsigned long _end = jiffies + 3*DRM_HZ;
				do {
					ret = drm_bo_wait(entry, 0, 1, 0);
				} while (ret && !time_after_eq(jiffies, _end));

				if (entry->fence) {
					nice_mode = 0;
					DRM_ERROR("Detected GPU hang. "
						  "Evicting waiting buffers\n");
				}
			}
			if (entry->fence) {
				drm_fence_usage_deref_unlocked(dev, entry->fence);
				entry->fence = NULL;
			}
		}
		ret = drm_bo_evict(entry, tt, 0);
		if (ret) {
			DRM_ERROR("Aargh. Eviction failed.\n");
		}
		mutex_unlock(&entry->mutex);
		mutex_lock(&dev->struct_mutex);

		if (!list_empty(l)) {
			list_del_init(l);
			if (list_empty(&entry->tt_lru) && 
			    list_empty(&entry->vram_lru)) {
				list_add_tail(l, &dev->bm.other);
			}
		}
			       
		drm_bo_usage_deref_locked(dev, entry);
		l = head->next;
	}
}

int drm_bo_clean_mm(drm_device_t * dev)
{
	drm_buffer_manager_t *bm = &dev->bm;
	int ret = 0;

	mutex_lock(&dev->struct_mutex);

	if (!bm->initialized)
		goto out;

	bm->use_vram = 0;
	bm->use_tt = 0;

	/*
	 * FIXME: Need to handle unfenced list.
	 */

	drm_bo_force_list_clean(dev, &bm->tt_lru, 1);
	drm_bo_force_list_clean(dev, &bm->tt_pinned, 1);
	drm_bo_force_list_clean(dev, &bm->vram_lru, 1);
	drm_bo_force_list_clean(dev, &bm->vram_pinned, 1);

	if (bm->has_vram) {
		if (drm_mm_clean(&bm->vram_manager)) {
			drm_mm_takedown(&bm->vram_manager);
			bm->has_vram = 0;
		} else
			ret = -EBUSY;
	}

	if (bm->has_tt) {
		if (drm_mm_clean(&bm->tt_manager)) {
			drm_mm_takedown(&bm->tt_manager);
			bm->has_tt = 0;
		} else
			ret = -EBUSY;

		if (!ret)
			bm->initialized = 0;
	}

      out:
	mutex_unlock(&dev->struct_mutex);

	return ret;
}

int drm_mm_init_ioctl(DRM_IOCTL_ARGS)
{
	DRM_DEVICE;

	int ret = 0;
	drm_mm_init_arg_t arg;
	drm_buffer_manager_t *bm = &dev->bm;
	drm_bo_driver_t *driver = dev->driver->bo_driver;

	if (!driver) {
		DRM_ERROR("Buffer objects are not supported by this driver\n");
		return -EINVAL;
	}

	DRM_COPY_FROM_USER_IOCTL(arg, (void __user *)data, sizeof(arg));

	switch (arg.req.op) {
	case mm_init:
		if (bm->initialized) {
			DRM_ERROR("Memory manager already initialized\n");
			return -EINVAL;
		}
		mutex_lock(&dev->struct_mutex);
		bm->has_vram = 0;
		bm->has_tt = 0;

		if (arg.req.vr_p_size) {
			ret = drm_mm_init(&bm->vram_manager,
					  arg.req.vr_p_offset,
					  arg.req.vr_p_size);
			bm->has_vram = 1;
			/*
			 * VRAM not supported yet.
			 */

			bm->use_vram = 0;
			if (ret)
				break;
		}

		if (arg.req.tt_p_size) {
			ret = drm_mm_init(&bm->tt_manager,
					  arg.req.tt_p_offset,
					  3000 /* arg.req.tt_p_size */);
			bm->has_tt = 1;
			bm->use_tt = 1;

			if (ret) {
				if (bm->has_vram)
					drm_mm_takedown(&bm->vram_manager);
				break;
			}
		}
		arg.rep.mm_sarea = 0;

		INIT_LIST_HEAD(&bm->vram_lru);
		INIT_LIST_HEAD(&bm->tt_lru);
		INIT_LIST_HEAD(&bm->vram_pinned);
		INIT_LIST_HEAD(&bm->tt_pinned);
		INIT_LIST_HEAD(&bm->unfenced);
		INIT_LIST_HEAD(&bm->ddestroy);
		INIT_LIST_HEAD(&bm->other);

		INIT_WORK(&bm->wq, &drm_bo_delayed_workqueue, dev);
		bm->initialized = 1;
		atomic_set(&bm->count, 0);
		bm->cur_pages = 0;
		bm->max_pages = arg.req.max_locked_pages;
		break;
	case mm_takedown:
	        LOCK_TEST_WITH_RETURN(dev, filp);
		if (drm_bo_clean_mm(dev)) {
			DRM_ERROR("Memory manager not clean. "
				  "Delaying takedown\n");
		}
		DRM_DEBUG("We have %ld still locked pages\n", bm->cur_pages);
		break;
	default:
		DRM_ERROR("Function not implemented yet\n");
		return -EINVAL;
	}

	mutex_unlock(&dev->struct_mutex);
	if (ret)
		return ret;

	DRM_COPY_TO_USER_IOCTL((void __user *)data, arg, sizeof(arg));
	return 0;
}
