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

static inline uint32_t drm_flag_masked(uint32_t old, uint32_t new, 
				       uint32_t mask)
{
	return old ^ ((old ^ new) & mask);
}

int drm_fence_buffer_objects(drm_file_t * priv)
{
	drm_device_t *dev = priv->head->dev;
	drm_buffer_manager_t *bm = &dev->bm;

	drm_buffer_object_t *entry, *next;
	uint32_t fence_flags = 0;
	int count = 0;
	drm_fence_object_t *fence;
	int ret;

	mutex_lock(&bm->mutex);

	list_for_each_entry(entry, &bm->unfenced, head) {
		BUG_ON(!entry->unfenced);
		fence_flags |= entry->fence_flags;
		count++;
	}

	if (!count) {
		mutex_unlock(&bm->mutex);
		return 0;
	}

	fence = drm_calloc(1, sizeof(*fence), DRM_MEM_FENCE);

	if (!fence) {
		mutex_unlock(&bm->mutex);
		return -ENOMEM;
	}

	ret = drm_fence_object_init(dev, fence_flags, 1, fence);
	if (ret) {
		drm_free(fence, sizeof(*fence), DRM_MEM_FENCE);
		mutex_unlock(&bm->mutex);
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
	mutex_unlock(&bm->mutex);
	return 0;
}

/*
 * bm locked,
 * dev locked.
 */

static int drm_move_tt_to_local(drm_buffer_object_t * buf, int lazy)
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
	buf->flags &= ~(DRM_BO_FLAG_MEM_TT);
	buf->flags |= DRM_BO_FLAG_MEM_LOCAL | DRM_BO_FLAG_CACHED;

	return 0;
}

static void drm_bo_destroy_locked(drm_device_t * dev, drm_buffer_object_t * bo)
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
	if (bo->ttm_region) {
		drm_destroy_ttm_region(bo->ttm_region);
	}
	if (bo->ttm_object) {
		drm_ttm_object_deref_locked(dev, bo->ttm_object);
	}
	drm_free(bo, sizeof(*bo), DRM_MEM_BUFOBJ);
}

void drm_bo_usage_deref_locked(drm_device_t * dev, drm_buffer_object_t * bo)
{
	if (atomic_dec_and_test(&bo->usage)) {
		drm_bo_destroy_locked(dev, bo);
	}
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

static void drm_bo_base_deref_locked(drm_file_t * priv, drm_user_object_t * uo)
{
	drm_bo_usage_deref_locked(priv->head->dev,
				  drm_user_object_entry(uo, drm_buffer_object_t,
							base));
}

static int drm_bo_new_flags(drm_bo_driver_t * driver,
			    uint32_t flags, uint32_t new_mask, uint32_t hint,
			    int init, uint32_t * n_flags)
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

	if (new_mask & DRM_BO_FLAG_BIND_CACHED) {
		new_flags |= DRM_BO_FLAG_CACHED;
		if (((new_flags & DRM_BO_FLAG_MEM_TT) && !driver->cached_tt) ||
		    ((new_flags & DRM_BO_FLAG_MEM_VRAM)
		     && !driver->cached_vram))
			new_flags &= ~DRM_BO_FLAG_CACHED;
	}

	if ((new_flags & DRM_BO_FLAG_NO_EVICT) &&
	    ((flags ^ new_flags) & DRM_BO_FLAG_CACHED)) {
		if (flags & DRM_BO_FLAG_CACHED) {
			DRM_ERROR
			    ("Cannot change caching policy of pinned buffer\n");
			return -EINVAL;
		} else {
			new_flags &= ~DRM_BO_FLAG_CACHED;
		}
	}

	*n_flags = new_flags;
	return 0;
}

#if 0

static int drm_bo_evict(drm_device_t * dev, drm_buffer_object_t * buf, int tt);
{
	int ret;
	if (tt) {
		ret = drm_move_tt_to_local(buf);
	} else {
		ret = drm_move_vram_to_local(buf);
	}
	return ret;
}

int drm_bo_alloc_space(drm_device_t * dev, int tt, drm_buffer_object_t * buf)
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
			bo = list_entry(lru->next, drm_buffer_object_t,
					vram_lru);
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
		DRM_ERROR("Could not find buffer object 0x%08x\n",
			  handle);
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
 * Wait until the buffer is idle.
 */

static int drm_bo_wait(drm_device_t * dev, drm_buffer_object_t * bo, int lazy)
{

	drm_fence_object_t *fence = bo->fence;
	int ret;

	if (fence) {
		if (drm_fence_object_signaled(fence, bo->fence_flags)) {
			drm_fence_usage_deref_unlocked(dev, fence);
			bo->fence = NULL;
			return 0;
		}

		/*
		 * Make sure another process doesn't destroy the fence 
		 * object when we release the buffer object mutex.
		 * We don't need to take the dev->struct_mutex lock here,
		 * since the fence usage count is at least 1 already.
		 */

		atomic_inc(&fence->usage);
		mutex_unlock(&bo->mutex);
		ret =
		    drm_fence_object_wait(dev, fence, lazy, !lazy,
					  bo->fence_flags);
		mutex_lock(&bo->mutex);
		if (ret)
			return ret;
		mutex_lock(&dev->struct_mutex);
		drm_fence_usage_deref_locked(dev, fence);
		if (bo->fence == fence) {
			drm_fence_usage_deref_locked(dev, fence);
			bo->fence = NULL;
		}
		mutex_unlock(&dev->struct_mutex);
	}
	return 0;
}

/*
 * Call bo->mutex locked.
 * Returns 1 if the buffer is currently rendered to or from. 0 otherwise.
 */

static int drm_bo_busy(drm_device_t * dev, drm_buffer_object_t * bo)
{
	drm_fence_object_t *fence = bo->fence;

	if (fence) {
		if (drm_fence_object_signaled(fence, bo->fence_flags)) {
			drm_fence_usage_deref_unlocked(dev, fence);
			bo->fence = NULL;
			return 0;
		}
		drm_fence_object_flush(dev, fence, DRM_FENCE_EXE);
		if (drm_fence_object_signaled(fence, bo->fence_flags)) {
			drm_fence_usage_deref_unlocked(dev, fence);
			bo->fence = NULL;
			return 0;
		}
		return 1;
	}
	return 0;
}

/*
 * Wait for buffer idle and register that we've mapped the buffer.
 * Mapping is registered as a drm_ref_object with type _DRM_REF_TYPE1, 
 * so that if the client dies, the mapping is automatically 
 * unregistered.
 */

static int drm_buffer_object_map(drm_file_t * priv, uint32_t handle, int wait)
{
	drm_buffer_object_t *bo;
	drm_device_t *dev = priv->head->dev;
	int ret;

	mutex_lock(&dev->struct_mutex);
	bo = drm_lookup_buffer_object(priv, handle, 1);
	mutex_unlock(&dev->struct_mutex);

	if (!bo)
		return -EINVAL;

	mutex_lock(&bo->mutex);

	if (!wait) {
		if ((atomic_read(&bo->mapped) == 0) && drm_bo_busy(dev, bo)) {
			mutex_unlock(&bo->mutex);
			ret = -EBUSY;
			goto out;
		}
	} else {
		ret = drm_bo_wait(dev, bo, 0);
		if (ret) {
			mutex_unlock(&bo->mutex);
			ret = -EBUSY;
			goto out;
		}
	}
	mutex_lock(&dev->struct_mutex);
	ret = drm_add_ref_object(priv, &bo->base, _DRM_REF_TYPE1);
	mutex_unlock(&dev->struct_mutex);
	if (!ret) {
		atomic_inc(&bo->mapped);
	}
	mutex_unlock(&bo->mutex);

      out:
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
	drm_device_t *dev = priv->head->dev;
	drm_buffer_object_t *bo =
	    drm_user_object_entry(uo, drm_buffer_object_t, base);

	BUG_ON(action != _DRM_REF_TYPE1);

	if (atomic_dec_and_test(&bo->mapped)) {
		mutex_unlock(&dev->struct_mutex);
		mutex_lock(&bo->mutex);
		if (atomic_read(&bo->mapped) == 0) {
			DRM_WAKEUP(&bo->validate_queue);
		}
		mutex_unlock(&bo->mutex);
		mutex_lock(&dev->struct_mutex);
	}
}

static int drm_bo_move_buffer(drm_buffer_object_t *bo, uint32_t new_flags,
			      int no_wait)
{
	return 0;
}


/*
 * bo locked.
 */


static int drm_buffer_object_validate(drm_buffer_object_t * bo,
				      uint32_t new_flags,
				      int move_unfenced,
				      int no_wait)
{
	drm_device_t *dev = bo->dev;
	drm_buffer_manager_t *bm = &dev->bm;
	uint32_t flag_diff = (new_flags ^ bo->flags);

	int ret;

	if (new_flags & DRM_BO_FLAG_MEM_VRAM) {
		DRM_ERROR("Vram support not implemented yet\n");
		return -EINVAL;
	}
	if ((new_flags & (DRM_BO_FLAG_MEM_TT | DRM_BO_FLAG_MEM_VRAM)) &&
	    (new_flags & DRM_BO_FLAG_CACHED)) {
		DRM_ERROR("Cached binding not implemented yet\n");
		return -EINVAL;
	}

	/*
	 * Check whether we need to move buffer.
	 */

	if (flag_diff & DRM_BO_MASK_MEM) {
		mutex_lock(&bm->mutex);
		ret = drm_bo_move_buffer(bo, new_flags, no_wait);
		mutex_unlock(&bm->mutex);
		if (ret) 
			return ret;
	}
	
	if (flag_diff & DRM_BO_FLAG_NO_EVICT) {
		mutex_lock(&bm->mutex);
		list_del_init(&bo->head);
		if (!(new_flags & DRM_BO_FLAG_NO_EVICT)) {
			if (new_flags & DRM_BO_FLAG_MEM_TT) {
				list_add_tail(&bo->head, &bm->tt_lru);
			} else {
				list_add_tail(&bo->head, &bm->vram_lru);
			}
		}
		mutex_unlock(&bm->mutex);
		DRM_FLAG_MASKED(bo->flags, new_flags, DRM_BO_FLAG_NO_EVICT);
	}

	if (move_unfenced) {

		/*
		 * Place on unfenced list.
		 */
	
		mutex_lock(&bm->mutex);
		list_del_init(&bo->head);
		list_add_tail(&bo->head, &bm->unfenced);
		mutex_unlock(&bm->mutex);
	}

	/*
	 * FIXME: Remove below.
	 */

	bo->flags = new_flags;
	return 0;
}
	
/*
 * Call bo->mutex locked.
 */

static int drm_bo_add_ttm(drm_file_t * priv, drm_buffer_object_t * bo,
			  uint32_t mask, uint32_t ttm_handle)
{
	drm_device_t *dev = bo->dev;
	drm_ttm_object_t *to = NULL;
	drm_ttm_t *ttm;
	int ret = 0;
	uint32_t ttm_flags = 0;

	bo->ttm_object = NULL;
	bo->ttm_region = NULL;

	switch (bo->type) {
	case drm_bo_type_dc:
		mutex_lock(&dev->struct_mutex);
		ret = drm_ttm_object_create(dev, bo->num_pages * PAGE_SIZE,
					    ttm_flags, &to);
		mutex_unlock(&dev->struct_mutex);
		break;
	case drm_bo_type_ttm:
		mutex_lock(&dev->struct_mutex);
		to = drm_lookup_ttm_object(priv, ttm_handle, 1);
		mutex_unlock(&dev->struct_mutex);
		if (!to) {
			DRM_ERROR("Could not find TTM object\n");
			ret = -EINVAL;
		}
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
		ttm = drm_ttm_from_object(to);
		ret = drm_create_ttm_region(ttm, bo->buffer_start >> PAGE_SHIFT,
					    bo->num_pages,
					    mask & DRM_BO_FLAG_BIND_CACHED,
					    &bo->ttm_region);
		if (ret) {
			drm_ttm_object_deref_unlocked(dev, to);
		}
	}
	return ret;
}

int drm_buffer_object_create(drm_file_t * priv,
			     unsigned long size,
			     drm_bo_type_t type,
			     uint32_t ttm_handle,
			     uint32_t mask,
			     uint32_t hint,
			     unsigned long buffer_start,
			     drm_buffer_object_t ** buf_obj)
{
	drm_device_t *dev = priv->head->dev;
	drm_buffer_object_t *bo;
	int ret = 0;
	uint32_t new_flags;
	unsigned long num_pages;

	if (buffer_start & ~PAGE_MASK) {
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
	atomic_set(&bo->mapped, 0);
	DRM_INIT_WAITQUEUE(&bo->validate_queue);
	INIT_LIST_HEAD(&bo->head);
	INIT_LIST_HEAD(&bo->ddestroy);
	bo->dev = dev;
	bo->type = type;
	bo->num_pages = num_pages;
	bo->buffer_start = buffer_start;

	ret = drm_bo_new_flags(dev->driver->bo_driver, bo->flags, mask, hint,
			       1, &new_flags);
	if (ret)
		goto out_err;
	ret = drm_bo_add_ttm(priv, bo, mask, ttm_handle);
	if (ret)
		goto out_err;

	bo->mask = mask;

	ret = drm_buffer_object_validate(bo, new_flags, 0, 
					 hint & DRM_BO_HINT_DONT_BLOCK);
	if (ret)
		goto out_err;

	mutex_unlock(&bo->mutex);
	*buf_obj = bo;
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

static void drm_bo_fill_rep_arg(const drm_buffer_object_t * bo,
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

	rep->map_flags = bo->map_flags;
	rep->mask = bo->mask;
	rep->buffer_start = bo->buffer_start;
}

int drm_bo_ioctl(DRM_IOCTL_ARGS)
{
	DRM_DEVICE;
	drm_bo_arg_t arg;
	drm_bo_arg_request_t *req = &arg.req;
	drm_bo_arg_reply_t rep;
	unsigned long next;
	drm_user_object_t *uo;
	drm_buffer_object_t *entry;

	do {
		DRM_COPY_FROM_USER_IOCTL(arg, (void __user *)data, sizeof(arg));
		rep.ret = 0;
		rep.handled = 0;
		switch (req->op) {
		case drm_bo_create:{
				unsigned long buffer_start = req->buffer_start;
				rep.ret =
				    drm_buffer_object_create(priv, req->size,
							     req->type,
							     req->arg_handle,
							     req->mask,
							     req->hint,
							     buffer_start,
							     &entry);
				if (rep.ret)
					break;

				rep.ret =
				    drm_bo_add_user_object(priv, entry,
							   req->
							   mask &
							   DRM_BO_FLAG_SHAREABLE);
				if (rep.ret)
					drm_bo_usage_deref_unlocked(dev, entry);

				mutex_lock(&entry->mutex);
				drm_bo_fill_rep_arg(entry, &rep);
				mutex_unlock(&entry->mutex);
				break;
			}
		case drm_bo_unmap:
			rep.ret = drm_buffer_object_unmap(priv, req->handle);
			break;
		case drm_bo_map:
			rep.ret = drm_buffer_object_map(priv, req->handle,
							!(req->hint &
							  DRM_BO_HINT_DONT_BLOCK));
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
		default:
			rep.ret = -EINVAL;
		}
		next = req->next;
		rep.handled = 1;
		arg.rep = rep;
		DRM_COPY_TO_USER_IOCTL((void __user *)data, arg, sizeof(arg));
		data = next;

	} while (data);
	return 0;

}

static void drm_bo_clean_mm(drm_file_t *priv)
{
}


int drm_mm_init_ioctl(DRM_IOCTL_ARGS)
{
	DRM_DEVICE;
	
	int ret = 0;
	drm_mm_init_arg_t arg;
	drm_buffer_manager_t *bm = &dev->bm;
	drm_bo_driver_t *driver = dev->driver->bo_driver;

	if (!driver) {
		DRM_ERROR("Buffer objects is not supported by this driver\n");
		return -EINVAL;
	}

	DRM_COPY_FROM_USER_IOCTL(arg, (void __user *)data, sizeof(arg));

	switch(arg.req.op) {
	case mm_init:
		if (bm->initialized) {
			DRM_ERROR("Memory manager already initialized\n");
			return -EINVAL;
		}
		mutex_init(&bm->mutex);
		mutex_lock(&bm->mutex);
		bm->has_vram = 0;
		bm->has_tt = 0;

		if (arg.req.vr_p_size) {
			ret = drm_mm_init(&bm->vram_manager, 
					  arg.req.vr_p_offset, 
					  arg.req.vr_p_size);
			bm->has_vram = 1;
			if (ret)
				break;
		}

		if (arg.req.tt_p_size) {
			ret = drm_mm_init(&bm->tt_manager, 
					  arg.req.tt_p_offset, 
					  arg.req.tt_p_size);
			bm->has_tt = 1;
			if (ret) {
				if (bm->has_vram)
					drm_mm_takedown(&bm->vram_manager);
				break;
			}
		}
		arg.rep.mm_sarea = 0;

		INIT_LIST_HEAD(&bm->vram_lru);
		INIT_LIST_HEAD(&bm->tt_lru);
		INIT_LIST_HEAD(&bm->unfenced);
		INIT_LIST_HEAD(&bm->ddestroy);

		bm->initialized = 1;
		break;
	case mm_takedown:
		if (!bm->initialized) {
			DRM_ERROR("Memory manager was not initialized\n");
			return -EINVAL;
		}
		mutex_lock(&bm->mutex);
		drm_bo_clean_mm(priv);
		if (bm->has_vram)
			drm_mm_takedown(&bm->vram_manager);
		if (bm->has_tt)
			drm_mm_takedown(&bm->tt_manager);
		bm->initialized = 0;
		break;
	default:
		return -EINVAL;
	}
		
	mutex_unlock(&bm->mutex);
	if (ret)
		return ret;
	
	DRM_COPY_TO_USER_IOCTL((void __user *)data, arg, sizeof(arg));
	return 0;
}
