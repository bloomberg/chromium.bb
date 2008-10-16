/**************************************************************************
 *
 * Copyright (c) 2006-2007 Tungsten Graphics, Inc., Cedar Park, TX., USA
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

/*
 * Locking may look a bit complicated but isn't really:
 *
 * The buffer usage atomic_t needs to be protected by dev->struct_mutex
 * when there is a chance that it can be zero before or after the operation.
 *
 * dev->struct_mutex also protects all lists and list heads,
 * Hash tables and hash heads.
 *
 * bo->mutex protects the buffer object itself excluding the usage field.
 * bo->mutex does also protect the buffer list heads, so to manipulate those,
 * we need both the bo->mutex and the dev->struct_mutex.
 *
 * Locking order is bo->mutex, dev->struct_mutex. Therefore list traversal
 * is a bit complicated. When dev->struct_mutex is released to grab bo->mutex,
 * the list traversal will, in general, need to be restarted.
 *
 */

static void drm_bo_destroy_locked(struct drm_buffer_object *bo);
static int drm_bo_setup_vm_locked(struct drm_buffer_object *bo);
static void drm_bo_unmap_virtual(struct drm_buffer_object *bo);

static inline uint64_t drm_bo_type_flags(unsigned type)
{
	return (1ULL << (24 + type));
}

/*
 * bo locked. dev->struct_mutex locked.
 */

void drm_bo_add_to_pinned_lru(struct drm_buffer_object *bo)
{
	struct drm_mem_type_manager *man;

	DRM_ASSERT_LOCKED(&bo->dev->struct_mutex);
	DRM_ASSERT_LOCKED(&bo->mutex);

	man = &bo->dev->bm.man[bo->pinned_mem_type];
	list_add_tail(&bo->pinned_lru, &man->pinned);
}

void drm_bo_add_to_lru(struct drm_buffer_object *bo)
{
	struct drm_mem_type_manager *man;

	DRM_ASSERT_LOCKED(&bo->dev->struct_mutex);

	if (!(bo->mem.proposed_flags & (DRM_BO_FLAG_NO_MOVE | DRM_BO_FLAG_NO_EVICT))
	    || bo->mem.mem_type != bo->pinned_mem_type) {
		man = &bo->dev->bm.man[bo->mem.mem_type];
		list_add_tail(&bo->lru, &man->lru);
	} else {
		INIT_LIST_HEAD(&bo->lru);
	}
}

static int drm_bo_vm_pre_move(struct drm_buffer_object *bo, int old_is_pci)
{
#ifdef DRM_ODD_MM_COMPAT
	int ret;

	if (!bo->map_list.map)
		return 0;

	ret = drm_bo_lock_kmm(bo);
	if (ret)
		return ret;
	drm_bo_unmap_virtual(bo);
	if (old_is_pci)
		drm_bo_finish_unmap(bo);
#else
	if (!bo->map_list.map)
		return 0;

	drm_bo_unmap_virtual(bo);
#endif
	return 0;
}

static void drm_bo_vm_post_move(struct drm_buffer_object *bo)
{
#ifdef DRM_ODD_MM_COMPAT
	int ret;

	if (!bo->map_list.map)
		return;

	ret = drm_bo_remap_bound(bo);
	if (ret) {
		DRM_ERROR("Failed to remap a bound buffer object.\n"
			  "\tThis might cause a sigbus later.\n");
	}
	drm_bo_unlock_kmm(bo);
#endif
}

/*
 * Call bo->mutex locked.
 */

int drm_bo_add_ttm(struct drm_buffer_object *bo)
{
	struct drm_device *dev = bo->dev;
	int ret = 0;
	uint32_t page_flags = 0;

	DRM_ASSERT_LOCKED(&bo->mutex);
	bo->ttm = NULL;

	if (bo->mem.proposed_flags & DRM_BO_FLAG_WRITE)
		page_flags |= DRM_TTM_PAGE_WRITE;

	switch (bo->type) {
	case drm_bo_type_device:
	case drm_bo_type_kernel:
		bo->ttm = drm_ttm_create(dev, bo->num_pages << PAGE_SHIFT, 
					 page_flags, dev->bm.dummy_read_page);
		if (!bo->ttm)
			ret = -ENOMEM;
		break;
	case drm_bo_type_user:
		bo->ttm = drm_ttm_create(dev, bo->num_pages << PAGE_SHIFT,
					 page_flags | DRM_TTM_PAGE_USER,
					 dev->bm.dummy_read_page);
		if (!bo->ttm)
			ret = -ENOMEM;

		ret = drm_ttm_set_user(bo->ttm, current,
				       bo->buffer_start,
				       bo->num_pages);
		if (ret)
			return ret;

		break;
	default:
		DRM_ERROR("Illegal buffer object type\n");
		ret = -EINVAL;
		break;
	}

	return ret;
}
EXPORT_SYMBOL(drm_bo_add_ttm);

static int drm_bo_handle_move_mem(struct drm_buffer_object *bo,
				  struct drm_bo_mem_reg *mem,
				  int evict, int no_wait)
{
	struct drm_device *dev = bo->dev;
	struct drm_buffer_manager *bm = &dev->bm;
	int old_is_pci = drm_mem_reg_is_pci(dev, &bo->mem);
	int new_is_pci = drm_mem_reg_is_pci(dev, mem);
	struct drm_mem_type_manager *old_man = &bm->man[bo->mem.mem_type];
	struct drm_mem_type_manager *new_man = &bm->man[mem->mem_type];
	int ret = 0;

	if (old_is_pci || new_is_pci ||
	    ((mem->flags ^ bo->mem.flags) & DRM_BO_FLAG_CACHED))
		ret = drm_bo_vm_pre_move(bo, old_is_pci);
	if (ret)
		return ret;

	/*
	 * Create and bind a ttm if required.
	 */

	if (!(new_man->flags & _DRM_FLAG_MEMTYPE_FIXED) && (bo->ttm == NULL)) {
		ret = drm_bo_add_ttm(bo);
		if (ret)
			goto out_err;

		if (mem->mem_type != DRM_BO_MEM_LOCAL) {
			ret = drm_ttm_bind(bo->ttm, mem);
			if (ret)
				goto out_err;
		}

		if (bo->mem.mem_type == DRM_BO_MEM_LOCAL) {
			
			struct drm_bo_mem_reg *old_mem = &bo->mem;
			uint64_t save_flags = old_mem->flags;
			uint64_t save_proposed_flags = old_mem->proposed_flags;
			
			*old_mem = *mem;
			mem->mm_node = NULL;
			old_mem->proposed_flags = save_proposed_flags;
			DRM_FLAG_MASKED(save_flags, mem->flags,
					DRM_BO_MASK_MEMTYPE);
			goto moved;
		}
		
	}

	if (!(old_man->flags & _DRM_FLAG_MEMTYPE_FIXED) &&
	    !(new_man->flags & _DRM_FLAG_MEMTYPE_FIXED))		
		ret = drm_bo_move_ttm(bo, evict, no_wait, mem);
	else if (dev->driver->bo_driver->move) 
		ret = dev->driver->bo_driver->move(bo, evict, no_wait, mem);
	else
		ret = drm_bo_move_memcpy(bo, evict, no_wait, mem);

	if (ret)
		goto out_err;

moved:
	if (old_is_pci || new_is_pci)
		drm_bo_vm_post_move(bo);

	if (bo->priv_flags & _DRM_BO_FLAG_EVICTED) {
		ret =
		    dev->driver->bo_driver->invalidate_caches(dev,
							      bo->mem.flags);
		if (ret)
			DRM_ERROR("Can not flush read caches\n");
	}

	DRM_FLAG_MASKED(bo->priv_flags,
			(evict) ? _DRM_BO_FLAG_EVICTED : 0,
			_DRM_BO_FLAG_EVICTED);

	if (bo->mem.mm_node)
		bo->offset = (bo->mem.mm_node->start << PAGE_SHIFT) +
			bm->man[bo->mem.mem_type].gpu_offset;


	return 0;

out_err:
	if (old_is_pci || new_is_pci)
		drm_bo_vm_post_move(bo);

	new_man = &bm->man[bo->mem.mem_type];
	if ((new_man->flags & _DRM_FLAG_MEMTYPE_FIXED) && bo->ttm) {
		drm_ttm_unbind(bo->ttm);
		drm_ttm_destroy(bo->ttm);
		bo->ttm = NULL;
	}

	return ret;
}

/*
 * Call bo->mutex locked.
 * Returns -EBUSY if the buffer is currently rendered to or from. 0 otherwise.
 */

static int drm_bo_busy(struct drm_buffer_object *bo, int check_unfenced)
{
	struct drm_fence_object *fence = bo->fence;

	if (check_unfenced && (bo->priv_flags & _DRM_BO_FLAG_UNFENCED))
		return -EBUSY;

	if (fence) {
		if (drm_fence_object_signaled(fence, bo->fence_type)) {
			drm_fence_usage_deref_unlocked(&bo->fence);
			return 0;
		}
		drm_fence_object_flush(fence, DRM_FENCE_TYPE_EXE);
		if (drm_fence_object_signaled(fence, bo->fence_type)) {
			drm_fence_usage_deref_unlocked(&bo->fence);
			return 0;
		}
		return -EBUSY;
	}
	return 0;
}

static int drm_bo_check_unfenced(struct drm_buffer_object *bo)
{
	int ret;

	mutex_lock(&bo->mutex);
	ret = (bo->priv_flags & _DRM_BO_FLAG_UNFENCED);
	mutex_unlock(&bo->mutex);
	return ret;
}


/*
 * Call bo->mutex locked.
 * Wait until the buffer is idle.
 */

int drm_bo_wait(struct drm_buffer_object *bo, int lazy, int interruptible,
		int no_wait, int check_unfenced)
{
	int ret;

	DRM_ASSERT_LOCKED(&bo->mutex);
	while(unlikely(drm_bo_busy(bo, check_unfenced))) {
		if (no_wait)
			return -EBUSY;

		if (check_unfenced &&  (bo->priv_flags & _DRM_BO_FLAG_UNFENCED)) {
			mutex_unlock(&bo->mutex);
			wait_event(bo->event_queue, !drm_bo_check_unfenced(bo));
			mutex_lock(&bo->mutex);
			bo->priv_flags |= _DRM_BO_FLAG_UNLOCKED;
		}

		if (bo->fence) {
			struct drm_fence_object *fence;
			uint32_t fence_type = bo->fence_type;

			drm_fence_reference_unlocked(&fence, bo->fence);
			mutex_unlock(&bo->mutex);

			ret = drm_fence_object_wait(fence, lazy, !interruptible,
						    fence_type);

			drm_fence_usage_deref_unlocked(&fence);
			mutex_lock(&bo->mutex);
			bo->priv_flags |= _DRM_BO_FLAG_UNLOCKED;
			if (ret)
				return ret;
		}

	}
	return 0;
}
EXPORT_SYMBOL(drm_bo_wait);

static int drm_bo_expire_fence(struct drm_buffer_object *bo, int allow_errors)
{
	struct drm_device *dev = bo->dev;
	struct drm_buffer_manager *bm = &dev->bm;

	if (bo->fence) {
		if (bm->nice_mode) {
			unsigned long _end = jiffies + 3 * DRM_HZ;
			int ret;
			do {
				ret = drm_bo_wait(bo, 0, 0, 0, 0);
				if (ret && allow_errors)
					return ret;

			} while (ret && !time_after_eq(jiffies, _end));

			if (bo->fence) {
				bm->nice_mode = 0;
				DRM_ERROR("Detected GPU lockup or "
					  "fence driver was taken down. "
					  "Evicting buffer.\n");
			}
		}
		if (bo->fence)
			drm_fence_usage_deref_unlocked(&bo->fence);
	}
	return 0;
}

/*
 * Call dev->struct_mutex locked.
 * Attempts to remove all private references to a buffer by expiring its
 * fence object and removing from lru lists and memory managers.
 */

static void drm_bo_cleanup_refs(struct drm_buffer_object *bo, int remove_all)
{
	struct drm_device *dev = bo->dev;
	struct drm_buffer_manager *bm = &dev->bm;

	DRM_ASSERT_LOCKED(&dev->struct_mutex);

	atomic_inc(&bo->usage);
	mutex_unlock(&dev->struct_mutex);
	mutex_lock(&bo->mutex);

	DRM_FLAG_MASKED(bo->priv_flags, 0, _DRM_BO_FLAG_UNFENCED);

	if (bo->fence && drm_fence_object_signaled(bo->fence,
						   bo->fence_type))
		drm_fence_usage_deref_unlocked(&bo->fence);

	if (bo->fence && remove_all)
		(void)drm_bo_expire_fence(bo, 0);

	mutex_lock(&dev->struct_mutex);

	if (!atomic_dec_and_test(&bo->usage))
		goto out;

	if (!bo->fence) {
		list_del_init(&bo->lru);
		if (bo->mem.mm_node) {
			drm_mm_put_block(bo->mem.mm_node);
			if (bo->pinned_node == bo->mem.mm_node)
				bo->pinned_node = NULL;
			bo->mem.mm_node = NULL;
		}
		list_del_init(&bo->pinned_lru);
		if (bo->pinned_node) {
			drm_mm_put_block(bo->pinned_node);
			bo->pinned_node = NULL;
		}
		list_del_init(&bo->ddestroy);
		mutex_unlock(&bo->mutex);
		drm_bo_destroy_locked(bo);
		return;
	}

	if (list_empty(&bo->ddestroy)) {
		drm_fence_object_flush(bo->fence, bo->fence_type);
		list_add_tail(&bo->ddestroy, &bm->ddestroy);
		schedule_delayed_work(&bm->wq,
				      ((DRM_HZ / 100) < 1) ? 1 : DRM_HZ / 100);
	}

out:
	mutex_unlock(&bo->mutex);
	return;
}

/*
 * Verify that refcount is 0 and that there are no internal references
 * to the buffer object. Then destroy it.
 */

static void drm_bo_destroy_locked(struct drm_buffer_object *bo)
{
	struct drm_device *dev = bo->dev;
	struct drm_buffer_manager *bm = &dev->bm;

	DRM_ASSERT_LOCKED(&dev->struct_mutex);

	DRM_DEBUG("freeing %p\n", bo);
	if (list_empty(&bo->lru) && bo->mem.mm_node == NULL &&
	    list_empty(&bo->pinned_lru) && bo->pinned_node == NULL &&
	    list_empty(&bo->ddestroy) && atomic_read(&bo->usage) == 0) {
		if (bo->fence != NULL) {
			DRM_ERROR("Fence was non-zero.\n");
			drm_bo_cleanup_refs(bo, 0);
			return;
		}

#ifdef DRM_ODD_MM_COMPAT
		BUG_ON(!list_empty(&bo->vma_list));
		BUG_ON(!list_empty(&bo->p_mm_list));
#endif

		if (bo->ttm) {
			drm_ttm_unbind(bo->ttm);
			drm_ttm_destroy(bo->ttm);
			bo->ttm = NULL;
		}

		atomic_dec(&bm->count);

		drm_ctl_free(bo, sizeof(*bo), DRM_MEM_BUFOBJ);

		return;
	}

	/*
	 * Some stuff is still trying to reference the buffer object.
	 * Get rid of those references.
	 */

	drm_bo_cleanup_refs(bo, 0);

	return;
}

/*
 * Call dev->struct_mutex locked.
 */

static void drm_bo_delayed_delete(struct drm_device *dev, int remove_all)
{
	struct drm_buffer_manager *bm = &dev->bm;

	struct drm_buffer_object *entry, *nentry;
	struct list_head *list, *next;

	list_for_each_safe(list, next, &bm->ddestroy) {
		entry = list_entry(list, struct drm_buffer_object, ddestroy);

		nentry = NULL;
		DRM_DEBUG("bo is %p, %d\n", entry, entry->num_pages);
		if (next != &bm->ddestroy) {
			nentry = list_entry(next, struct drm_buffer_object,
					    ddestroy);
			atomic_inc(&nentry->usage);
		}

		drm_bo_cleanup_refs(entry, remove_all);

		if (nentry)
			atomic_dec(&nentry->usage);
	}
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
static void drm_bo_delayed_workqueue(void *data)
#else
static void drm_bo_delayed_workqueue(struct work_struct *work)
#endif
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
	struct drm_device *dev = (struct drm_device *) data;
	struct drm_buffer_manager *bm = &dev->bm;
#else
	struct drm_buffer_manager *bm =
	    container_of(work, struct drm_buffer_manager, wq.work);
	struct drm_device *dev = container_of(bm, struct drm_device, bm);
#endif

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

void drm_bo_usage_deref_locked(struct drm_buffer_object **bo)
{
	struct drm_buffer_object *tmp_bo = *bo;
	bo = NULL;

	DRM_ASSERT_LOCKED(&tmp_bo->dev->struct_mutex);

	if (atomic_dec_and_test(&tmp_bo->usage))
		drm_bo_destroy_locked(tmp_bo);
}
EXPORT_SYMBOL(drm_bo_usage_deref_locked);

void drm_bo_usage_deref_unlocked(struct drm_buffer_object **bo)
{
	struct drm_buffer_object *tmp_bo = *bo;
	struct drm_device *dev = tmp_bo->dev;

	*bo = NULL;
	if (atomic_dec_and_test(&tmp_bo->usage)) {
		mutex_lock(&dev->struct_mutex);
		if (atomic_read(&tmp_bo->usage) == 0)
			drm_bo_destroy_locked(tmp_bo);
		mutex_unlock(&dev->struct_mutex);
	}
}
EXPORT_SYMBOL(drm_bo_usage_deref_unlocked);

void drm_putback_buffer_objects(struct drm_device *dev)
{
	struct drm_buffer_manager *bm = &dev->bm;
	struct list_head *list = &bm->unfenced;
	struct drm_buffer_object *entry, *next;

	mutex_lock(&dev->struct_mutex);
	list_for_each_entry_safe(entry, next, list, lru) {
		atomic_inc(&entry->usage);
		mutex_unlock(&dev->struct_mutex);

		mutex_lock(&entry->mutex);
		BUG_ON(!(entry->priv_flags & _DRM_BO_FLAG_UNFENCED));
		mutex_lock(&dev->struct_mutex);

		list_del_init(&entry->lru);
		DRM_FLAG_MASKED(entry->priv_flags, 0, _DRM_BO_FLAG_UNFENCED);
		wake_up_all(&entry->event_queue);

		/*
		 * FIXME: Might want to put back on head of list
		 * instead of tail here.
		 */

		drm_bo_add_to_lru(entry);
		mutex_unlock(&entry->mutex);
		drm_bo_usage_deref_locked(&entry);
	}
	mutex_unlock(&dev->struct_mutex);
}
EXPORT_SYMBOL(drm_putback_buffer_objects);

/*
 * Note. The caller has to register (if applicable)
 * and deregister fence object usage.
 */

int drm_fence_buffer_objects(struct drm_device *dev,
			     struct list_head *list,
			     uint32_t fence_flags,
			     struct drm_fence_object *fence,
			     struct drm_fence_object **used_fence)
{
	struct drm_buffer_manager *bm = &dev->bm;
	struct drm_buffer_object *entry;
	uint32_t fence_type = 0;
	uint32_t fence_class = ~0;
	int count = 0;
	int ret = 0;
	struct list_head *l;

	mutex_lock(&dev->struct_mutex);

	if (!list)
		list = &bm->unfenced;

	if (fence)
		fence_class = fence->fence_class;

	list_for_each_entry(entry, list, lru) {
		BUG_ON(!(entry->priv_flags & _DRM_BO_FLAG_UNFENCED));
		fence_type |= entry->new_fence_type;
		if (fence_class == ~0)
			fence_class = entry->new_fence_class;
		else if (entry->new_fence_class != fence_class) {
			DRM_ERROR("Unmatching fence classes on unfenced list: "
				  "%d and %d.\n",
				  fence_class,
				  entry->new_fence_class);
			ret = -EINVAL;
			goto out;
		}
		count++;
	}

	if (!count) {
		ret = -EINVAL;
		goto out;
	}

	if (fence) {
		if ((fence_type & fence->type) != fence_type ||
		    (fence->fence_class != fence_class)) {
			DRM_ERROR("Given fence doesn't match buffers "
				  "on unfenced list.\n");
			ret = -EINVAL;
			goto out;
		}
	} else {
		mutex_unlock(&dev->struct_mutex);
		ret = drm_fence_object_create(dev, fence_class, fence_type,
					      fence_flags | DRM_FENCE_FLAG_EMIT,
					      &fence);
		mutex_lock(&dev->struct_mutex);
		if (ret)
			goto out;
	}

	count = 0;
	l = list->next;
	while (l != list) {
		prefetch(l->next);
		entry = list_entry(l, struct drm_buffer_object, lru);
		atomic_inc(&entry->usage);
		mutex_unlock(&dev->struct_mutex);
		mutex_lock(&entry->mutex);
		mutex_lock(&dev->struct_mutex);
		list_del_init(l);
		if (entry->priv_flags & _DRM_BO_FLAG_UNFENCED) {
			count++;
			if (entry->fence)
				drm_fence_usage_deref_locked(&entry->fence);
			entry->fence = drm_fence_reference_locked(fence);
			entry->fence_class = entry->new_fence_class;
			entry->fence_type = entry->new_fence_type;
			DRM_FLAG_MASKED(entry->priv_flags, 0,
					_DRM_BO_FLAG_UNFENCED);
			wake_up_all(&entry->event_queue);
			drm_bo_add_to_lru(entry);
		}
		mutex_unlock(&entry->mutex);
		drm_bo_usage_deref_locked(&entry);
		l = list->next;
	}
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

static int drm_bo_evict(struct drm_buffer_object *bo, unsigned mem_type,
			int no_wait)
{
	int ret = 0;
	struct drm_device *dev = bo->dev;
	struct drm_bo_mem_reg evict_mem;

	/*
	 * Someone might have modified the buffer before we took the
	 * buffer mutex.
	 */

	do {
		bo->priv_flags &= ~_DRM_BO_FLAG_UNLOCKED;

		if (unlikely(bo->mem.flags &
			     (DRM_BO_FLAG_NO_MOVE | DRM_BO_FLAG_NO_EVICT)))
			goto out_unlock;
		if (unlikely(bo->priv_flags & _DRM_BO_FLAG_UNFENCED))
			goto out_unlock;
		if (unlikely(bo->mem.mem_type != mem_type))
			goto out_unlock;
		ret = drm_bo_wait(bo, 0, 1, no_wait, 0);
		if (ret)
			goto out_unlock;

	} while(bo->priv_flags & _DRM_BO_FLAG_UNLOCKED);

	evict_mem = bo->mem;
	evict_mem.mm_node = NULL;

	evict_mem = bo->mem;
	evict_mem.proposed_flags = dev->driver->bo_driver->evict_flags(bo);

	mutex_lock(&dev->struct_mutex);
	list_del_init(&bo->lru);
	mutex_unlock(&dev->struct_mutex);

	ret = drm_bo_mem_space(bo, &evict_mem, no_wait);

	if (ret) {
		if (ret != -EAGAIN)
			DRM_ERROR("Failed to find memory space for "
				  "buffer 0x%p eviction.\n", bo);
		goto out;
	}

	ret = drm_bo_handle_move_mem(bo, &evict_mem, 1, no_wait);

	if (ret) {
		if (ret != -EAGAIN)
			DRM_ERROR("Buffer eviction failed\n");
		goto out;
	}

	DRM_FLAG_MASKED(bo->priv_flags, _DRM_BO_FLAG_EVICTED,
			_DRM_BO_FLAG_EVICTED);

out:
	mutex_lock(&dev->struct_mutex);
	if (evict_mem.mm_node) {
		if (evict_mem.mm_node != bo->pinned_node)
			drm_mm_put_block(evict_mem.mm_node);
		evict_mem.mm_node = NULL;
	}
	drm_bo_add_to_lru(bo);
	BUG_ON(bo->priv_flags & _DRM_BO_FLAG_UNLOCKED);
out_unlock:
	mutex_unlock(&dev->struct_mutex);

	return ret;
}

/**
 * Repeatedly evict memory from the LRU for @mem_type until we create enough
 * space, or we've evicted everything and there isn't enough space.
 */
static int drm_bo_mem_force_space(struct drm_device *dev,
				  struct drm_bo_mem_reg *mem,
				  uint32_t mem_type, int no_wait)
{
	struct drm_mm_node *node;
	struct drm_buffer_manager *bm = &dev->bm;
	struct drm_buffer_object *entry;
	struct drm_mem_type_manager *man = &bm->man[mem_type];
	struct list_head *lru;
	unsigned long num_pages = mem->num_pages;
	int ret;

	mutex_lock(&dev->struct_mutex);
	do {
		node = drm_mm_search_free(&man->manager, num_pages,
					  mem->page_alignment, 1);
		if (node)
			break;

		lru = &man->lru;
		if (lru->next == lru)
			break;

		entry = list_entry(lru->next, struct drm_buffer_object, lru);
		atomic_inc(&entry->usage);
		mutex_unlock(&dev->struct_mutex);
		mutex_lock(&entry->mutex);
		ret = drm_bo_evict(entry, mem_type, no_wait);
		mutex_unlock(&entry->mutex);
		drm_bo_usage_deref_unlocked(&entry);
		if (ret)
			return ret;
		mutex_lock(&dev->struct_mutex);
	} while (1);

	if (!node) {
		mutex_unlock(&dev->struct_mutex);
		return -ENOMEM;
	}

	node = drm_mm_get_block(node, num_pages, mem->page_alignment);
	if (unlikely(!node)) {
		mutex_unlock(&dev->struct_mutex);
		return -ENOMEM;
	}

	mutex_unlock(&dev->struct_mutex);
	mem->mm_node = node;
	mem->mem_type = mem_type;
	return 0;
}

static int drm_bo_mt_compatible(struct drm_mem_type_manager *man,
				int disallow_fixed,
				uint32_t mem_type,
				uint64_t mask, uint32_t *res_mask)
{
	uint64_t cur_flags = drm_bo_type_flags(mem_type);
	uint64_t flag_diff;

	if ((man->flags & _DRM_FLAG_MEMTYPE_FIXED) && disallow_fixed)
		return 0;
	if (man->flags & _DRM_FLAG_MEMTYPE_CACHED)
		cur_flags |= DRM_BO_FLAG_CACHED;
	if (man->flags & _DRM_FLAG_MEMTYPE_MAPPABLE)
		cur_flags |= DRM_BO_FLAG_MAPPABLE;
	if (man->flags & _DRM_FLAG_MEMTYPE_CSELECT)
		DRM_FLAG_MASKED(cur_flags, mask, DRM_BO_FLAG_CACHED);

	if ((cur_flags & mask & DRM_BO_MASK_MEM) == 0)
		return 0;

	if (mem_type == DRM_BO_MEM_LOCAL) {
		*res_mask = cur_flags;
		return 1;
	}

	flag_diff = (mask ^ cur_flags);
	if (flag_diff & DRM_BO_FLAG_CACHED_MAPPED)
		cur_flags |= DRM_BO_FLAG_CACHED_MAPPED;

	if ((flag_diff & DRM_BO_FLAG_CACHED) &&
	    (!(mask & DRM_BO_FLAG_CACHED) ||
	     (mask & DRM_BO_FLAG_FORCE_CACHING)))
		return 0;

	if ((flag_diff & DRM_BO_FLAG_MAPPABLE) &&
	    ((mask & DRM_BO_FLAG_MAPPABLE) ||
	     (mask & DRM_BO_FLAG_FORCE_MAPPABLE)))
		return 0;

	*res_mask = cur_flags;
	return 1;
}

/**
 * Creates space for memory region @mem according to its type.
 *
 * This function first searches for free space in compatible memory types in
 * the priority order defined by the driver.  If free space isn't found, then
 * drm_bo_mem_force_space is attempted in priority order to evict and find
 * space.
 */
int drm_bo_mem_space(struct drm_buffer_object *bo,
		     struct drm_bo_mem_reg *mem, int no_wait)
{
	struct drm_device *dev = bo->dev;
	struct drm_buffer_manager *bm = &dev->bm;
	struct drm_mem_type_manager *man;

	uint32_t num_prios = dev->driver->bo_driver->num_mem_type_prio;
	const uint32_t *prios = dev->driver->bo_driver->mem_type_prio;
	uint32_t i;
	uint32_t mem_type = DRM_BO_MEM_LOCAL;
	uint32_t cur_flags;
	int type_found = 0;
	int type_ok = 0;
	int has_eagain = 0;
	struct drm_mm_node *node = NULL;
	int ret;

	mem->mm_node = NULL;
	for (i = 0; i < num_prios; ++i) {
		mem_type = prios[i];
		man = &bm->man[mem_type];

		type_ok = drm_bo_mt_compatible(man,
					       bo->type == drm_bo_type_user,
					       mem_type, mem->proposed_flags,
					       &cur_flags);

		if (!type_ok)
			continue;

		if (mem_type == DRM_BO_MEM_LOCAL)
			break;

		if ((mem_type == bo->pinned_mem_type) &&
		    (bo->pinned_node != NULL)) {
			node = bo->pinned_node;
			break;
		}

		mutex_lock(&dev->struct_mutex);
		if (man->has_type && man->use_type) {
			type_found = 1;
			node = drm_mm_search_free(&man->manager, mem->num_pages,
						  mem->page_alignment, 1);
			if (node)
				node = drm_mm_get_block(node, mem->num_pages,
							mem->page_alignment);
		}
		mutex_unlock(&dev->struct_mutex);
		if (node)
			break;
	}

	if ((type_ok && (mem_type == DRM_BO_MEM_LOCAL)) || node) {
		mem->mm_node = node;
		mem->mem_type = mem_type;
		mem->flags = cur_flags;
		return 0;
	}

	if (!type_found)
		return -EINVAL;

	num_prios = dev->driver->bo_driver->num_mem_busy_prio;
	prios = dev->driver->bo_driver->mem_busy_prio;

	for (i = 0; i < num_prios; ++i) {
		mem_type = prios[i];
		man = &bm->man[mem_type];

		if (!man->has_type)
			continue;

		if (!drm_bo_mt_compatible(man,
					  bo->type == drm_bo_type_user,
					  mem_type,
					  mem->proposed_flags,
					  &cur_flags))
			continue;

		ret = drm_bo_mem_force_space(dev, mem, mem_type, no_wait);

		if (ret == 0 && mem->mm_node) {
			mem->flags = cur_flags;
			return 0;
		}

		if (ret == -EAGAIN)
			has_eagain = 1;
	}

	ret = (has_eagain) ? -EAGAIN : -ENOMEM;
	return ret;
}
EXPORT_SYMBOL(drm_bo_mem_space);

/*
 * drm_bo_propose_flags:
 *
 * @bo: the buffer object getting new flags
 *
 * @new_flags: the new set of proposed flag bits
 *
 * @new_mask: the mask of bits changed in new_flags
 *
 * Modify the proposed_flag bits in @bo
 */
static int drm_bo_modify_proposed_flags (struct drm_buffer_object *bo,
					 uint64_t new_flags, uint64_t new_mask)
{
	uint32_t new_access;

	/* Copy unchanging bits from existing proposed_flags */
	DRM_FLAG_MASKED(new_flags, bo->mem.proposed_flags, ~new_mask);
	 
	if (bo->type == drm_bo_type_user &&
	    ((new_flags & (DRM_BO_FLAG_CACHED | DRM_BO_FLAG_FORCE_CACHING)) !=
	     (DRM_BO_FLAG_CACHED | DRM_BO_FLAG_FORCE_CACHING))) {
		DRM_ERROR("User buffers require cache-coherent memory.\n");
		return -EINVAL;
	}

	if (bo->type != drm_bo_type_kernel && (new_mask & DRM_BO_FLAG_NO_EVICT) && !DRM_SUSER(DRM_CURPROC)) {
		DRM_ERROR("DRM_BO_FLAG_NO_EVICT is only available to priviliged processes.\n");
		return -EPERM;
	}

	if (likely(new_mask & DRM_BO_MASK_MEM) &&
	    (bo->mem.flags & DRM_BO_FLAG_NO_EVICT) &&
	    !DRM_SUSER(DRM_CURPROC)) {
		if (likely(bo->mem.flags & new_flags & new_mask &
			   DRM_BO_MASK_MEM))
			new_flags = (new_flags & ~DRM_BO_MASK_MEM) |
				(bo->mem.flags & DRM_BO_MASK_MEM);
		else {
			DRM_ERROR("Incompatible memory type specification "
				  "for NO_EVICT buffer.\n");
			return -EPERM;
		}
	}

	if ((new_flags & DRM_BO_FLAG_NO_MOVE)) {
		DRM_ERROR("DRM_BO_FLAG_NO_MOVE is not properly implemented yet.\n");
		return -EPERM;
	}

	new_access = new_flags & (DRM_BO_FLAG_EXE | DRM_BO_FLAG_WRITE |
				  DRM_BO_FLAG_READ);

	if (new_access == 0) {
		DRM_ERROR("Invalid buffer object rwx properties\n");
		return -EINVAL;
	}

	bo->mem.proposed_flags = new_flags;
	return 0;
}

/*
 * Call bo->mutex locked.
 * Returns -EBUSY if the buffer is currently rendered to or from. 0 otherwise.
 * Doesn't do any fence flushing as opposed to the drm_bo_busy function.
 */

int drm_bo_quick_busy(struct drm_buffer_object *bo, int check_unfenced)
{
	struct drm_fence_object *fence = bo->fence;

	if (check_unfenced && (bo->priv_flags & _DRM_BO_FLAG_UNFENCED))
		return -EBUSY;

	if (fence) {
		if (drm_fence_object_signaled(fence, bo->fence_type)) {
			drm_fence_usage_deref_unlocked(&bo->fence);
			return 0;
		}
		return -EBUSY;
	}
	return 0;
}

int drm_bo_evict_cached(struct drm_buffer_object *bo)
{
	int ret = 0;

	BUG_ON(bo->priv_flags & _DRM_BO_FLAG_UNFENCED);
	if (bo->mem.mm_node)
		ret = drm_bo_evict(bo, DRM_BO_MEM_TT, 1);
	return ret;
}

EXPORT_SYMBOL(drm_bo_evict_cached);
/*
 * Wait until a buffer is unmapped.
 */

static int drm_bo_wait_unmapped(struct drm_buffer_object *bo, int no_wait)
{
	int ret = 0;

	if (likely(atomic_read(&bo->mapped)) == 0)
		return 0;

	if (unlikely(no_wait))
		return -EBUSY;

	do {
		mutex_unlock(&bo->mutex);
		ret = wait_event_interruptible(bo->event_queue,
					       atomic_read(&bo->mapped) == 0);
		mutex_lock(&bo->mutex);
		bo->priv_flags |= _DRM_BO_FLAG_UNLOCKED;

		if (ret == -ERESTARTSYS)
			ret = -EAGAIN;
	} while((ret == 0) && atomic_read(&bo->mapped) > 0);

	return ret;
}

/*
 * bo->mutex locked.
 * Note that new_mem_flags are NOT transferred to the bo->mem.proposed_flags.
 */

int drm_bo_move_buffer(struct drm_buffer_object *bo, uint64_t new_mem_flags,
		       int no_wait, int move_unfenced)
{
	struct drm_device *dev = bo->dev;
	struct drm_buffer_manager *bm = &dev->bm;
	int ret = 0;
	struct drm_bo_mem_reg mem;

	BUG_ON(bo->fence != NULL);

	mem.num_pages = bo->num_pages;
	mem.size = mem.num_pages << PAGE_SHIFT;
	mem.proposed_flags = new_mem_flags;
	mem.page_alignment = bo->mem.page_alignment;

	mutex_lock(&bm->evict_mutex);
	mutex_lock(&dev->struct_mutex);
	list_del_init(&bo->lru);
	mutex_unlock(&dev->struct_mutex);

	/*
	 * Determine where to move the buffer.
	 */
	ret = drm_bo_mem_space(bo, &mem, no_wait);
	if (ret)
		goto out_unlock;

	ret = drm_bo_handle_move_mem(bo, &mem, 0, no_wait);

out_unlock:
	mutex_lock(&dev->struct_mutex);
	if (ret || !move_unfenced) {
		if (mem.mm_node) {
			if (mem.mm_node != bo->pinned_node)
				drm_mm_put_block(mem.mm_node);
			mem.mm_node = NULL;
		}
		drm_bo_add_to_lru(bo);
		if (bo->priv_flags & _DRM_BO_FLAG_UNFENCED) {
			wake_up_all(&bo->event_queue);
			DRM_FLAG_MASKED(bo->priv_flags, 0,
					_DRM_BO_FLAG_UNFENCED);
		}
	} else {
		list_add_tail(&bo->lru, &bm->unfenced);
		DRM_FLAG_MASKED(bo->priv_flags, _DRM_BO_FLAG_UNFENCED,
				_DRM_BO_FLAG_UNFENCED);
	}
	/* clear the clean flags */
	bo->mem.flags &= ~DRM_BO_FLAG_CLEAN;
	bo->mem.proposed_flags &= ~DRM_BO_FLAG_CLEAN;

	mutex_unlock(&dev->struct_mutex);
	mutex_unlock(&bm->evict_mutex);
	return ret;
}

static int drm_bo_mem_compat(struct drm_bo_mem_reg *mem)
{
	uint32_t flag_diff = (mem->proposed_flags ^ mem->flags);

	if ((mem->proposed_flags & mem->flags & DRM_BO_MASK_MEM) == 0)
		return 0;
	if ((flag_diff & DRM_BO_FLAG_CACHED) &&
	    (/* !(mem->proposed_flags & DRM_BO_FLAG_CACHED) ||*/
	     (mem->proposed_flags & DRM_BO_FLAG_FORCE_CACHING)))
		return 0;

	if ((flag_diff & DRM_BO_FLAG_MAPPABLE) &&
	    ((mem->proposed_flags & DRM_BO_FLAG_MAPPABLE) ||
	     (mem->proposed_flags & DRM_BO_FLAG_FORCE_MAPPABLE)))
		return 0;
	return 1;
}

/**
 * drm_buffer_object_validate:
 *
 * @bo: the buffer object to modify
 *
 * @fence_class: the new fence class covering this buffer
 *
 * @move_unfenced: a boolean indicating whether switching the
 * memory space of this buffer should cause the buffer to
 * be placed on the unfenced list.
 *
 * @no_wait: whether this function should return -EBUSY instead
 * of waiting.
 *
 * Change buffer access parameters. This can involve moving
 * the buffer to the correct memory type, pinning the buffer
 * or changing the class/type of fence covering this buffer
 *
 * Must be called with bo locked.
 */

static int drm_buffer_object_validate(struct drm_buffer_object *bo,
				      uint32_t fence_class,
				      int move_unfenced, int no_wait,
				      int move_buffer)
{
	struct drm_device *dev = bo->dev;
	struct drm_buffer_manager *bm = &dev->bm;
	int ret;

	if (move_buffer) {
		ret = drm_bo_move_buffer(bo, bo->mem.proposed_flags, no_wait,
					 move_unfenced);
		if (ret) {
			if (ret != -EAGAIN)
				DRM_ERROR("Failed moving buffer.\n");
			if (ret == -ENOMEM)
				DRM_ERROR("Out of aperture space or "
					  "DRM memory quota.\n");
			return ret;
		}
	}

	/*
	 * Pinned buffers.
	 */

	if (bo->mem.proposed_flags & (DRM_BO_FLAG_NO_EVICT | DRM_BO_FLAG_NO_MOVE)) {
		bo->pinned_mem_type = bo->mem.mem_type;
		mutex_lock(&dev->struct_mutex);
		list_del_init(&bo->pinned_lru);
		drm_bo_add_to_pinned_lru(bo);

		if (bo->pinned_node != bo->mem.mm_node) {
			if (bo->pinned_node != NULL)
				drm_mm_put_block(bo->pinned_node);
			bo->pinned_node = bo->mem.mm_node;
		}

		mutex_unlock(&dev->struct_mutex);

	} else if (bo->pinned_node != NULL) {

		mutex_lock(&dev->struct_mutex);

		if (bo->pinned_node != bo->mem.mm_node)
			drm_mm_put_block(bo->pinned_node);

		list_del_init(&bo->pinned_lru);
		bo->pinned_node = NULL;
		mutex_unlock(&dev->struct_mutex);

	}

	/*
	 * We might need to add a TTM.
	 */

	if (bo->mem.mem_type == DRM_BO_MEM_LOCAL && bo->ttm == NULL) {
		ret = drm_bo_add_ttm(bo);
		if (ret)
			return ret;
	}
	/*
	 * Validation has succeeded, move the access and other
	 * non-mapping-related flag bits from the proposed flags to
	 * the active flags
	 */

	DRM_FLAG_MASKED(bo->mem.flags, bo->mem.proposed_flags, ~DRM_BO_MASK_MEMTYPE);

	/*
	 * Finally, adjust lru to be sure.
	 */

	mutex_lock(&dev->struct_mutex);
	list_del(&bo->lru);
	if (move_unfenced) {
		list_add_tail(&bo->lru, &bm->unfenced);
		DRM_FLAG_MASKED(bo->priv_flags, _DRM_BO_FLAG_UNFENCED,
				_DRM_BO_FLAG_UNFENCED);
	} else {
		drm_bo_add_to_lru(bo);
		if (bo->priv_flags & _DRM_BO_FLAG_UNFENCED) {
			wake_up_all(&bo->event_queue);
			DRM_FLAG_MASKED(bo->priv_flags, 0,
					_DRM_BO_FLAG_UNFENCED);
		}
	}
	mutex_unlock(&dev->struct_mutex);

	return 0;
}

/*
 * This function is called with bo->mutex locked, but may release it
 * temporarily to wait for events.
 */

static int drm_bo_prepare_for_validate(struct drm_buffer_object *bo,
				       uint64_t flags,
				       uint64_t mask,
				       uint32_t hint,
				       uint32_t fence_class,
				       int no_wait,
				       int *move_buffer)
{
	struct drm_device *dev = bo->dev;
	struct drm_bo_driver *driver = dev->driver->bo_driver;
	uint32_t ftype;

	int ret;


	ret = drm_bo_modify_proposed_flags (bo, flags, mask);
	if (ret)
		return ret;

	DRM_DEBUG("Proposed flags 0x%016llx, Old flags 0x%016llx\n",
		  (unsigned long long) bo->mem.proposed_flags,
		  (unsigned long long) bo->mem.flags);

	ret = drm_bo_wait_unmapped(bo, no_wait);
	if (ret)
		return ret;

	ret = driver->fence_type(bo, &fence_class, &ftype);

	if (ret) {
		DRM_ERROR("Driver did not support given buffer permissions.\n");
		return ret;
	}

	/*
	 * We're switching command submission mechanism,
	 * or cannot simply rely on the hardware serializing for us.
	 * Insert a driver-dependant barrier or wait for buffer idle.
	 */

	if ((fence_class != bo->fence_class) ||
	    ((ftype ^ bo->fence_type) & bo->fence_type)) {

		ret = -EINVAL;
		if (driver->command_stream_barrier) {
			ret = driver->command_stream_barrier(bo,
							     fence_class,
							     ftype,
							     no_wait);
		}
		if (ret && ret != -EAGAIN) 
			ret = drm_bo_wait(bo, 0, 1, no_wait, 1);
		
		if (ret)
			return ret;
	}

	bo->new_fence_class = fence_class;
	bo->new_fence_type = ftype;

	/*
	 * Check whether we need to move buffer.
	 */

	*move_buffer = 0;
	if (!drm_bo_mem_compat(&bo->mem)) {
		*move_buffer = 1;
		ret = drm_bo_wait(bo, 0, 1, no_wait, 1);
	}

	return ret;
}

/**
 * drm_bo_do_validate:
 *
 * @bo:	the buffer object
 *
 * @flags: access rights, mapping parameters and cacheability. See
 * the DRM_BO_FLAG_* values in drm.h
 *
 * @mask: Which flag values to change; this allows callers to modify
 * things without knowing the current state of other flags.
 *
 * @hint: changes the proceedure for this operation, see the DRM_BO_HINT_*
 * values in drm.h.
 *
 * @fence_class: a driver-specific way of doing fences. Presumably,
 * this would be used if the driver had more than one submission and
 * fencing mechanism. At this point, there isn't any use of this
 * from the user mode code.
 *
 * @rep: To be stuffed with the reply from validation
 * 
 * 'validate' a buffer object. This changes where the buffer is
 * located, along with changing access modes.
 */

int drm_bo_do_validate(struct drm_buffer_object *bo,
		       uint64_t flags, uint64_t mask, uint32_t hint,
		       uint32_t fence_class)
{
	int ret;
	int no_wait = (hint & DRM_BO_HINT_DONT_BLOCK) != 0;
	int move_buffer;

	mutex_lock(&bo->mutex);

	do {
		bo->priv_flags &= ~_DRM_BO_FLAG_UNLOCKED;

		ret = drm_bo_prepare_for_validate(bo, flags, mask, hint,
						  fence_class, no_wait,
						  &move_buffer);
		if (ret)
			goto out;

	} while(unlikely(bo->priv_flags & _DRM_BO_FLAG_UNLOCKED));

	ret = drm_buffer_object_validate(bo,
					 fence_class,
					 !(hint & DRM_BO_HINT_DONT_FENCE),
					 no_wait,
					 move_buffer);

	BUG_ON(bo->priv_flags & _DRM_BO_FLAG_UNLOCKED);
out:
	mutex_unlock(&bo->mutex);

	return ret;
}
EXPORT_SYMBOL(drm_bo_do_validate);

int drm_buffer_object_create(struct drm_device *dev,
			     unsigned long size,
			     enum drm_bo_type type,
			     uint64_t flags,
			     uint32_t hint,
			     uint32_t page_alignment,
			     unsigned long buffer_start,
			     struct drm_buffer_object **buf_obj)
{
	struct drm_buffer_manager *bm = &dev->bm;
	struct drm_buffer_object *bo;
	int ret = 0;
	unsigned long num_pages;

	size += buffer_start & ~PAGE_MASK;
	num_pages = (size + PAGE_SIZE - 1) >> PAGE_SHIFT;
	if (num_pages == 0) {
		DRM_ERROR("Illegal buffer object size %ld.\n", size);
		return -EINVAL;
	}

	bo = drm_ctl_calloc(1, sizeof(*bo), DRM_MEM_BUFOBJ);

	if (!bo)
		return -ENOMEM;

	mutex_init(&bo->mutex);
	mutex_lock(&bo->mutex);

	atomic_set(&bo->usage, 1);
	atomic_set(&bo->mapped, 0);
	DRM_INIT_WAITQUEUE(&bo->event_queue);
	INIT_LIST_HEAD(&bo->lru);
	INIT_LIST_HEAD(&bo->pinned_lru);
	INIT_LIST_HEAD(&bo->ddestroy);
#ifdef DRM_ODD_MM_COMPAT
	INIT_LIST_HEAD(&bo->p_mm_list);
	INIT_LIST_HEAD(&bo->vma_list);
#endif
	bo->dev = dev;
	bo->type = type;
	bo->num_pages = num_pages;
	bo->mem.mem_type = DRM_BO_MEM_LOCAL;
	bo->mem.num_pages = bo->num_pages;
	bo->mem.mm_node = NULL;
	bo->mem.page_alignment = page_alignment;
	bo->buffer_start = buffer_start & PAGE_MASK;
	bo->priv_flags = 0;
	bo->mem.flags = (DRM_BO_FLAG_MEM_LOCAL | DRM_BO_FLAG_CACHED |
			 DRM_BO_FLAG_MAPPABLE | DRM_BO_FLAG_CLEAN);
	bo->mem.proposed_flags = 0;
	atomic_inc(&bm->count);
	/*
	 * Use drm_bo_modify_proposed_flags to error-check the proposed flags
	 */
	flags |= DRM_BO_FLAG_CLEAN; /* or in the clean flag */

	ret = drm_bo_modify_proposed_flags (bo, flags, flags);
	if (ret)
		goto out_err;

	/*
	 * For drm_bo_type_device buffers, allocate
	 * address space from the device so that applications
	 * can mmap the buffer from there
	 */
	if (bo->type == drm_bo_type_device) {
		mutex_lock(&dev->struct_mutex);
		ret = drm_bo_setup_vm_locked(bo);
		mutex_unlock(&dev->struct_mutex);
		if (ret)
			goto out_err;
	}

	mutex_unlock(&bo->mutex);
	ret = drm_bo_do_validate(bo, 0, 0, hint | DRM_BO_HINT_DONT_FENCE,
				 0);
	if (ret)
		goto out_err_unlocked;

	*buf_obj = bo;
	return 0;

out_err:
	mutex_unlock(&bo->mutex);
out_err_unlocked:
	drm_bo_usage_deref_unlocked(&bo);
	return ret;
}
EXPORT_SYMBOL(drm_buffer_object_create);

static int drm_bo_leave_list(struct drm_buffer_object *bo,
			     uint32_t mem_type,
			     int free_pinned,
			     int allow_errors)
{
	struct drm_device *dev = bo->dev;
	int ret = 0;

	mutex_lock(&bo->mutex);

	ret = drm_bo_expire_fence(bo, allow_errors);
	if (ret)
		goto out;

	if (free_pinned) {
		DRM_FLAG_MASKED(bo->mem.flags, 0, DRM_BO_FLAG_NO_MOVE);
		mutex_lock(&dev->struct_mutex);
		list_del_init(&bo->pinned_lru);
		if (bo->pinned_node == bo->mem.mm_node)
			bo->pinned_node = NULL;
		if (bo->pinned_node != NULL) {
			drm_mm_put_block(bo->pinned_node);
			bo->pinned_node = NULL;
		}
		mutex_unlock(&dev->struct_mutex);
	}

	if (bo->mem.flags & DRM_BO_FLAG_NO_EVICT) {
		DRM_ERROR("A DRM_BO_NO_EVICT buffer present at "
			  "cleanup. Removing flag and evicting.\n");
		bo->mem.flags &= ~DRM_BO_FLAG_NO_EVICT;
		bo->mem.proposed_flags &= ~DRM_BO_FLAG_NO_EVICT;
	}

	if (bo->mem.mem_type == mem_type)
		ret = drm_bo_evict(bo, mem_type, 0);

	if (ret) {
		if (allow_errors) {
			goto out;
		} else {
			ret = 0;
			DRM_ERROR("Cleanup eviction failed\n");
		}
	}

out:
	mutex_unlock(&bo->mutex);
	return ret;
}


static struct drm_buffer_object *drm_bo_entry(struct list_head *list,
					 int pinned_list)
{
	if (pinned_list)
		return list_entry(list, struct drm_buffer_object, pinned_lru);
	else
		return list_entry(list, struct drm_buffer_object, lru);
}

/*
 * dev->struct_mutex locked.
 */

static int drm_bo_force_list_clean(struct drm_device *dev,
				   struct list_head *head,
				   unsigned mem_type,
				   int free_pinned,
				   int allow_errors,
				   int pinned_list)
{
	struct list_head *list, *next, *prev;
	struct drm_buffer_object *entry, *nentry;
	int ret;
	int do_restart;

	/*
	 * The list traversal is a bit odd here, because an item may
	 * disappear from the list when we release the struct_mutex or
	 * when we decrease the usage count. Also we're not guaranteed
	 * to drain pinned lists, so we can't always restart.
	 */

restart:
	nentry = NULL;
	list_for_each_safe(list, next, head) {
		prev = list->prev;

		entry = (nentry != NULL) ? nentry: drm_bo_entry(list, pinned_list);
		atomic_inc(&entry->usage);
		if (nentry) {
			atomic_dec(&nentry->usage);
			nentry = NULL;
		}

		/*
		 * Protect the next item from destruction, so we can check
		 * its list pointers later on.
		 */

		if (next != head) {
			nentry = drm_bo_entry(next, pinned_list);
			atomic_inc(&nentry->usage);
		}
		mutex_unlock(&dev->struct_mutex);

		ret = drm_bo_leave_list(entry, mem_type, free_pinned,
					allow_errors);
		mutex_lock(&dev->struct_mutex);

		drm_bo_usage_deref_locked(&entry);
		if (ret)
			return ret;

		/*
		 * Has the next item disappeared from the list?
		 */

		do_restart = ((next->prev != list) && (next->prev != prev));

		if (nentry != NULL && do_restart)
			drm_bo_usage_deref_locked(&nentry);

		if (do_restart)
			goto restart;
	}
	return 0;
}

int drm_bo_clean_mm(struct drm_device *dev, unsigned mem_type, int kern_clean)
{
	struct drm_buffer_manager *bm = &dev->bm;
	struct drm_mem_type_manager *man = &bm->man[mem_type];
	int ret = -EINVAL;

	if (mem_type >= DRM_BO_MEM_TYPES) {
		DRM_ERROR("Illegal memory type %d\n", mem_type);
		return ret;
	}

	if (!man->has_type) {
		DRM_ERROR("Trying to take down uninitialized "
			  "memory manager type %u\n", mem_type);
		return ret;
	}

	if ((man->kern_init_type) && (kern_clean == 0)) {
		DRM_ERROR("Trying to take down kernel initialized "
			  "memory manager type %u\n", mem_type);
		return -EPERM;
	}

	man->use_type = 0;
	man->has_type = 0;

	ret = 0;
	if (mem_type > 0) {
		BUG_ON(!list_empty(&bm->unfenced));
		drm_bo_force_list_clean(dev, &man->lru, mem_type, 1, 0, 0);
		drm_bo_force_list_clean(dev, &man->pinned, mem_type, 1, 0, 1);

		if (drm_mm_clean(&man->manager)) {
			drm_mm_takedown(&man->manager);
		} else {
			ret = -EBUSY;
		}
	}

	return ret;
}
EXPORT_SYMBOL(drm_bo_clean_mm);

/**
 *Evict all buffers of a particular mem_type, but leave memory manager
 *regions for NO_MOVE buffers intact. New buffers cannot be added at this
 *point since we have the hardware lock.
 */

int drm_bo_lock_mm(struct drm_device *dev, unsigned mem_type)
{
	int ret;
	struct drm_buffer_manager *bm = &dev->bm;
	struct drm_mem_type_manager *man = &bm->man[mem_type];

	if (mem_type == 0 || mem_type >= DRM_BO_MEM_TYPES) {
		DRM_ERROR("Illegal memory manager memory type %u.\n", mem_type);
		return -EINVAL;
	}

	if (!man->has_type) {
		DRM_ERROR("Memory type %u has not been initialized.\n",
			  mem_type);
		return 0;
	}

	ret = drm_bo_force_list_clean(dev, &man->lru, mem_type, 0, 1, 0);
	if (ret)
		return ret;
	ret = drm_bo_force_list_clean(dev, &man->pinned, mem_type, 0, 1, 1);

	return ret;
}

int drm_bo_init_mm(struct drm_device *dev, unsigned type,
		   unsigned long p_offset, unsigned long p_size,
		   int kern_init)
{
	struct drm_buffer_manager *bm = &dev->bm;
	int ret = -EINVAL;
	struct drm_mem_type_manager *man;

	if (type >= DRM_BO_MEM_TYPES) {
		DRM_ERROR("Illegal memory type %d\n", type);
		return ret;
	}

	man = &bm->man[type];
	if (man->has_type) {
		DRM_ERROR("Memory manager already initialized for type %d\n",
			  type);
		return ret;
	}

	ret = dev->driver->bo_driver->init_mem_type(dev, type, man);
	if (ret)
		return ret;

	ret = 0;
	if (type != DRM_BO_MEM_LOCAL) {
		if (!p_size) {
			DRM_ERROR("Zero size memory manager type %d\n", type);
			return ret;
		}
		ret = drm_mm_init(&man->manager, p_offset, p_size);
		if (ret)
			return ret;
	}
	man->has_type = 1;
	man->use_type = 1;
	man->kern_init_type = kern_init;
	man->size = p_size;

	INIT_LIST_HEAD(&man->lru);
	INIT_LIST_HEAD(&man->pinned);

	return 0;
}
EXPORT_SYMBOL(drm_bo_init_mm);

/*
 * This function is intended to be called on drm driver unload.
 * If you decide to call it from lastclose, you must protect the call
 * from a potentially racing drm_bo_driver_init in firstopen.
 * (This may happen on X server restart).
 */

int drm_bo_driver_finish(struct drm_device *dev)
{
	struct drm_buffer_manager *bm = &dev->bm;
	int ret = 0;
	unsigned i = DRM_BO_MEM_TYPES;
	struct drm_mem_type_manager *man;

	mutex_lock(&dev->struct_mutex);

	if (!bm->initialized)
		goto out;
	bm->initialized = 0;

	while (i--) {
		man = &bm->man[i];
		if (man->has_type) {
			man->use_type = 0;
			if ((i != DRM_BO_MEM_LOCAL) && drm_bo_clean_mm(dev, i, 1)) {
				ret = -EBUSY;
				DRM_ERROR("DRM memory manager type %d "
					  "is not clean.\n", i);
			}
			man->has_type = 0;
		}
	}
	mutex_unlock(&dev->struct_mutex);

	if (!cancel_delayed_work(&bm->wq))
		flush_scheduled_work();

	mutex_lock(&dev->struct_mutex);
	drm_bo_delayed_delete(dev, 1);
	if (list_empty(&bm->ddestroy))
		DRM_DEBUG("Delayed destroy list was clean\n");

	if (list_empty(&bm->man[0].lru))
		DRM_DEBUG("Swap list was clean\n");

	if (list_empty(&bm->man[0].pinned))
		DRM_DEBUG("NO_MOVE list was clean\n");

	if (list_empty(&bm->unfenced))
		DRM_DEBUG("Unfenced list was clean\n");

	if (bm->dummy_read_page) {
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,15))
		ClearPageReserved(bm->dummy_read_page);
#endif
		__free_page(bm->dummy_read_page);
	}

out:
	mutex_unlock(&dev->struct_mutex);
	return ret;
}
EXPORT_SYMBOL(drm_bo_driver_finish);

/*
 * This function is intended to be called on drm driver load.
 * If you decide to call it from firstopen, you must protect the call
 * from a potentially racing drm_bo_driver_finish in lastclose.
 * (This may happen on X server restart).
 */

int drm_bo_driver_init(struct drm_device *dev)
{
	struct drm_bo_driver *driver = dev->driver->bo_driver;
	struct drm_buffer_manager *bm = &dev->bm;
	int ret = -EINVAL;

	bm->dummy_read_page = NULL;
	mutex_lock(&dev->struct_mutex);
	if (!driver)
		goto out_unlock;

	bm->dummy_read_page = alloc_page(__GFP_ZERO | GFP_DMA32);
	if (!bm->dummy_read_page) {
		ret = -ENOMEM;
		goto out_unlock;
	}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,15))
	SetPageReserved(bm->dummy_read_page);
#endif

	/*
	 * Initialize the system memory buffer type.
	 * Other types need to be driver / IOCTL initialized.
	 */
	ret = drm_bo_init_mm(dev, DRM_BO_MEM_LOCAL, 0, 0, 1);
	if (ret) {
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,15))
		ClearPageReserved(bm->dummy_read_page);
#endif
		__free_page(bm->dummy_read_page);
		bm->dummy_read_page = NULL;
		goto out_unlock;
	}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
	INIT_WORK(&bm->wq, &drm_bo_delayed_workqueue, dev);
#else
	INIT_DELAYED_WORK(&bm->wq, drm_bo_delayed_workqueue);
#endif
	bm->initialized = 1;
	bm->nice_mode = 1;
	atomic_set(&bm->count, 0);
	bm->cur_pages = 0;
	INIT_LIST_HEAD(&bm->unfenced);
	INIT_LIST_HEAD(&bm->ddestroy);
out_unlock:
	mutex_unlock(&dev->struct_mutex);
	return ret;
}
EXPORT_SYMBOL(drm_bo_driver_init);

/*
 * buffer object vm functions.
 */

int drm_mem_reg_is_pci(struct drm_device *dev, struct drm_bo_mem_reg *mem)
{
	struct drm_buffer_manager *bm = &dev->bm;
	struct drm_mem_type_manager *man = &bm->man[mem->mem_type];

	if (!(man->flags & _DRM_FLAG_MEMTYPE_FIXED)) {
		if (mem->mem_type == DRM_BO_MEM_LOCAL)
			return 0;

		if (man->flags & _DRM_FLAG_MEMTYPE_CMA)
			return 0;

		if (mem->flags & DRM_BO_FLAG_CACHED)
			return 0;
	}
	return 1;
}
EXPORT_SYMBOL(drm_mem_reg_is_pci);

/**
 * \c Get the PCI offset for the buffer object memory.
 *
 * \param bo The buffer object.
 * \param bus_base On return the base of the PCI region
 * \param bus_offset On return the byte offset into the PCI region
 * \param bus_size On return the byte size of the buffer object or zero if
 *     the buffer object memory is not accessible through a PCI region.
 * \return Failure indication.
 *
 * Returns -EINVAL if the buffer object is currently not mappable.
 * Otherwise returns zero.
 */

int drm_bo_pci_offset(struct drm_device *dev,
		      struct drm_bo_mem_reg *mem,
		      unsigned long *bus_base,
		      unsigned long *bus_offset, unsigned long *bus_size)
{
	struct drm_buffer_manager *bm = &dev->bm;
	struct drm_mem_type_manager *man = &bm->man[mem->mem_type];

	*bus_size = 0;
	if (!(man->flags & _DRM_FLAG_MEMTYPE_MAPPABLE))
		return -EINVAL;

	if (drm_mem_reg_is_pci(dev, mem)) {
		*bus_offset = mem->mm_node->start << PAGE_SHIFT;
		*bus_size = mem->num_pages << PAGE_SHIFT;
		*bus_base = man->io_offset;
	}

	return 0;
}

/**
 * \c Kill all user-space virtual mappings of this buffer object.
 *
 * \param bo The buffer object.
 *
 * Call bo->mutex locked.
 */

void drm_bo_unmap_virtual(struct drm_buffer_object *bo)
{
	struct drm_device *dev = bo->dev;
	loff_t offset = ((loff_t) bo->map_list.hash.key) << PAGE_SHIFT;
	loff_t holelen = ((loff_t) bo->mem.num_pages) << PAGE_SHIFT;

	if (!dev->dev_mapping)
		return;

	unmap_mapping_range(dev->dev_mapping, offset, holelen, 1);
}

/**
 * drm_bo_takedown_vm_locked:
 *
 * @bo: the buffer object to remove any drm device mapping
 *
 * Remove any associated vm mapping on the drm device node that
 * would have been created for a drm_bo_type_device buffer
 */
void drm_bo_takedown_vm_locked(struct drm_buffer_object *bo)
{
	struct drm_map_list *list;
	drm_local_map_t *map;
	struct drm_device *dev = bo->dev;

	DRM_ASSERT_LOCKED(&dev->struct_mutex);
	if (bo->type != drm_bo_type_device)
		return;

	list = &bo->map_list;
	if (list->user_token) {
		drm_ht_remove_item(&dev->map_hash, &list->hash);
		list->user_token = 0;
	}
	if (list->file_offset_node) {
		drm_mm_put_block(list->file_offset_node);
		list->file_offset_node = NULL;
	}

	map = list->map;
	if (!map)
		return;

	drm_ctl_free(map, sizeof(*map), DRM_MEM_BUFOBJ);
	list->map = NULL;
	list->user_token = 0ULL;
	drm_bo_usage_deref_locked(&bo);
}
EXPORT_SYMBOL(drm_bo_takedown_vm_locked);

/**
 * drm_bo_setup_vm_locked:
 *
 * @bo: the buffer to allocate address space for
 *
 * Allocate address space in the drm device so that applications
 * can mmap the buffer and access the contents. This only
 * applies to drm_bo_type_device objects as others are not
 * placed in the drm device address space.
 */
static int drm_bo_setup_vm_locked(struct drm_buffer_object *bo)
{
	struct drm_map_list *list = &bo->map_list;
	drm_local_map_t *map;
	struct drm_device *dev = bo->dev;

	DRM_ASSERT_LOCKED(&dev->struct_mutex);
	list->map = drm_ctl_calloc(1, sizeof(*map), DRM_MEM_BUFOBJ);
	if (!list->map)
		return -ENOMEM;

	map = list->map;
	map->offset = 0;
	map->type = _DRM_TTM;
	map->flags = _DRM_REMOVABLE;
	map->size = bo->mem.num_pages * PAGE_SIZE;
	atomic_inc(&bo->usage);
	map->handle = (void *)bo;

	list->file_offset_node = drm_mm_search_free(&dev->offset_manager,
						    bo->mem.num_pages, 0, 0);

	if (unlikely(!list->file_offset_node)) {
		drm_bo_takedown_vm_locked(bo);
		return -ENOMEM;
	}

	list->file_offset_node = drm_mm_get_block(list->file_offset_node,
						  bo->mem.num_pages, 0);

	if (unlikely(!list->file_offset_node)) {
		drm_bo_takedown_vm_locked(bo);
		return -ENOMEM;
	}
		
	list->hash.key = list->file_offset_node->start;
	if (drm_ht_insert_item(&dev->map_hash, &list->hash)) {
		drm_bo_takedown_vm_locked(bo);
		return -ENOMEM;
	}

	list->user_token = ((uint64_t) list->hash.key) << PAGE_SHIFT;

	return 0;
}

/* used to EVICT VRAM lru at suspend time */
void drm_bo_evict_mm(struct drm_device *dev, int mem_type, int no_wait)
{
	struct drm_buffer_manager *bm = &dev->bm;
	struct drm_mem_type_manager *man = &bm->man[mem_type];
	struct drm_buffer_object *entry;
	/* we need to migrate all objects in VRAM */
	struct list_head *lru;
	int ret;
	/* evict all buffers on the LRU - won't evict pinned buffers */
	
	drm_mm_dump(&man->manager);
	mutex_lock(&dev->struct_mutex);
	do {
		lru = &man->lru;

redo:
		if (lru->next == &man->lru) {
			DRM_ERROR("lru empty\n");
			break;
		}

		entry = list_entry(lru->next, struct drm_buffer_object, lru);

		if (entry->mem.flags & DRM_BO_FLAG_DISCARDABLE) {
			lru = lru->next;
			goto redo;
		}

		atomic_inc(&entry->usage);
		mutex_unlock(&dev->struct_mutex);
		mutex_lock(&entry->mutex);

		ret = drm_bo_evict(entry, mem_type, no_wait);
		mutex_unlock(&entry->mutex);

		if (ret)
			DRM_ERROR("Evict failed for BO\n");

		mutex_lock(&entry->mutex);
		(void)drm_bo_expire_fence(entry, 0);
		mutex_unlock(&entry->mutex);
		drm_bo_usage_deref_unlocked(&entry);

		mutex_lock(&dev->struct_mutex);
	} while(1);

	mutex_unlock(&dev->struct_mutex);

}
EXPORT_SYMBOL(drm_bo_evict_mm);
