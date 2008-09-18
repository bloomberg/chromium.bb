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
 * Convenience function to be called by fence::wait methods that
 * need polling.
 */

int drm_fence_wait_polling(struct drm_fence_object *fence, int lazy,
			   int interruptible, uint32_t mask, 
			   unsigned long end_jiffies)
{
	struct drm_device *dev = fence->dev;
	struct drm_fence_manager *fm = &dev->fm;
	struct drm_fence_class_manager *fc = &fm->fence_class[fence->fence_class];
	uint32_t count = 0;
	int ret;

	DECLARE_WAITQUEUE(entry, current);
	add_wait_queue(&fc->fence_queue, &entry);

	ret = 0;
	
	for (;;) {
		__set_current_state((interruptible) ? 
				    TASK_INTERRUPTIBLE :
				    TASK_UNINTERRUPTIBLE);
		if (drm_fence_object_signaled(fence, mask))
			break;
		if (time_after_eq(jiffies, end_jiffies)) {
			ret = -EBUSY;
			break;
		}
		if (lazy)
			schedule_timeout(1);
		else if ((++count & 0x0F) == 0){
			__set_current_state(TASK_RUNNING);
			schedule();
			__set_current_state((interruptible) ? 
					    TASK_INTERRUPTIBLE :
					    TASK_UNINTERRUPTIBLE);
		}			
		if (interruptible && signal_pending(current)) {
			ret = -EAGAIN;
			break;
		}
	}
	__set_current_state(TASK_RUNNING);
	remove_wait_queue(&fc->fence_queue, &entry);
	return ret;
}
EXPORT_SYMBOL(drm_fence_wait_polling);

/*
 * Typically called by the IRQ handler.
 */

void drm_fence_handler(struct drm_device *dev, uint32_t fence_class,
		       uint32_t sequence, uint32_t type, uint32_t error)
{
	int wake = 0;
	uint32_t diff;
	uint32_t relevant_type;
	uint32_t new_type;
	struct drm_fence_manager *fm = &dev->fm;
	struct drm_fence_class_manager *fc = &fm->fence_class[fence_class];
	struct drm_fence_driver *driver = dev->driver->fence_driver;
	struct list_head *head;
	struct drm_fence_object *fence, *next;
	int found = 0;

	if (list_empty(&fc->ring))
		return;

	list_for_each_entry(fence, &fc->ring, ring) {
		diff = (sequence - fence->sequence) & driver->sequence_mask;
		if (diff > driver->wrap_diff) {
			found = 1;
			break;
		}
	}

	fc->waiting_types &= ~type;
	head = (found) ? &fence->ring : &fc->ring;

	list_for_each_entry_safe_reverse(fence, next, head, ring) {
		if (&fence->ring == &fc->ring)
			break;

		if (error) {
			fence->error = error;
			fence->signaled_types = fence->type;
			list_del_init(&fence->ring);
			wake = 1;
			break;
		}

		if (type & DRM_FENCE_TYPE_EXE)
			type |= fence->native_types;

		relevant_type = type & fence->type;
		new_type = (fence->signaled_types | relevant_type) ^
			fence->signaled_types;

		if (new_type) {
			fence->signaled_types |= new_type;
			DRM_DEBUG("Fence %p signaled 0x%08x\n",
				  fence, fence->signaled_types);

			if (driver->needed_flush)
				fc->pending_flush |= driver->needed_flush(fence);

			if (new_type & fence->waiting_types)
				wake = 1;
		}

		fc->waiting_types |= fence->waiting_types & ~fence->signaled_types;

		if (!(fence->type & ~fence->signaled_types)) {
			DRM_DEBUG("Fence completely signaled %p\n",
				  fence);
			list_del_init(&fence->ring);
		}
	}

	/*
	 * Reinstate lost waiting types.
	 */

	if ((fc->waiting_types & type) != type) {
		head = head->prev;
		list_for_each_entry(fence, head, ring) {
			if (&fence->ring == &fc->ring)
				break;
			diff = (fc->highest_waiting_sequence - fence->sequence) &
				driver->sequence_mask;
			if (diff > driver->wrap_diff)
				break;
			
			fc->waiting_types |= fence->waiting_types & ~fence->signaled_types;
		}
	}

	if (wake) 
		wake_up_all(&fc->fence_queue);
}
EXPORT_SYMBOL(drm_fence_handler);

static void drm_fence_unring(struct drm_device *dev, struct list_head *ring)
{
	struct drm_fence_manager *fm = &dev->fm;
	unsigned long flags;

	write_lock_irqsave(&fm->lock, flags);
	list_del_init(ring);
	write_unlock_irqrestore(&fm->lock, flags);
}

void drm_fence_usage_deref_locked(struct drm_fence_object **fence)
{
	struct drm_fence_object *tmp_fence = *fence;
	struct drm_device *dev = tmp_fence->dev;
	struct drm_fence_manager *fm = &dev->fm;

	DRM_ASSERT_LOCKED(&dev->struct_mutex);
	*fence = NULL;
	if (atomic_dec_and_test(&tmp_fence->usage)) {
		drm_fence_unring(dev, &tmp_fence->ring);
		DRM_DEBUG("Destroyed a fence object %p\n",
			  tmp_fence);
		atomic_dec(&fm->count);
		drm_ctl_free(tmp_fence, sizeof(*tmp_fence), DRM_MEM_FENCE);
	}
}
EXPORT_SYMBOL(drm_fence_usage_deref_locked);

void drm_fence_usage_deref_unlocked(struct drm_fence_object **fence)
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

int drm_fence_object_signaled(struct drm_fence_object *fence, uint32_t mask)
{
	unsigned long flags;
	int signaled;
	struct drm_device *dev = fence->dev;
	struct drm_fence_manager *fm = &dev->fm;
	struct drm_fence_driver *driver = dev->driver->fence_driver;
	
	mask &= fence->type;
	read_lock_irqsave(&fm->lock, flags);
	signaled = (mask & fence->signaled_types) == mask;
	read_unlock_irqrestore(&fm->lock, flags);
	if (!signaled && driver->poll) {
		write_lock_irqsave(&fm->lock, flags);
		driver->poll(dev, fence->fence_class, mask);
		signaled = (mask & fence->signaled_types) == mask;
		write_unlock_irqrestore(&fm->lock, flags);
	}
	return signaled;
}
EXPORT_SYMBOL(drm_fence_object_signaled);


int drm_fence_object_flush(struct drm_fence_object *fence,
			   uint32_t type)
{
	struct drm_device *dev = fence->dev;
	struct drm_fence_manager *fm = &dev->fm;
	struct drm_fence_class_manager *fc = &fm->fence_class[fence->fence_class];
	struct drm_fence_driver *driver = dev->driver->fence_driver;
	unsigned long irq_flags;
	uint32_t saved_pending_flush;
	uint32_t diff;
	int call_flush;

	if (type & ~fence->type) {
		DRM_ERROR("Flush trying to extend fence type, "
			  "0x%x, 0x%x\n", type, fence->type);
		return -EINVAL;
	}

	write_lock_irqsave(&fm->lock, irq_flags);
	fence->waiting_types |= type;
	fc->waiting_types |= fence->waiting_types;
	diff = (fence->sequence - fc->highest_waiting_sequence) & 
		driver->sequence_mask;

	if (diff < driver->wrap_diff)
		fc->highest_waiting_sequence = fence->sequence;

	/*
	 * fence->waiting_types has changed. Determine whether
	 * we need to initiate some kind of flush as a result of this.
	 */

	saved_pending_flush = fc->pending_flush;
	if (driver->needed_flush) 
		fc->pending_flush |= driver->needed_flush(fence);

	if (driver->poll)
		driver->poll(dev, fence->fence_class, fence->waiting_types);

	call_flush = fc->pending_flush;
	write_unlock_irqrestore(&fm->lock, irq_flags);

	if (call_flush && driver->flush)
		driver->flush(dev, fence->fence_class);

	return 0;
}
EXPORT_SYMBOL(drm_fence_object_flush);

/*
 * Make sure old fence objects are signaled before their fence sequences are
 * wrapped around and reused.
 */

void drm_fence_flush_old(struct drm_device *dev, uint32_t fence_class,
			 uint32_t sequence)
{
	struct drm_fence_manager *fm = &dev->fm;
	struct drm_fence_class_manager *fc = &fm->fence_class[fence_class];
	struct drm_fence_object *fence;
	unsigned long irq_flags;
	struct drm_fence_driver *driver = dev->driver->fence_driver;
	int call_flush;

	uint32_t diff;

	write_lock_irqsave(&fm->lock, irq_flags);

	list_for_each_entry_reverse(fence, &fc->ring, ring) {
		diff = (sequence - fence->sequence) & driver->sequence_mask;
		if (diff <= driver->flush_diff)
			break;
	
		fence->waiting_types = fence->type;
		fc->waiting_types |= fence->type;

		if (driver->needed_flush)
			fc->pending_flush |= driver->needed_flush(fence);
	}	
	
	if (driver->poll)
		driver->poll(dev, fence_class, fc->waiting_types);

	call_flush = fc->pending_flush;
	write_unlock_irqrestore(&fm->lock, irq_flags);

	if (call_flush && driver->flush)
		driver->flush(dev, fence->fence_class);

	/*
	 * FIXME: Shold we implement a wait here for really old fences?
	 */

}
EXPORT_SYMBOL(drm_fence_flush_old);

int drm_fence_object_wait(struct drm_fence_object *fence,
			  int lazy, int ignore_signals, uint32_t mask)
{
	struct drm_device *dev = fence->dev;
	struct drm_fence_driver *driver = dev->driver->fence_driver;
	struct drm_fence_manager *fm = &dev->fm;
	struct drm_fence_class_manager *fc = &fm->fence_class[fence->fence_class];
	int ret = 0;
	unsigned long _end = 3 * DRM_HZ;

	if (mask & ~fence->type) {
		DRM_ERROR("Wait trying to extend fence type"
			  " 0x%08x 0x%08x\n", mask, fence->type);
		BUG();
		return -EINVAL;
	}

	if (driver->wait)
		return driver->wait(fence, lazy, !ignore_signals, mask);

	drm_fence_object_flush(fence, mask);
	if (driver->has_irq(dev, fence->fence_class, mask)) {
		if (!ignore_signals)
			ret = wait_event_interruptible_timeout
				(fc->fence_queue, 
				 drm_fence_object_signaled(fence, mask), 
				 3 * DRM_HZ);
		else 
			ret = wait_event_timeout
				(fc->fence_queue, 
				 drm_fence_object_signaled(fence, mask), 
				 3 * DRM_HZ);

		if (unlikely(ret == -ERESTARTSYS))
			return -EAGAIN;

		if (unlikely(ret == 0))
			return -EBUSY;

		return 0;
	}

	return drm_fence_wait_polling(fence, lazy, !ignore_signals, mask,
				      _end);
}
EXPORT_SYMBOL(drm_fence_object_wait);

int drm_fence_object_emit(struct drm_fence_object *fence, uint32_t fence_flags,
			  uint32_t fence_class, uint32_t type)
{
	struct drm_device *dev = fence->dev;
	struct drm_fence_manager *fm = &dev->fm;
	struct drm_fence_driver *driver = dev->driver->fence_driver;
	struct drm_fence_class_manager *fc = &fm->fence_class[fence->fence_class];
	unsigned long flags;
	uint32_t sequence;
	uint32_t native_types;
	int ret;

	drm_fence_unring(dev, &fence->ring);
	ret = driver->emit(dev, fence_class, fence_flags, &sequence,
			   &native_types);
	if (ret)
		return ret;

	write_lock_irqsave(&fm->lock, flags);
	fence->fence_class = fence_class;
	fence->type = type;
	fence->waiting_types = 0;
	fence->signaled_types = 0;
	fence->error = 0;
	fence->sequence = sequence;
	fence->native_types = native_types;
	if (list_empty(&fc->ring))
		fc->highest_waiting_sequence = sequence - 1;
	list_add_tail(&fence->ring, &fc->ring);
	fc->latest_queued_sequence = sequence;
	write_unlock_irqrestore(&fm->lock, flags);
	return 0;
}
EXPORT_SYMBOL(drm_fence_object_emit);

static int drm_fence_object_init(struct drm_device *dev, uint32_t fence_class,
				 uint32_t type,
				 uint32_t fence_flags,
				 struct drm_fence_object *fence)
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

	fence->fence_class = fence_class;
	fence->type = type;
	fence->signaled_types = 0;
	fence->waiting_types = 0;
	fence->sequence = 0;
	fence->error = 0;
	fence->dev = dev;
	write_unlock_irqrestore(&fm->lock, flags);
	if (fence_flags & DRM_FENCE_FLAG_EMIT) {
		ret = drm_fence_object_emit(fence, fence_flags,
					    fence->fence_class, type);
	}
	return ret;
}

int drm_fence_object_create(struct drm_device *dev, uint32_t fence_class,
			    uint32_t type, unsigned flags,
			    struct drm_fence_object **c_fence)
{
	struct drm_fence_object *fence;
	int ret;
	struct drm_fence_manager *fm = &dev->fm;

	fence = drm_ctl_calloc(1, sizeof(*fence), DRM_MEM_FENCE);
	if (!fence) {
		DRM_ERROR("Out of memory creating fence object\n");
		return -ENOMEM;
	}
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

void drm_fence_manager_init(struct drm_device *dev)
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

	for (i = 0; i < fm->num_classes; ++i) {
	    fence_class = &fm->fence_class[i];

	    memset(fence_class, 0, sizeof(*fence_class));
	    INIT_LIST_HEAD(&fence_class->ring);
	    DRM_INIT_WAITQUEUE(&fence_class->fence_queue);
	}

	atomic_set(&fm->count, 0);
 out_unlock:
	write_unlock_irqrestore(&fm->lock, flags);
}

void drm_fence_manager_takedown(struct drm_device *dev)
{
}

