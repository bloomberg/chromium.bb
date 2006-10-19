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

static inline uint32_t drm_bo_type_flags(unsigned type)
{
	return (1 << (24 + type));
}

static inline drm_buffer_object_t *drm_bo_entry(struct list_head *list,
						unsigned type)
{
	switch (type) {
	case DRM_BO_MEM_LOCAL:
	case DRM_BO_MEM_TT:
		return list_entry(list, drm_buffer_object_t, lru_ttm);
	case DRM_BO_MEM_VRAM:
	case DRM_BO_MEM_VRAM_NM:
		return list_entry(list, drm_buffer_object_t, lru_card);
	default:
		BUG_ON(1);
	}
	return NULL;
}

static inline drm_mm_node_t *drm_bo_mm_node(drm_buffer_object_t * bo,
					    unsigned type)
{
	switch (type) {
	case DRM_BO_MEM_LOCAL:
	case DRM_BO_MEM_TT:
		return bo->node_ttm;
	case DRM_BO_MEM_VRAM:
	case DRM_BO_MEM_VRAM_NM:
		return bo->node_card;
	default:
		BUG_ON(1);
	}
	return NULL;
}

/*
 * bo locked. dev->struct_mutex locked.
 */

static void drm_bo_add_to_lru(drm_buffer_object_t * buf,
			      drm_buffer_manager_t * bm)
{
	struct list_head *list;
	unsigned mem_type;

	if (buf->flags & DRM_BO_FLAG_MEM_TT) {
		mem_type = DRM_BO_MEM_TT;
		list =
		    (buf->
		     flags & (DRM_BO_FLAG_NO_EVICT | DRM_BO_FLAG_NO_MOVE)) ?
		    &bm->pinned[mem_type] : &bm->lru[mem_type];
		list_add_tail(&buf->lru_ttm, list);
	} else {
		mem_type = DRM_BO_MEM_LOCAL;
		list =
		    (buf->
		     flags & (DRM_BO_FLAG_NO_EVICT | DRM_BO_FLAG_NO_MOVE)) ?
		    &bm->pinned[mem_type] : &bm->lru[mem_type];
		list_add_tail(&buf->lru_ttm, list);
	}
	if (buf->flags & DRM_BO_FLAG_MEM_VRAM) {
		mem_type = DRM_BO_MEM_VRAM;
		list =
		    (buf->
		     flags & (DRM_BO_FLAG_NO_EVICT | DRM_BO_FLAG_NO_MOVE)) ?
		    &bm->pinned[mem_type] : &bm->lru[mem_type];
		list_add_tail(&buf->lru_card, list);
	}
}

/*
 * bo locked.
 */

static int drm_move_tt_to_local(drm_buffer_object_t * buf, int evict,
				int force_no_move)
{
	drm_device_t *dev = buf->dev;
	int ret;

	if (buf->node_ttm) {
		mutex_lock(&dev->struct_mutex);
		if (evict)
			ret = drm_evict_ttm(buf->ttm);
		else
			ret = drm_unbind_ttm(buf->ttm);

		if (ret) {
			mutex_unlock(&dev->struct_mutex);
			if (ret == -EAGAIN)
				schedule();
			return ret;
		}

		if (!(buf->flags & DRM_BO_FLAG_NO_MOVE) || force_no_move) {
			drm_mm_put_block(buf->node_ttm);
			buf->node_ttm = NULL;
		}
		mutex_unlock(&dev->struct_mutex);
	}

	buf->flags &= ~DRM_BO_FLAG_MEM_TT;
	buf->flags |= DRM_BO_FLAG_MEM_LOCAL | DRM_BO_FLAG_CACHED;

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

	list_del_init(&bo->lru_ttm);
	list_del_init(&bo->lru_card);

	if (bo->ttm) {
		unsigned long _end = jiffies + DRM_HZ;
		int ret;

		/*
		 * This temporarily unlocks struct_mutex. 
		 */

		do {
			ret = drm_unbind_ttm(bo->ttm);
			if (ret == -EAGAIN) {
				mutex_unlock(&dev->struct_mutex);
				schedule();
				mutex_lock(&dev->struct_mutex);
			}
		} while (ret == -EAGAIN && !time_after_eq(jiffies, _end));

		if (ret) {
			DRM_ERROR("Couldn't unbind buffer. "
				  "Bad. Continuing anyway\n");
		}
	}

	if (bo->node_ttm) {
		drm_mm_put_block(bo->node_ttm);
		bo->node_ttm = NULL;
	}
	if (bo->node_card) {
		drm_mm_put_block(bo->node_card);
		bo->node_card = NULL;
	}
	if (bo->ttm_object) {
		drm_ttm_object_deref_locked(dev, bo->ttm_object);
	}
	atomic_dec(&bm->count);
	drm_ctl_free(bo, sizeof(*bo), DRM_MEM_BUFOBJ);
}

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
 * Call dev->struct_mutex locked.
 */

static void drm_bo_delayed_delete(drm_device_t * dev, int remove_all)
{
	drm_buffer_manager_t *bm = &dev->bm;

	drm_buffer_object_t *entry, *nentry;
	struct list_head *list, *next;
	drm_fence_object_t *fence;

	list_for_each_safe(list, next, &bm->ddestroy) {
		entry = list_entry(list, drm_buffer_object_t, ddestroy);
		atomic_inc(&entry->usage);
		if (atomic_read(&entry->usage) != 1) {
			atomic_dec(&entry->usage);
			continue;
		}

		nentry = NULL;
		if (next != &bm->ddestroy) {
			nentry = list_entry(next, drm_buffer_object_t,
					    ddestroy);
			atomic_inc(&nentry->usage);
		}

		mutex_unlock(&dev->struct_mutex);
		mutex_lock(&entry->mutex);
		fence = entry->fence;
		if (fence && drm_fence_object_signaled(fence,
						       entry->fence_type)) {
			drm_fence_usage_deref_locked(dev, fence);
			entry->fence = NULL;
		}

		if (entry->fence && remove_all) {
			if (bm->nice_mode) {
				unsigned long _end = jiffies + 3 * DRM_HZ;
				int ret;
				do {
					ret = drm_bo_wait(entry, 0, 1, 0);
				} while (ret && !time_after_eq(jiffies, _end));

				if (entry->fence) {
					bm->nice_mode = 0;
					DRM_ERROR("Detected GPU lockup or "
						  "fence driver was taken down. "
						  "Evicting waiting buffers.\n");
				}
			}
			if (entry->fence) {
				drm_fence_usage_deref_unlocked(dev,
							       entry->fence);
				entry->fence = NULL;
			}
		}
		mutex_lock(&dev->struct_mutex);
		mutex_unlock(&entry->mutex);
		if (atomic_dec_and_test(&entry->usage) && (!entry->fence)) {
			list_del_init(&entry->ddestroy);
			drm_bo_destroy_locked(dev, entry);
		}
		if (nentry) {
			atomic_dec(&nentry->usage);
		}
	}

}

static void drm_bo_delayed_workqueue(void *data)
{
	drm_device_t *dev = (drm_device_t *) data;
	drm_buffer_manager_t *bm = &dev->bm;

	DRM_DEBUG("Delayed delete Worker\n");

	mutex_lock(&dev->struct_mutex);
	if (!bm->initialized) {
		mutex_unlock(&dev->struct_mutex);
		return;
	}
	drm_bo_delayed_delete(dev, 0);
	if (bm->initialized && !list_empty(&bm->ddestroy)) {
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

	mutex_lock(&dev->struct_mutex);

	if (!list)
		list = &bm->unfenced;

	list_for_each_entry(entry, list, lru_ttm) {
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
		entry = list_entry(l, drm_buffer_object_t, lru_ttm);
		atomic_inc(&entry->usage);
		mutex_unlock(&dev->struct_mutex);
		mutex_lock(&entry->mutex);
		mutex_lock(&dev->struct_mutex);
		list_del_init(l);
		list_del_init(&entry->lru_card);
		if (entry->priv_flags & _DRM_BO_FLAG_UNFENCED) {
			count++;
			if (entry->fence)
				drm_fence_usage_deref_locked(dev, entry->fence);
			entry->fence = fence;
			DRM_FLAG_MASKED(entry->priv_flags, 0,
					_DRM_BO_FLAG_UNFENCED);
			DRM_WAKEUP(&entry->event_queue);
			drm_bo_add_to_lru(entry, bm);
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
 * bo->mutex locked 
 */

static int drm_bo_evict(drm_buffer_object_t * bo, unsigned mem_type,
			int no_wait, int force_no_move)
{
	int ret = 0;
	drm_device_t *dev = bo->dev;
	drm_buffer_manager_t *bm = &dev->bm;

	/*
	 * Someone might have modified the buffer before we took the buffer mutex.
	 */

	if (bo->priv_flags & _DRM_BO_FLAG_UNFENCED)
		goto out;
	if (!(bo->flags & drm_bo_type_flags(mem_type)))
		goto out;

	ret = drm_bo_wait(bo, 0, 0, no_wait);

	if (ret) {
		if (ret != -EAGAIN)
			DRM_ERROR("Failed to expire fence before "
				  "buffer eviction.\n");
		goto out;
	}

	if (mem_type == DRM_BO_MEM_TT) {
		ret = drm_move_tt_to_local(bo, 1, force_no_move);
		if (ret)
			goto out;
		mutex_lock(&dev->struct_mutex);
		list_del_init(&bo->lru_ttm);
		drm_bo_add_to_lru(bo, bm);
		mutex_unlock(&dev->struct_mutex);
	}
#if 0
	else {
		ret = drm_move_vram_to_local(bo);
		mutex_lock(&dev->struct_mutex);
		list_del_init(&bo->lru_card);
		mutex_unlock(&dev->struct_mutex);
	}
#endif
	if (ret)
		goto out;

	DRM_FLAG_MASKED(bo->priv_flags, _DRM_BO_FLAG_EVICTED,
			_DRM_BO_FLAG_EVICTED);
      out:
	return ret;
}

/*
 * buf->mutex locked.
 */

int drm_bo_alloc_space(drm_buffer_object_t * buf, unsigned mem_type,
		       int no_wait)
{
	drm_device_t *dev = buf->dev;
	drm_mm_node_t *node;
	drm_buffer_manager_t *bm = &dev->bm;
	drm_buffer_object_t *bo;
	drm_mm_t *mm = &bm->manager[mem_type];
	struct list_head *lru;
	unsigned long size = buf->num_pages;
	int ret;

	mutex_lock(&dev->struct_mutex);
	do {
		node = drm_mm_search_free(mm, size, 0, 1);
		if (node)
			break;

		lru = &bm->lru[mem_type];
		if (lru->next == lru)
			break;

		bo = drm_bo_entry(lru->next, mem_type);

		atomic_inc(&bo->usage);
		mutex_unlock(&dev->struct_mutex);
		mutex_lock(&bo->mutex);
		BUG_ON(bo->flags & DRM_BO_FLAG_NO_MOVE);
		ret = drm_bo_evict(bo, mem_type, no_wait, 0);
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

	if (mem_type == DRM_BO_MEM_TT) {
		buf->node_ttm = node;
	} else {
		buf->node_card = node;
	}
	buf->offset = node->start * PAGE_SIZE;
	return 0;
}

static int drm_move_local_to_tt(drm_buffer_object_t * bo, int no_wait)
{
	drm_device_t *dev = bo->dev;
	drm_ttm_backend_t *be;
	int ret;

	if (!(bo->node_ttm && (bo->flags & DRM_BO_FLAG_NO_MOVE))) {
		BUG_ON(bo->node_ttm);
		ret = drm_bo_alloc_space(bo, DRM_BO_MEM_TT, no_wait);
		if (ret)
			return ret;
	}

	DRM_DEBUG("Flipping in to AGP 0x%08lx\n", bo->node_ttm->start);

	mutex_lock(&dev->struct_mutex);
	ret = drm_bind_ttm(bo->ttm, bo->flags & DRM_BO_FLAG_BIND_CACHED,
			   bo->node_ttm->start);
	if (ret) {
		drm_mm_put_block(bo->node_ttm);
		bo->node_ttm = NULL;
	}
	mutex_unlock(&dev->struct_mutex);

	if (ret) {
		return ret;
	}

	be = bo->ttm->be;
	if (be->needs_ub_cache_adjust(be))
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
	unsigned i;

	/*
	 * First adjust the mask to take away nonexistant memory types. 
	 */

	for (i = 0; i < DRM_BO_MEM_TYPES; ++i) {
		if (!bm->use_type[i])
			new_mask &= ~drm_bo_type_flags(i);
	}

	if ((new_mask & DRM_BO_FLAG_NO_EVICT) && !DRM_SUSER(DRM_CURPROC)) {
		DRM_ERROR
		    ("DRM_BO_FLAG_NO_EVICT is only available to priviliged "
		     "processes\n");
		return -EPERM;
	}
	if (new_mask & DRM_BO_FLAG_BIND_CACHED) {
		if (((new_mask & DRM_BO_FLAG_MEM_TT) &&
		     !driver->cached[DRM_BO_MEM_TT]) &&
		    ((new_mask & DRM_BO_FLAG_MEM_VRAM)
		     && !driver->cached[DRM_BO_MEM_VRAM])) {
			new_mask &= ~DRM_BO_FLAG_BIND_CACHED;
		} else {
			if (!driver->cached[DRM_BO_MEM_TT])
				new_flags &= DRM_BO_FLAG_MEM_TT;
			if (!driver->cached[DRM_BO_MEM_VRAM])
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
	if (bo->node_card)
		ret = drm_bo_evict(bo, DRM_BO_MEM_VRAM, 1, 0);
	if (ret)
		return ret;
	if (bo->node_ttm)
		ret = drm_bo_evict(bo, DRM_BO_MEM_TT, 1, 0);
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
			      int no_wait, int force_no_move)
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
		drm_move_tt_to_local(bo, 0, force_no_move);
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
	 * Move out if we need to change caching policy.
	 */

	if ((flag_diff & DRM_BO_FLAG_BIND_CACHED) &&
	    !(bo->flags & DRM_BO_FLAG_MEM_LOCAL)) {
		if (bo->flags & (DRM_BO_FLAG_NO_EVICT | DRM_BO_FLAG_NO_MOVE)) {
			DRM_ERROR("Cannot change caching policy of "
				  "pinned buffer.\n");
			return -EINVAL;
		}
		ret = drm_bo_move_buffer(bo, DRM_BO_FLAG_MEM_LOCAL, no_wait, 0);
		if (ret) {
			if (ret != -EAGAIN)
				DRM_ERROR("Failed moving buffer.\n");
			return ret;
		}
	}
	DRM_MASK_VAL(bo->flags, DRM_BO_FLAG_BIND_CACHED, new_flags);
	flag_diff = (new_flags ^ bo->flags);

	/*
	 * Check whether we dropped no_move policy, and in that case,
	 * release reserved manager regions.
	 */

	if ((flag_diff & DRM_BO_FLAG_NO_MOVE) &&
	    !(new_flags & DRM_BO_FLAG_NO_MOVE)) {
		mutex_lock(&dev->struct_mutex);
		if (bo->node_ttm) {
			drm_mm_put_block(bo->node_ttm);
			bo->node_ttm = NULL;
		}
		if (bo->node_card) {
			drm_mm_put_block(bo->node_card);
			bo->node_card = NULL;
		}
		mutex_unlock(&dev->struct_mutex);
	}

	/*
	 * Check whether we need to move buffer.
	 */

	if ((bo->type != drm_bo_type_fake) && (flag_diff & DRM_BO_MASK_MEM)) {
		ret = drm_bo_move_buffer(bo, new_flags, no_wait, 1);
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
		list_del(&bo->lru_ttm);
		list_add_tail(&bo->lru_ttm, &bm->unfenced);
		list_del_init(&bo->lru_card);
		mutex_unlock(&dev->struct_mutex);
	} else {

		mutex_lock(&dev->struct_mutex);
		list_del_init(&bo->lru_ttm);
		list_del_init(&bo->lru_card);
		drm_bo_add_to_lru(bo, bm);
		mutex_unlock(&dev->struct_mutex);
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

	if ((buffer_start & ~PAGE_MASK) && (type != drm_bo_type_fake)) {
		DRM_ERROR("Invalid buffer object start.\n");
		return -EINVAL;
	}
	num_pages = (size + PAGE_SIZE - 1) >> PAGE_SHIFT;
	if (num_pages == 0) {
		DRM_ERROR("Illegal buffer object size.\n");
		return -EINVAL;
	}

	bo = drm_ctl_calloc(1, sizeof(*bo), DRM_MEM_BUFOBJ);

	if (!bo)
		return -ENOMEM;

	mutex_init(&bo->mutex);
	mutex_lock(&bo->mutex);

	atomic_set(&bo->usage, 1);
	atomic_set(&bo->mapped, -1);
	DRM_INIT_WAITQUEUE(&bo->event_queue);
	INIT_LIST_HEAD(&bo->lru_ttm);
	INIT_LIST_HEAD(&bo->lru_card);
	INIT_LIST_HEAD(&bo->ddestroy);
	bo->dev = dev;
	bo->type = type;
	bo->num_pages = num_pages;
	bo->node_card = NULL;
	bo->node_ttm = NULL;
	if (bo->type == drm_bo_type_fake) {
		bo->offset = buffer_start;
		bo->buffer_start = 0;
	} else {
		bo->buffer_start = buffer_start;
	}
	bo->priv_flags = 0;
	bo->flags = DRM_BO_FLAG_MEM_LOCAL | DRM_BO_FLAG_CACHED;
	atomic_inc(&bm->count);
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
	return 0;

      out_err:
	mutex_unlock(&bo->mutex);
	drm_bo_usage_deref_unlocked(dev, bo);
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

static int drm_bo_force_list_clean(drm_device_t * dev,
				   struct list_head *head,
				   unsigned mem_type,
				   int force_no_move, int allow_errors)
{
	drm_buffer_manager_t *bm = &dev->bm;
	struct list_head *list, *next, *prev;
	drm_buffer_object_t *entry;
	int ret;
	int clean;

      retry:
	clean = 1;
	list_for_each_safe(list, next, head) {
		prev = list->prev;
		entry = drm_bo_entry(list, mem_type);
		atomic_inc(&entry->usage);
		mutex_unlock(&dev->struct_mutex);
		mutex_lock(&entry->mutex);
		mutex_lock(&dev->struct_mutex);

		if (prev != list->prev || next != list->next) {
			mutex_unlock(&entry->mutex);
			drm_bo_usage_deref_locked(dev, entry);
			goto retry;
		}
		if (drm_bo_mm_node(entry, mem_type)) {
			clean = 0;

			/*
			 * Expire the fence.
			 */

			mutex_unlock(&dev->struct_mutex);
			if (entry->fence && bm->nice_mode) {
				unsigned long _end = jiffies + 3 * DRM_HZ;
				do {
					ret = drm_bo_wait(entry, 0, 1, 0);
					if (ret && allow_errors) {
						if (ret == -EINTR)
							ret = -EAGAIN;
						goto out_err;
					}
				} while (ret && !time_after_eq(jiffies, _end));

				if (entry->fence) {
					bm->nice_mode = 0;
					DRM_ERROR("Detected GPU hang or "
						  "fence manager was taken down. "
						  "Evicting waiting buffers\n");
				}
			}
			if (entry->fence) {
				drm_fence_usage_deref_unlocked(dev,
							       entry->fence);
				entry->fence = NULL;
			}

			DRM_MASK_VAL(entry->priv_flags, _DRM_BO_FLAG_UNFENCED,
				     0);

			if (force_no_move) {
				DRM_MASK_VAL(entry->flags, DRM_BO_FLAG_NO_MOVE,
					     0);
			}
			if (entry->flags & DRM_BO_FLAG_NO_EVICT) {
				DRM_ERROR("A DRM_BO_NO_EVICT buffer present at "
					  "cleanup. Removing flag and evicting.\n");
				entry->flags &= ~DRM_BO_FLAG_NO_EVICT;
				entry->mask &= ~DRM_BO_FLAG_NO_EVICT;
			}

			ret = drm_bo_evict(entry, mem_type, 1, force_no_move);
			if (ret) {
				if (allow_errors) {
					goto out_err;
				} else {
					DRM_ERROR("Aargh. Eviction failed.\n");
				}
			}
			mutex_lock(&dev->struct_mutex);
		}
		mutex_unlock(&entry->mutex);
		drm_bo_usage_deref_locked(dev, entry);
		if (prev != list->prev || next != list->next) {
			goto retry;
		}
	}
	if (!clean)
		goto retry;
	return 0;
      out_err:
	mutex_unlock(&entry->mutex);
	drm_bo_usage_deref_unlocked(dev, entry);
	mutex_lock(&dev->struct_mutex);
	return ret;
}

int drm_bo_clean_mm(drm_device_t * dev, unsigned mem_type)
{
	drm_buffer_manager_t *bm = &dev->bm;
	int ret = -EINVAL;

	if (mem_type >= DRM_BO_MEM_TYPES) {
		DRM_ERROR("Illegal memory type %d\n", mem_type);
		return ret;
	}

	if (!bm->has_type[mem_type]) {
		DRM_ERROR("Trying to take down uninitialized "
			  "memory manager type\n");
		return ret;
	}
	bm->use_type[mem_type] = 0;
	bm->has_type[mem_type] = 0;

	ret = 0;
	if (mem_type > 0) {

		/*
		 * Throw out unfenced buffers.
		 */

		drm_bo_force_list_clean(dev, &bm->unfenced, mem_type, 1, 0);

		/*
		 * Throw out evicted no-move buffers.
		 */

		drm_bo_force_list_clean(dev, &bm->pinned[DRM_BO_MEM_LOCAL],
					mem_type, 1, 0);
		drm_bo_force_list_clean(dev, &bm->lru[mem_type], mem_type, 1,
					0);
		drm_bo_force_list_clean(dev, &bm->pinned[mem_type], mem_type, 1,
					0);

		if (drm_mm_clean(&bm->manager[mem_type])) {
			drm_mm_takedown(&bm->manager[mem_type]);
		} else {
			ret = -EBUSY;
		}
	}

	return ret;
}

static int drm_bo_lock_mm(drm_device_t * dev, unsigned mem_type)
{
	int ret;
	drm_buffer_manager_t *bm = &dev->bm;

	if (mem_type == 0 || mem_type >= DRM_BO_MEM_TYPES) {
		DRM_ERROR("Illegal memory manager memory type %u,\n", mem_type);
		return -EINVAL;
	}

	ret = drm_bo_force_list_clean(dev, &bm->unfenced, mem_type, 0, 1);
	if (ret)
		return ret;
	ret = drm_bo_force_list_clean(dev, &bm->lru[mem_type], mem_type, 0, 1);
	if (ret)
		return ret;
	ret =
	    drm_bo_force_list_clean(dev, &bm->pinned[mem_type], mem_type, 0, 1);
	return ret;
}

static int drm_bo_init_mm(drm_device_t * dev,
			  unsigned type,
			  unsigned long p_offset, unsigned long p_size)
{
	drm_buffer_manager_t *bm = &dev->bm;
	int ret = -EINVAL;

	if (type >= DRM_BO_MEM_TYPES) {
		DRM_ERROR("Illegal memory type %d\n", type);
		return ret;
	}
	if (bm->has_type[type]) {
		DRM_ERROR("Memory manager already initialized for type %d\n",
			  type);
		return ret;
	}

	ret = 0;
	if (type != DRM_BO_MEM_LOCAL) {
		if (!p_size) {
			DRM_ERROR("Zero size memory manager type %d\n", type);
			return ret;
		}
		ret = drm_mm_init(&bm->manager[type], p_offset, p_size);
		if (ret)
			return ret;
	}
	bm->has_type[type] = 1;
	bm->use_type[type] = 1;

	INIT_LIST_HEAD(&bm->lru[type]);
	INIT_LIST_HEAD(&bm->pinned[type]);

	return 0;
}

/*
 * This is called from lastclose, so we don't need to bother about
 * any clients still running when we set the initialized flag to zero.
 */

int drm_bo_driver_finish(drm_device_t * dev)
{
	drm_buffer_manager_t *bm = &dev->bm;
	int ret = 0;
	unsigned i = DRM_BO_MEM_TYPES;

	mutex_lock(&dev->bm.init_mutex);
	mutex_lock(&dev->struct_mutex);

	if (!bm->initialized)
		goto out;
	bm->initialized = 0;

	while (i--) {
		if (bm->has_type[i]) {
			bm->use_type[i] = 0;
			if ((i != DRM_BO_MEM_LOCAL) && drm_bo_clean_mm(dev, i)) {
				ret = -EBUSY;
				DRM_ERROR("DRM memory manager type %d "
					  "is not clean.\n", i);
			}
			bm->has_type[i] = 0;
		}
	}
	mutex_unlock(&dev->struct_mutex);
	if (!cancel_delayed_work(&bm->wq)) {
		flush_scheduled_work();
	}
	mutex_lock(&dev->struct_mutex);
	drm_bo_delayed_delete(dev, 1);
	if (list_empty(&bm->ddestroy)) {
		DRM_DEBUG("Delayed destroy list was clean\n");
	}
	if (list_empty(&bm->lru[0])) {
		DRM_DEBUG("Swap list was clean\n");
	}
	if (list_empty(&bm->pinned[0])) {
		DRM_DEBUG("NO_MOVE list was clean\n");
	}
	if (list_empty(&bm->unfenced)) {
		DRM_DEBUG("Unfenced list was clean\n");
	}
      out:
	mutex_unlock(&dev->struct_mutex);
	mutex_unlock(&dev->bm.init_mutex);
	return ret;
}

int drm_bo_driver_init(drm_device_t * dev)
{
	drm_bo_driver_t *driver = dev->driver->bo_driver;
	drm_buffer_manager_t *bm = &dev->bm;
	int ret = -EINVAL;

	mutex_lock(&dev->bm.init_mutex);
	mutex_lock(&dev->struct_mutex);
	if (!driver)
		goto out_unlock;

	/*
	 * Initialize the system memory buffer type.
	 * Other types need to be driver / IOCTL initialized.
	 */

	ret = drm_bo_init_mm(dev, 0, 0, 0);
	if (ret)
		goto out_unlock;

	INIT_WORK(&bm->wq, &drm_bo_delayed_workqueue, dev);
	bm->initialized = 1;
	bm->nice_mode = 1;
	atomic_set(&bm->count, 0);
	bm->cur_pages = 0;
	INIT_LIST_HEAD(&bm->unfenced);
	INIT_LIST_HEAD(&bm->ddestroy);
      out_unlock:
	mutex_unlock(&dev->struct_mutex);
	mutex_unlock(&dev->bm.init_mutex);
	return ret;
}

EXPORT_SYMBOL(drm_bo_driver_init);

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
		ret = -EINVAL;
		mutex_lock(&dev->bm.init_mutex);
		mutex_lock(&dev->struct_mutex);
		if (!bm->initialized) {
			DRM_ERROR("DRM memory manager was not initialized.\n");
			break;
		}
		if (arg.req.mem_type == 0) {
			DRM_ERROR
			    ("System memory buffers already initialized.\n");
			break;
		}
		ret = drm_bo_init_mm(dev, arg.req.mem_type,
				     arg.req.p_offset, arg.req.p_size);
		break;
	case mm_takedown:
		LOCK_TEST_WITH_RETURN(dev, filp);
		mutex_lock(&dev->bm.init_mutex);
		mutex_lock(&dev->struct_mutex);
		ret = -EINVAL;
		if (!bm->initialized) {
			DRM_ERROR("DRM memory manager was not initialized\n");
			break;
		}
		if (arg.req.mem_type == 0) {
			DRM_ERROR("No takedown for System memory buffers.\n");
			break;
		}
		ret = 0;
		if (drm_bo_clean_mm(dev, arg.req.mem_type)) {
			DRM_ERROR("Memory manager type %d not clean. "
				  "Delaying takedown\n", arg.req.mem_type);
		}
		break;
	case mm_lock:
		LOCK_TEST_WITH_RETURN(dev, filp);
		mutex_lock(&dev->bm.init_mutex);
		mutex_lock(&dev->struct_mutex);
		ret = drm_bo_lock_mm(dev, arg.req.mem_type);
		break;
	case mm_unlock:
		LOCK_TEST_WITH_RETURN(dev, filp);
		mutex_lock(&dev->bm.init_mutex);
		mutex_lock(&dev->struct_mutex);
		ret = 0;
		break;
	default:
		DRM_ERROR("Function not implemented yet\n");
		return -EINVAL;
	}

	mutex_unlock(&dev->struct_mutex);
	mutex_unlock(&dev->bm.init_mutex);
	if (ret)
		return ret;

	DRM_COPY_TO_USER_IOCTL((void __user *)data, arg, sizeof(arg));
	return 0;
}
