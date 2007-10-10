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
 * Typically called by the IRQ handler.
 */

void drm_fence_handler(struct drm_device * dev, uint32_t fence_class,
		       uint32_t sequence, uint32_t type, uint32_t error)
{
	int wake = 0;
	uint32_t diff;
	uint32_t relevant;
	struct drm_fence_manager *fm = &dev->fm;
	struct drm_fence_class_manager *fc = &fm->fence_class[fence_class];
	struct drm_fence_driver *driver = dev->driver->fence_driver;
	struct list_head *head;
	struct drm_fence_object *fence, *next;
	int found = 0;
	int is_exe = (type & DRM_FENCE_TYPE_EXE);
	int ge_last_exe;


	diff = (sequence - fc->exe_flush_sequence) & driver->sequence_mask;

	if (fc->pending_exe_flush && is_exe && diff < driver->wrap_diff)
		fc->pending_exe_flush = 0;

	diff = (sequence - fc->last_exe_flush) & driver->sequence_mask;
	ge_last_exe = diff < driver->wrap_diff;

	if (is_exe && ge_last_exe) {
		fc->last_exe_flush = sequence;
	}

	if (list_empty(&fc->ring))
		return;

	list_for_each_entry(fence, &fc->ring, ring) {
		diff = (sequence - fence->sequence) & driver->sequence_mask;
		if (diff > driver->wrap_diff) {
			found = 1;
			break;
		}
	}

	fc->pending_flush &= ~type;
	head = (found) ? &fence->ring : &fc->ring;

	list_for_each_entry_safe_reverse(fence, next, head, ring) {
		if (&fence->ring == &fc->ring)
			break;

		if (error) {
			fence->error = error;
			fence->signaled = fence->type;
			fence->submitted_flush = fence->type;
			fence->flush_mask = fence->type;
			list_del_init(&fence->ring);
			wake = 1;
			break;
		}

		if (is_exe)
			type |= fence->native_type;

		relevant = type & fence->type;

		if ((fence->signaled | relevant) != fence->signaled) {
			fence->signaled |= relevant;
			fence->flush_mask |= relevant;
			fence->submitted_flush |= relevant;
			DRM_DEBUG("Fence 0x%08lx signaled 0x%08x\n",
				  fence->base.hash.key, fence->signaled);
			wake = 1;
		}

		relevant = fence->flush_mask &
			~(fence->submitted_flush | fence->signaled);

		fc->pending_flush |= relevant;
		fence->submitted_flush |= relevant;

		if (!(fence->type & ~fence->signaled)) {
			DRM_DEBUG("Fence completely signaled 0x%08lx\n",
				  fence->base.hash.key);
			list_del_init(&fence->ring);
		}

	}

	/*
	 * Reinstate lost flush flags.
	 */

	if ((fc->pending_flush & type) != type) {
	        head = head->prev;
		list_for_each_entry(fence, head, ring) {
			if (&fence->ring == &fc->ring)
				break;
	    		diff = (fc->last_exe_flush - fence->sequence) &
				driver->sequence_mask;
			if (diff > driver->wrap_diff)
				break;

			relevant = fence->submitted_flush & ~fence->signaled;
			fc->pending_flush |= relevant;
		}
	}

	if (wake) {
		DRM_WAKEUP(&fc->fence_queue);
	}
}

EXPORT_SYMBOL(drm_fence_handler);

static void drm_fence_unring(struct drm_device * dev, struct list_head *ring)
{
	struct drm_fence_manager *fm = &dev->fm;
	unsigned long flags;

	write_lock_irqsave(&fm->lock, flags);
	list_del_init(ring);
	write_unlock_irqrestore(&fm->lock, flags);
}

void drm_fence_usage_deref_locked(struct drm_fence_object ** fence)
{
	struct drm_fence_object *tmp_fence = *fence;
	struct drm_device *dev = tmp_fence->dev;
	struct drm_fence_manager *fm = &dev->fm;

	DRM_ASSERT_LOCKED(&dev->struct_mutex);
	*fence = NULL;
	if (atomic_dec_and_test(&tmp_fence->usage)) {
		drm_fence_unring(dev, &tmp_fence->ring);
		DRM_DEBUG("Destroyed a fence object 0x%08lx\n",
			  tmp_fence->base.hash.key);
		atomic_dec(&fm->count);
		BUG_ON(!list_empty(&tmp_fence->base.list));
		drm_ctl_free(tmp_fence, sizeof(*tmp_fence), DRM_MEM_FENCE);
	}
}
EXPORT_SYMBOL(drm_fence_usage_deref_locked);

void drm_fence_usage_deref_unlocked(struct drm_fence_object ** fence)
{
	struct drm_fence_object *tmp_fence = *fence;
	struct drm_device *dev = tmp_fence->dev;
	struct drm_fence_manager *fm = &dev->fm;

	*fence = NULL;
	if (atomic_dec_and_test(&tmp_fence->usage)) {
		mutex_lock(&dev->struct_mutex);
		if (atomic_read(&tmp_fence->usage) == 0) {
			drm_fence_unring(dev, &tmp_fence->ring);
			atomic_dec(&fm->count);
			BUG_ON(!list_empty(&tmp_fence->base.list));
			drm_ctl_free(tmp_fence, sizeof(*tmp_fence), DRM_MEM_FENCE);
		}
		mutex_unlock(&dev->struct_mutex);
	}
}
EXPORT_SYMBOL(drm_fence_usage_deref_unlocked);

struct drm_fence_object
*drm_fence_reference_locked(struct drm_fence_object *src)
{
	DRM_ASSERT_LOCKED(&src->dev->struct_mutex);

	atomic_inc(&src->usage);
	return src;
}

void drm_fence_reference_unlocked(struct drm_fence_object **dst,
				  struct drm_fence_object *src)
{
	mutex_lock(&src->dev->struct_mutex);
	*dst = src;
	atomic_inc(&src->usage);
	mutex_unlock(&src->dev->struct_mutex);
}
EXPORT_SYMBOL(drm_fence_reference_unlocked);

static void drm_fence_object_destroy(struct drm_file *priv, struct drm_user_object * base)
{
	struct drm_fence_object *fence =
	    drm_user_object_entry(base, struct drm_fence_object, base);

	drm_fence_usage_deref_locked(&fence);
}

int drm_fence_object_signaled(struct drm_fence_object * fence,
			      uint32_t mask, int poke_flush)
{
	unsigned long flags;
	int signaled;
	struct drm_device *dev = fence->dev;
	struct drm_fence_manager *fm = &dev->fm;
	struct drm_fence_driver *driver = dev->driver->fence_driver;

	if (poke_flush)
		driver->poke_flush(dev, fence->fence_class);
	read_lock_irqsave(&fm->lock, flags);
	signaled =
	    (fence->type & mask & fence->signaled) == (fence->type & mask);
	read_unlock_irqrestore(&fm->lock, flags);

	return signaled;
}
EXPORT_SYMBOL(drm_fence_object_signaled);

static void drm_fence_flush_exe(struct drm_fence_class_manager * fc,
				struct drm_fence_driver * driver, uint32_t sequence)
{
	uint32_t diff;

	if (!fc->pending_exe_flush) {
		fc->exe_flush_sequence = sequence;
		fc->pending_exe_flush = 1;
	} else {
		diff =
		    (sequence - fc->exe_flush_sequence) & driver->sequence_mask;
		if (diff < driver->wrap_diff) {
			fc->exe_flush_sequence = sequence;
		}
	}
}

int drm_fence_object_flush(struct drm_fence_object * fence,
			   uint32_t type)
{
	struct drm_device *dev = fence->dev;
	struct drm_fence_manager *fm = &dev->fm;
	struct drm_fence_class_manager *fc = &fm->fence_class[fence->fence_class];
	struct drm_fence_driver *driver = dev->driver->fence_driver;
	unsigned long flags;

	if (type & ~fence->type) {
		DRM_ERROR("Flush trying to extend fence type, "
			  "0x%x, 0x%x\n", type, fence->type);
		return -EINVAL;
	}

	write_lock_irqsave(&fm->lock, flags);
	fence->flush_mask |= type;
	if ((fence->submitted_flush & fence->signaled)
	    == fence->submitted_flush) {
		if ((fence->type & DRM_FENCE_TYPE_EXE) &&
		    !(fence->submitted_flush & DRM_FENCE_TYPE_EXE)) {
			drm_fence_flush_exe(fc, driver, fence->sequence);
			fence->submitted_flush |= DRM_FENCE_TYPE_EXE;
		} else {
			fc->pending_flush |= (fence->flush_mask &
					      ~fence->submitted_flush);
			fence->submitted_flush = fence->flush_mask;
		}
	}
	write_unlock_irqrestore(&fm->lock, flags);
	driver->poke_flush(dev, fence->fence_class);
	return 0;
}

/*
 * Make sure old fence objects are signaled before their fence sequences are
 * wrapped around and reused.
 */

void drm_fence_flush_old(struct drm_device * dev, uint32_t fence_class, uint32_t sequence)
{
	struct drm_fence_manager *fm = &dev->fm;
	struct drm_fence_class_manager *fc = &fm->fence_class[fence_class];
	struct drm_fence_driver *driver = dev->driver->fence_driver;
	uint32_t old_sequence;
	unsigned long flags;
	struct drm_fence_object *fence;
	uint32_t diff;

	write_lock_irqsave(&fm->lock, flags);
	old_sequence = (sequence - driver->flush_diff) & driver->sequence_mask;
	diff = (old_sequence - fc->last_exe_flush) & driver->sequence_mask;

	if ((diff < driver->wrap_diff) && !fc->pending_exe_flush) {
		fc->pending_exe_flush = 1;
		fc->exe_flush_sequence = sequence - (driver->flush_diff / 2);
	}
	write_unlock_irqrestore(&fm->lock, flags);

	mutex_lock(&dev->struct_mutex);
	read_lock_irqsave(&fm->lock, flags);

	if (list_empty(&fc->ring)) {
		read_unlock_irqrestore(&fm->lock, flags);
		mutex_unlock(&dev->struct_mutex);
		return;
	}
	fence = drm_fence_reference_locked(list_entry(fc->ring.next, struct drm_fence_object, ring));
	mutex_unlock(&dev->struct_mutex);
	diff = (old_sequence - fence->sequence) & driver->sequence_mask;
	read_unlock_irqrestore(&fm->lock, flags);
	if (diff < driver->wrap_diff) {
		drm_fence_object_flush(fence, fence->type);
	}
	drm_fence_usage_deref_unlocked(&fence);
}

EXPORT_SYMBOL(drm_fence_flush_old);

static int drm_fence_lazy_wait(struct drm_fence_object *fence,
			       int ignore_signals,
			       uint32_t mask)
{
	struct drm_device *dev = fence->dev;
	struct drm_fence_manager *fm = &dev->fm;
	struct drm_fence_class_manager *fc = &fm->fence_class[fence->fence_class];
	int signaled;
	unsigned long _end = jiffies + 3*DRM_HZ;
	int ret = 0;

	do {
		DRM_WAIT_ON(ret, fc->fence_queue, 3 * DRM_HZ,
			    (signaled = drm_fence_object_signaled(fence, mask, 1)));
		if (signaled)
			return 0;
		if (time_after_eq(jiffies, _end))
			break;
	} while (ret == -EINTR && ignore_signals);
	if (drm_fence_object_signaled(fence, mask, 0))
		return 0;
	if (time_after_eq(jiffies, _end))
		ret = -EBUSY;
	if (ret) {
		if (ret == -EBUSY) {
			DRM_ERROR("Fence timeout. "
				  "GPU lockup or fence driver was "
				  "taken down. %d 0x%08x 0x%02x 0x%02x 0x%02x\n",
				  fence->fence_class,
				  fence->sequence,
				  fence->type,
				  mask,
				  fence->signaled);
			DRM_ERROR("Pending exe flush %d 0x%08x\n",
				  fc->pending_exe_flush,
				  fc->exe_flush_sequence);
		}
		return ((ret == -EINTR) ? -EAGAIN : ret);
	}
	return 0;
}

int drm_fence_object_wait(struct drm_fence_object * fence,
			  int lazy, int ignore_signals, uint32_t mask)
{
	struct drm_device *dev = fence->dev;
	struct drm_fence_driver *driver = dev->driver->fence_driver;
	int ret = 0;
	unsigned long _end;
	int signaled;

	if (mask & ~fence->type) {
		DRM_ERROR("Wait trying to extend fence type"
			  " 0x%08x 0x%08x\n", mask, fence->type);
		BUG();
		return -EINVAL;
	}

	if (drm_fence_object_signaled(fence, mask, 0))
		return 0;

	_end = jiffies + 3 * DRM_HZ;

	drm_fence_object_flush(fence, mask);

	if (lazy && driver->lazy_capable) {

		ret = drm_fence_lazy_wait(fence, ignore_signals, mask);
		if (ret)
			return ret;

	} else {

		if (driver->has_irq(dev, fence->fence_class,
				    DRM_FENCE_TYPE_EXE)) {
			ret = drm_fence_lazy_wait(fence, ignore_signals,
						  DRM_FENCE_TYPE_EXE);
			if (ret)
				return ret;
		}

		if (driver->has_irq(dev, fence->fence_class,
				    mask & ~DRM_FENCE_TYPE_EXE)) {
			ret = drm_fence_lazy_wait(fence, ignore_signals,
						  mask);
			if (ret)
				return ret;
		}
	}
	if (drm_fence_object_signaled(fence, mask, 0))
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
		signaled = drm_fence_object_signaled(fence, mask, 1);
	} while (!signaled && !time_after_eq(jiffies, _end));

	if (!signaled)
		return -EBUSY;

	return 0;
}
EXPORT_SYMBOL(drm_fence_object_wait);


int drm_fence_object_emit(struct drm_fence_object * fence,
			  uint32_t fence_flags, uint32_t fence_class, uint32_t type)
{
	struct drm_device *dev = fence->dev;
	struct drm_fence_manager *fm = &dev->fm;
	struct drm_fence_driver *driver = dev->driver->fence_driver;
	struct drm_fence_class_manager *fc = &fm->fence_class[fence->fence_class];
	unsigned long flags;
	uint32_t sequence;
	uint32_t native_type;
	int ret;

	drm_fence_unring(dev, &fence->ring);
	ret = driver->emit(dev, fence_class, fence_flags, &sequence, &native_type);
	if (ret)
		return ret;

	write_lock_irqsave(&fm->lock, flags);
	fence->fence_class = fence_class;
	fence->type = type;
	fence->flush_mask = 0x00;
	fence->submitted_flush = 0x00;
	fence->signaled = 0x00;
	fence->sequence = sequence;
	fence->native_type = native_type;
	if (list_empty(&fc->ring))
		fc->last_exe_flush = sequence - 1;
	list_add_tail(&fence->ring, &fc->ring);
	write_unlock_irqrestore(&fm->lock, flags);
	return 0;
}
EXPORT_SYMBOL(drm_fence_object_emit);

static int drm_fence_object_init(struct drm_device * dev, uint32_t fence_class,
				 uint32_t type,
				 uint32_t fence_flags,
				 struct drm_fence_object * fence)
{
	int ret = 0;
	unsigned long flags;
	struct drm_fence_manager *fm = &dev->fm;

	mutex_lock(&dev->struct_mutex);
	atomic_set(&fence->usage, 1);
	mutex_unlock(&dev->struct_mutex);

	write_lock_irqsave(&fm->lock, flags);
	INIT_LIST_HEAD(&fence->ring);

	/* 
	 *  Avoid hitting BUG() for kernel-only fence objects.
	 */

	INIT_LIST_HEAD(&fence->base.list);
	fence->fence_class = fence_class;
	fence->type = type;
	fence->flush_mask = 0;
	fence->submitted_flush = 0;
	fence->signaled = 0;
	fence->sequence = 0;
	fence->dev = dev;
	write_unlock_irqrestore(&fm->lock, flags);
	if (fence_flags & DRM_FENCE_FLAG_EMIT) {
		ret = drm_fence_object_emit(fence, fence_flags,
					    fence->fence_class, type);
	}
	return ret;
}

int drm_fence_add_user_object(struct drm_file * priv, struct drm_fence_object * fence,
			      int shareable)
{
	struct drm_device *dev = priv->head->dev;
	int ret;

	mutex_lock(&dev->struct_mutex);
	ret = drm_add_user_object(priv, &fence->base, shareable);
	if (ret)
		goto out;
	atomic_inc(&fence->usage);
	fence->base.type = drm_fence_type;
	fence->base.remove = &drm_fence_object_destroy;
	DRM_DEBUG("Fence 0x%08lx created\n", fence->base.hash.key);
out:
	mutex_unlock(&dev->struct_mutex);
	return ret;
}
EXPORT_SYMBOL(drm_fence_add_user_object);

int drm_fence_object_create(struct drm_device * dev, uint32_t fence_class, uint32_t type,
			    unsigned flags, struct drm_fence_object ** c_fence)
{
	struct drm_fence_object *fence;
	int ret;
	struct drm_fence_manager *fm = &dev->fm;

	fence = drm_ctl_calloc(1, sizeof(*fence), DRM_MEM_FENCE);
	if (!fence)
		return -ENOMEM;
	ret = drm_fence_object_init(dev, fence_class, type, flags, fence);
	if (ret) {
		drm_fence_usage_deref_unlocked(&fence);
		return ret;
	}
	*c_fence = fence;
	atomic_inc(&fm->count);

	return 0;
}

EXPORT_SYMBOL(drm_fence_object_create);

void drm_fence_manager_init(struct drm_device * dev)
{
	struct drm_fence_manager *fm = &dev->fm;
	struct drm_fence_class_manager *fence_class;
	struct drm_fence_driver *fed = dev->driver->fence_driver;
	int i;
	unsigned long flags;

	rwlock_init(&fm->lock);
	write_lock_irqsave(&fm->lock, flags);
	fm->initialized = 0;
	if (!fed)
	    goto out_unlock;

	fm->initialized = 1;
	fm->num_classes = fed->num_classes;
	BUG_ON(fm->num_classes > _DRM_FENCE_CLASSES);

	for (i=0; i<fm->num_classes; ++i) {
	    fence_class = &fm->fence_class[i];

	    INIT_LIST_HEAD(&fence_class->ring);
	    fence_class->pending_flush = 0;
	    DRM_INIT_WAITQUEUE(&fence_class->fence_queue);
	}

	atomic_set(&fm->count, 0);
 out_unlock:
	write_unlock_irqrestore(&fm->lock, flags);
}

void drm_fence_fill_arg(struct drm_fence_object *fence, struct drm_fence_arg *arg)
{
	struct drm_device *dev = fence->dev;
	struct drm_fence_manager *fm = &dev->fm;
	unsigned long irq_flags;

	read_lock_irqsave(&fm->lock, irq_flags);
	arg->handle = fence->base.hash.key;
	arg->fence_class = fence->fence_class;
	arg->type = fence->type;
	arg->signaled = fence->signaled;
	arg->error = fence->error;
	arg->sequence = fence->sequence;
	read_unlock_irqrestore(&fm->lock, irq_flags);
}
EXPORT_SYMBOL(drm_fence_fill_arg);


void drm_fence_manager_takedown(struct drm_device * dev)
{
}

struct drm_fence_object *drm_lookup_fence_object(struct drm_file * priv, uint32_t handle)
{
	struct drm_device *dev = priv->head->dev;
	struct drm_user_object *uo;
	struct drm_fence_object *fence;

	mutex_lock(&dev->struct_mutex);
	uo = drm_lookup_user_object(priv, handle);
	if (!uo || (uo->type != drm_fence_type)) {
		mutex_unlock(&dev->struct_mutex);
		return NULL;
	}
	fence = drm_fence_reference_locked(drm_user_object_entry(uo, struct drm_fence_object, base));
	mutex_unlock(&dev->struct_mutex);
	return fence;
}

int drm_fence_create_ioctl(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	int ret;
	struct drm_fence_manager *fm = &dev->fm;
	struct drm_fence_arg *arg = data;
	struct drm_fence_object *fence;
	ret = 0;

	if (!fm->initialized) {
		DRM_ERROR("The DRM driver does not support fencing.\n");
		return -EINVAL;
	}

	if (arg->flags & DRM_FENCE_FLAG_EMIT)
		LOCK_TEST_WITH_RETURN(dev, file_priv);
	ret = drm_fence_object_create(dev, arg->fence_class,
				      arg->type, arg->flags, &fence);
	if (ret)
		return ret;
	ret = drm_fence_add_user_object(file_priv, fence,
					arg->flags &
					DRM_FENCE_FLAG_SHAREABLE);
	if (ret) {
		drm_fence_usage_deref_unlocked(&fence);
		return ret;
	}
	
	/*
	 * usage > 0. No need to lock dev->struct_mutex;
	 */

	arg->handle = fence->base.hash.key;


	drm_fence_fill_arg(fence, arg);
	drm_fence_usage_deref_unlocked(&fence);

	return ret;
}

int drm_fence_destroy_ioctl(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	int ret;
	struct drm_fence_manager *fm = &dev->fm;
	struct drm_fence_arg *arg = data;
	struct drm_user_object *uo;
	ret = 0;

	if (!fm->initialized) {
		DRM_ERROR("The DRM driver does not support fencing.\n");
		return -EINVAL;
	}

	mutex_lock(&dev->struct_mutex);
	uo = drm_lookup_user_object(file_priv, arg->handle);
	if (!uo || (uo->type != drm_fence_type) || uo->owner != file_priv) {
		mutex_unlock(&dev->struct_mutex);
		return -EINVAL;
	}
	ret = drm_remove_user_object(file_priv, uo);
	mutex_unlock(&dev->struct_mutex);
	return ret;
}


int drm_fence_reference_ioctl(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	int ret;
	struct drm_fence_manager *fm = &dev->fm;
	struct drm_fence_arg *arg = data;
	struct drm_fence_object *fence;
	struct drm_user_object *uo;
	ret = 0;

	if (!fm->initialized) {
		DRM_ERROR("The DRM driver does not support fencing.\n");
		return -EINVAL;
	}

	ret = drm_user_object_ref(file_priv, arg->handle, drm_fence_type, &uo);
	if (ret)
		return ret;
	fence = drm_lookup_fence_object(file_priv, arg->handle);
	drm_fence_fill_arg(fence, arg);
	drm_fence_usage_deref_unlocked(&fence);

	return ret;
}


int drm_fence_unreference_ioctl(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	int ret;
	struct drm_fence_manager *fm = &dev->fm;
	struct drm_fence_arg *arg = data;
	ret = 0;

	if (!fm->initialized) {
		DRM_ERROR("The DRM driver does not support fencing.\n");
		return -EINVAL;
	}

	return drm_user_object_unref(file_priv, arg->handle, drm_fence_type);
}

int drm_fence_signaled_ioctl(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	int ret;
	struct drm_fence_manager *fm = &dev->fm;
	struct drm_fence_arg *arg = data;
	struct drm_fence_object *fence;
	ret = 0;

	if (!fm->initialized) {
		DRM_ERROR("The DRM driver does not support fencing.\n");
		return -EINVAL;
	}

	fence = drm_lookup_fence_object(file_priv, arg->handle);
	if (!fence)
		return -EINVAL;

	drm_fence_fill_arg(fence, arg);
	drm_fence_usage_deref_unlocked(&fence);

	return ret;
}

int drm_fence_flush_ioctl(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	int ret;
	struct drm_fence_manager *fm = &dev->fm;
	struct drm_fence_arg *arg = data;
	struct drm_fence_object *fence;
	ret = 0;

	if (!fm->initialized) {
		DRM_ERROR("The DRM driver does not support fencing.\n");
		return -EINVAL;
	}

	fence = drm_lookup_fence_object(file_priv, arg->handle);
	if (!fence)
		return -EINVAL;
	ret = drm_fence_object_flush(fence, arg->type);

	drm_fence_fill_arg(fence, arg);
	drm_fence_usage_deref_unlocked(&fence);

	return ret;
}


int drm_fence_wait_ioctl(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	int ret;
	struct drm_fence_manager *fm = &dev->fm;
	struct drm_fence_arg *arg = data;
	struct drm_fence_object *fence;
	ret = 0;

	if (!fm->initialized) {
		DRM_ERROR("The DRM driver does not support fencing.\n");
		return -EINVAL;
	}

	fence = drm_lookup_fence_object(file_priv, arg->handle);
	if (!fence)
		return -EINVAL;
	ret = drm_fence_object_wait(fence,
				    arg->flags & DRM_FENCE_FLAG_WAIT_LAZY,
				    0, arg->type);

	drm_fence_fill_arg(fence, arg);
	drm_fence_usage_deref_unlocked(&fence);

	return ret;
}


int drm_fence_emit_ioctl(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	int ret;
	struct drm_fence_manager *fm = &dev->fm;
	struct drm_fence_arg *arg = data;
	struct drm_fence_object *fence;
	ret = 0;

	if (!fm->initialized) {
		DRM_ERROR("The DRM driver does not support fencing.\n");
		return -EINVAL;
	}

	LOCK_TEST_WITH_RETURN(dev, file_priv);
	fence = drm_lookup_fence_object(file_priv, arg->handle);
	if (!fence)
		return -EINVAL;
	ret = drm_fence_object_emit(fence, arg->flags, arg->fence_class,
				    arg->type);

	drm_fence_fill_arg(fence, arg);
	drm_fence_usage_deref_unlocked(&fence);

	return ret;
}

int drm_fence_buffers_ioctl(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	int ret;
	struct drm_fence_manager *fm = &dev->fm;
	struct drm_fence_arg *arg = data;
	struct drm_fence_object *fence;
	ret = 0;

	if (!fm->initialized) {
		DRM_ERROR("The DRM driver does not support fencing.\n");
		return -EINVAL;
	}

	if (!dev->bm.initialized) {
		DRM_ERROR("Buffer object manager is not initialized\n");
		return -EINVAL;
	}
	LOCK_TEST_WITH_RETURN(dev, file_priv);
	ret = drm_fence_buffer_objects(dev, NULL, arg->flags,
				       NULL, &fence);
	if (ret)
		return ret;

	if (!(arg->flags & DRM_FENCE_FLAG_NO_USER)) {
		ret = drm_fence_add_user_object(file_priv, fence,
						arg->flags &
						DRM_FENCE_FLAG_SHAREABLE);
		if (ret)
			return ret;
	}

	arg->handle = fence->base.hash.key;

	drm_fence_fill_arg(fence, arg);
	drm_fence_usage_deref_unlocked(&fence);

	return ret;
}
