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
 * Typically called by the IRQ handler.
 */

void drm_fence_handler(drm_device_t * dev, uint32_t sequence, uint32_t type)
{
	int wake = 0;
	uint32_t diff;
	uint32_t relevant;
	drm_fence_manager_t *fm = &dev->fm;
	drm_fence_driver_t *driver = dev->driver->fence_driver;
	struct list_head *list, *prev;
	drm_fence_object_t *fence;
	int found = 0;

	if (list_empty(&fm->ring))
		return;

	list_for_each_entry(fence, &fm->ring, ring) {
		diff = (sequence - fence->sequence) & driver->sequence_mask;
		if (diff > driver->wrap_diff) {
			found = 1;
			break;
		}
	}

	list = (found) ? fence->ring.prev : fm->ring.prev;
	prev = list->prev;

	for (; list != &fm->ring; list = prev, prev = list->prev) {
		fence = list_entry(list, drm_fence_object_t, ring);

		type |= fence->native_type;
		relevant = type & fence->type;

		if ((fence->signaled | relevant) != fence->signaled) {
			fence->signaled |= relevant;
			DRM_DEBUG("Fence 0x%08lx signaled 0x%08x\n",
				  fence->base.hash.key, fence->signaled);
			fence->submitted_flush |= relevant;
			wake = 1;
		}

		relevant = fence->flush_mask &
		    ~(fence->signaled | fence->submitted_flush);

		if (relevant) {
			fm->pending_flush |= relevant;
			fence->submitted_flush = fence->flush_mask;
		}

		if (!(fence->type & ~fence->signaled)) {
			DRM_DEBUG("Fence completely signaled 0x%08lx\n",
				  fence->base.hash.key);
			list_del_init(&fence->ring);
		}

	}
		
	if (wake) {
		DRM_WAKEUP(&fm->fence_queue);
	}
}

EXPORT_SYMBOL(drm_fence_handler);

static void drm_fence_unring(drm_device_t * dev, struct list_head *ring)
{
	drm_fence_manager_t *fm = &dev->fm;
	unsigned long flags;

	write_lock_irqsave(&fm->lock, flags);
	list_del_init(ring);
	write_unlock_irqrestore(&fm->lock, flags);
}

void drm_fence_usage_deref_locked(drm_device_t * dev,
				  drm_fence_object_t * fence)
{
	if (atomic_dec_and_test(&fence->usage)) {
		drm_fence_unring(dev, &fence->ring);
		DRM_DEBUG("Destroyed a fence object 0x%08lx\n",
			  fence->base.hash.key);
		kmem_cache_free(drm_cache.fence_object, fence);
	}
}

void drm_fence_usage_deref_unlocked(drm_device_t * dev,
				    drm_fence_object_t * fence)
{
	if (atomic_dec_and_test(&fence->usage)) {
		mutex_lock(&dev->struct_mutex);
		if (atomic_read(&fence->usage) == 0) {
			drm_fence_unring(dev, &fence->ring);
			kmem_cache_free(drm_cache.fence_object, fence);
		}
		mutex_unlock(&dev->struct_mutex);
	}
}

static void drm_fence_object_destroy(drm_file_t * priv,
				     drm_user_object_t * base)
{
	drm_device_t *dev = priv->head->dev;
	drm_fence_object_t *fence =
	    drm_user_object_entry(base, drm_fence_object_t, base);

	drm_fence_usage_deref_locked(dev, fence);
}

static int fence_signaled(drm_device_t * dev, drm_fence_object_t * fence,
			  uint32_t mask, int poke_flush)
{
	unsigned long flags;
	int signaled;
	drm_fence_manager_t *fm = &dev->fm;
	drm_fence_driver_t *driver = dev->driver->fence_driver;

	if (poke_flush)
		driver->poke_flush(dev);
	read_lock_irqsave(&fm->lock, flags);
	signaled =
	    (fence->type & mask & fence->signaled) == (fence->type & mask);
	read_unlock_irqrestore(&fm->lock, flags);

	return signaled;
}

static void drm_fence_flush_exe(drm_fence_manager_t * fm,
				drm_fence_driver_t * driver, uint32_t sequence)
{
	uint32_t diff;

	if (!fm->pending_exe_flush) {
		struct list_head *list;

		/*
		 * Last_exe_flush is invalid. Find oldest sequence.
		 */

/*		list = fm->fence_types[_DRM_FENCE_TYPE_EXE];*/
		list = &fm->ring;
		if (list->next == &fm->ring) {
			return;
		} else {
			drm_fence_object_t *fence =
			    list_entry(list->next, drm_fence_object_t, ring);
			fm->last_exe_flush = (fence->sequence - 1) &
			    driver->sequence_mask;
		}
		diff = (sequence - fm->last_exe_flush) & driver->sequence_mask;
		if (diff >= driver->wrap_diff)
			return;
		fm->exe_flush_sequence = sequence;
		fm->pending_exe_flush = 1;
	} else {
		diff =
		    (sequence - fm->exe_flush_sequence) & driver->sequence_mask;
		if (diff < driver->wrap_diff) {
			fm->exe_flush_sequence = sequence;
		}
	}
}

int drm_fence_object_signaled(drm_fence_object_t * fence, uint32_t type)
{
	return ((fence->signaled & type) == type);
}

/*
 * Make sure old fence objects are signaled before their fence sequences are
 * wrapped around and reused.
 */

int drm_fence_object_flush(drm_device_t * dev,
			   drm_fence_object_t * fence, uint32_t type)
{
	drm_fence_manager_t *fm = &dev->fm;
	drm_fence_driver_t *driver = dev->driver->fence_driver;
	unsigned long flags;

	if (type & ~fence->type) {
		DRM_ERROR("Flush trying to extend fence type, "
                          "0x%x, 0x%x\n", type, fence->type);
		return -EINVAL;
	}

	write_lock_irqsave(&fm->lock, flags);
	fence->flush_mask |= type;
	if (fence->submitted_flush == fence->signaled) {
		if ((fence->type & DRM_FENCE_TYPE_EXE) &&
		    !(fence->submitted_flush & DRM_FENCE_TYPE_EXE)) {
			drm_fence_flush_exe(fm, driver, fence->sequence);
			fence->submitted_flush |= DRM_FENCE_TYPE_EXE;
		} else {
			fm->pending_flush |= (fence->flush_mask &
					      ~fence->submitted_flush);
			fence->submitted_flush = fence->flush_mask;
		}
	}
	write_unlock_irqrestore(&fm->lock, flags);
	driver->poke_flush(dev);
	return 0;
}

void drm_fence_flush_old(drm_device_t * dev, uint32_t sequence)
{
	drm_fence_manager_t *fm = &dev->fm;
	drm_fence_driver_t *driver = dev->driver->fence_driver;
	uint32_t old_sequence;
	unsigned long flags;
	drm_fence_object_t *fence;
	uint32_t diff;

	mutex_lock(&dev->struct_mutex);
	read_lock_irqsave(&fm->lock, flags);
	if (fm->ring.next == &fm->ring) {
		read_unlock_irqrestore(&fm->lock, flags);
		mutex_unlock(&dev->struct_mutex);
		return;
	}
	old_sequence = (sequence - driver->flush_diff) & driver->sequence_mask;
	fence = list_entry(fm->ring.next, drm_fence_object_t, ring);
	atomic_inc(&fence->usage);
	mutex_unlock(&dev->struct_mutex);
	diff = (old_sequence - fence->sequence) & driver->sequence_mask;
	read_unlock_irqrestore(&fm->lock, flags);
	if (diff < driver->wrap_diff) {
		drm_fence_object_flush(dev, fence, fence->type);
	}
	drm_fence_usage_deref_unlocked(dev, fence);
}

EXPORT_SYMBOL(drm_fence_flush_old);

int drm_fence_object_wait(drm_device_t * dev, drm_fence_object_t * fence,
			  int lazy, int ignore_signals, uint32_t mask)
{
	drm_fence_manager_t *fm = &dev->fm;
	drm_fence_driver_t *driver = dev->driver->fence_driver;
	int ret = 0;
	unsigned long _end;
	int signaled;

	if (mask & ~fence->type) {
		DRM_ERROR("Wait trying to extend fence type"
			  " 0x%08x 0x%08x\n", mask, fence->type);
		return -EINVAL;
	}

	if (fence_signaled(dev, fence, mask, 0))
		return 0;

	_end = jiffies + 3 * DRM_HZ;

	drm_fence_object_flush(dev, fence, mask);

	if (lazy && driver->lazy_capable) {

		do {
			DRM_WAIT_ON(ret, fm->fence_queue, 3 * DRM_HZ,
				    fence_signaled(dev, fence, mask, 1));
			if (time_after_eq(jiffies, _end))
				break;
		} while (ret == -EINTR && ignore_signals);
		if (time_after_eq(jiffies, _end) && (ret != 0))
			ret = -EBUSY;
		if (ret) {
			if (ret == -EBUSY) {
				DRM_ERROR("Fence timout. GPU lockup.\n");
			}
			return ((ret == -EINTR) ? -EAGAIN : ret);
		}
	} else if ((fence->class == 0) && (mask & DRM_FENCE_TYPE_EXE) &&
		   driver->lazy_capable) {

		/*
		 * We use IRQ wait for EXE fence if available to gain 
		 * CPU in some cases.
		 */

		do {
			DRM_WAIT_ON(ret, fm->fence_queue, 3 * DRM_HZ,
				    fence_signaled(dev, fence, DRM_FENCE_TYPE_EXE,
						   1));
			if (time_after_eq(jiffies, _end))
				break;
		} while (ret == -EINTR && ignore_signals);
		if (time_after_eq(jiffies, _end) && (ret != 0))
			ret = -EBUSY;
		if (ret)
			return ((ret == -EINTR) ? -EAGAIN : ret);
	}

	if (fence_signaled(dev, fence, mask, 0))
		return 0;

	/*
	 * Avoid kernel-space busy-waits.
	 */
#if 1
	if (!ignore_signals)
	        return -EAGAIN;
#endif
	do { 
		schedule();
		signaled = fence_signaled(dev, fence, mask, 1);
	} while (!signaled && !time_after_eq(jiffies, _end));

	if (!signaled)
		return -EBUSY;

	return 0;
}

int drm_fence_object_emit(drm_device_t * dev, drm_fence_object_t * fence,
			  uint32_t fence_flags, uint32_t type)
{
	drm_fence_manager_t *fm = &dev->fm;
	drm_fence_driver_t *driver = dev->driver->fence_driver;
	unsigned long flags;
	uint32_t sequence;
	uint32_t native_type;
	int ret;

	drm_fence_unring(dev, &fence->ring);
	ret = driver->emit(dev, fence_flags, &sequence, &native_type);
	if (ret)
		return ret;

	write_lock_irqsave(&fm->lock, flags);
	fence->type = type;
	fence->flush_mask = 0x00;
	fence->submitted_flush = 0x00;
	fence->signaled = 0x00;
	fence->sequence = sequence;
	fence->native_type = native_type;
	list_add_tail(&fence->ring, &fm->ring);
	write_unlock_irqrestore(&fm->lock, flags);
	return 0;
}

static int drm_fence_object_init(drm_device_t * dev, uint32_t type, 
				 uint32_t fence_flags,
				 drm_fence_object_t * fence)
{
	int ret = 0;
	unsigned long flags;
	drm_fence_manager_t *fm = &dev->fm;

	mutex_lock(&dev->struct_mutex);
	atomic_set(&fence->usage, 1);
	mutex_unlock(&dev->struct_mutex);

	write_lock_irqsave(&fm->lock, flags);
	INIT_LIST_HEAD(&fence->ring);
	fence->class = 0;
	fence->type = type;
	fence->flush_mask = 0;
	fence->submitted_flush = 0;
	fence->signaled = 0;
	fence->sequence = 0;
	write_unlock_irqrestore(&fm->lock, flags);
	if (fence_flags & DRM_FENCE_FLAG_EMIT) {
		ret = drm_fence_object_emit(dev, fence, fence_flags, type);
	}
	return ret;
}


int drm_fence_add_user_object(drm_file_t * priv, drm_fence_object_t * fence,
			      int shareable)
{
	drm_device_t *dev = priv->head->dev;
	int ret;

	mutex_lock(&dev->struct_mutex);
	ret = drm_add_user_object(priv, &fence->base, shareable);
	mutex_unlock(&dev->struct_mutex);
	if (ret)
		return ret;
	fence->base.type = drm_fence_type;
	fence->base.remove = &drm_fence_object_destroy;
	DRM_DEBUG("Fence 0x%08lx created\n", fence->base.hash.key);
	return 0;
}

EXPORT_SYMBOL(drm_fence_add_user_object);

int drm_fence_object_create(drm_device_t * dev, uint32_t type,
			    unsigned flags, drm_fence_object_t ** c_fence)
{
	drm_fence_object_t *fence;
	int ret;

	fence = kmem_cache_alloc(drm_cache.fence_object, GFP_KERNEL);
	if (!fence)
		return -ENOMEM;
	ret = drm_fence_object_init(dev, type, flags, fence);
	if (ret) {
		drm_fence_usage_deref_unlocked(dev, fence);
		return ret;
	}
	*c_fence = fence;
	return 0;
}

EXPORT_SYMBOL(drm_fence_object_create);

void drm_fence_manager_init(drm_device_t * dev)
{
	drm_fence_manager_t *fm = &dev->fm;
	drm_fence_driver_t *fed = dev->driver->fence_driver;
	int i;

	fm->lock = RW_LOCK_UNLOCKED;
	INIT_LIST_HEAD(&fm->ring);
	fm->pending_flush = 0;
	DRM_INIT_WAITQUEUE(&fm->fence_queue);
	fm->initialized = 0;
	if (fed) {
		fm->initialized = 1;
		for (i = 0; i < fed->no_types; ++i) {
			fm->fence_types[i] = &fm->ring;
		}
	}
}

void drm_fence_manager_takedown(drm_device_t * dev)
{
}

drm_fence_object_t *drm_lookup_fence_object(drm_file_t * priv, uint32_t handle)
{
	drm_device_t *dev = priv->head->dev;
	drm_user_object_t *uo;
	drm_fence_object_t *fence;

	mutex_lock(&dev->struct_mutex);
	uo = drm_lookup_user_object(priv, handle);
	if (!uo || (uo->type != drm_fence_type)) {
		mutex_unlock(&dev->struct_mutex);
		return NULL;
	}
	fence = drm_user_object_entry(uo, drm_fence_object_t, base);
	atomic_inc(&fence->usage);
	mutex_unlock(&dev->struct_mutex);
	return fence;
}

int drm_fence_ioctl(DRM_IOCTL_ARGS)
{
	DRM_DEVICE;
	int ret;
	drm_fence_manager_t *fm = &dev->fm;
	drm_fence_arg_t arg;
	drm_fence_object_t *fence;
	drm_user_object_t *uo;
	unsigned long flags;
	ret = 0;

	if (!fm->initialized) {
		DRM_ERROR("The DRM driver does not support fencing.\n");
		return -EINVAL;
	}

	DRM_COPY_FROM_USER_IOCTL(arg, (void __user *)data, sizeof(arg));
	switch (arg.op) {
	case drm_fence_create:
		if (arg.flags & DRM_FENCE_FLAG_EMIT)
			LOCK_TEST_WITH_RETURN(dev, filp);
		ret = drm_fence_object_create(dev, arg.type,
					      arg.flags,
					      &fence);
		if (ret)
			return ret;
		ret = drm_fence_add_user_object(priv, fence,
						arg.flags &
						DRM_FENCE_FLAG_SHAREABLE);
		if (ret) {
			drm_fence_usage_deref_unlocked(dev, fence);
			return ret;
		}

		/*
		 * usage > 0. No need to lock dev->struct_mutex;
		 */

		atomic_inc(&fence->usage);
		arg.handle = fence->base.hash.key;
		break;
	case drm_fence_destroy:
		mutex_lock(&dev->struct_mutex);
		uo = drm_lookup_user_object(priv, arg.handle);
		if (!uo || (uo->type != drm_fence_type) || uo->owner != priv) {
			mutex_unlock(&dev->struct_mutex);
			return -EINVAL;
		}
		ret = drm_remove_user_object(priv, uo);
		mutex_unlock(&dev->struct_mutex);
		return ret;
	case drm_fence_reference:
		ret =
		    drm_user_object_ref(priv, arg.handle, drm_fence_type, &uo);
		if (ret)
			return ret;
		fence = drm_lookup_fence_object(priv, arg.handle);
		break;
	case drm_fence_unreference:
		ret = drm_user_object_unref(priv, arg.handle, drm_fence_type);
		return ret;
	case drm_fence_signaled:
		fence = drm_lookup_fence_object(priv, arg.handle);
		if (!fence)
			return -EINVAL;
		break;
	case drm_fence_flush:
		fence = drm_lookup_fence_object(priv, arg.handle);
		if (!fence)
			return -EINVAL;
		ret = drm_fence_object_flush(dev, fence, arg.type);
		break;
	case drm_fence_wait:
		fence = drm_lookup_fence_object(priv, arg.handle);
		if (!fence)
			return -EINVAL;
		ret =
		    drm_fence_object_wait(dev, fence,
					  arg.flags & DRM_FENCE_FLAG_WAIT_LAZY,
					  0, arg.type);
		break;
	case drm_fence_emit:
		LOCK_TEST_WITH_RETURN(dev, filp);
		fence = drm_lookup_fence_object(priv, arg.handle);
		if (!fence)
			return -EINVAL;
		ret = drm_fence_object_emit(dev, fence, arg.flags, arg.type);
		break;
	case drm_fence_buffers:
		if (!dev->bm.initialized) {
			DRM_ERROR("Buffer object manager is not initialized\n");
			return -EINVAL;
		}
		LOCK_TEST_WITH_RETURN(dev, filp);
		ret = drm_fence_buffer_objects(priv, NULL, arg.flags, 
					       NULL, &fence);
		if (ret)
			return ret;
		ret = drm_fence_add_user_object(priv, fence,
						arg.flags &
						DRM_FENCE_FLAG_SHAREABLE);
		if (ret)
			return ret;
		atomic_inc(&fence->usage);
		arg.handle = fence->base.hash.key;
		break;
	default:
		return -EINVAL;
	}
	read_lock_irqsave(&fm->lock, flags);
	arg.class = fence->class;
	arg.type = fence->type;
	arg.signaled = fence->signaled;
	read_unlock_irqrestore(&fm->lock, flags);
	drm_fence_usage_deref_unlocked(dev, fence);

	DRM_COPY_TO_USER_IOCTL((void __user *)data, arg, sizeof(arg));
	return ret;
}
