/**************************************************************************
 *
 * Copyright (c) 2007 Tungsten Graphics, Inc., Cedar Park, TX., USA
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

/*
 * This file implements a simple replacement for the buffer manager use
 * of the heavyweight hardware lock.
 * The lock is a read-write lock. Taking it in read mode is fast, and
 * intended for in-kernel use only.
 * Taking it in write mode is slow.
 *
 * The write mode is used only when there is a need to block all
 * user-space processes from allocating a
 * new memory area.
 * Typical use in write mode is X server VT switching, and it's allowed
 * to leave kernel space with the write lock held. If a user-space process
 * dies while having the write-lock, it will be released during the file
 * descriptor release.
 *
 * The read lock is typically placed at the start of an IOCTL- or
 * user-space callable function that may end up allocating a memory area.
 * This includes setstatus, super-ioctls and no_pfn; the latter may move
 * unmappable regions to mappable. It's a bug to leave kernel space with the
 * read lock held.
 *
 * Both read- and write lock taking is interruptible for low signal-delivery
 * latency. The locking functions will return -EAGAIN if interrupted by a
 * signal.
 *
 * Locking order: The lock should be taken BEFORE any kernel mutexes
 * or spinlocks.
 */

#include "drmP.h"

void drm_bo_init_lock(struct drm_bo_lock *lock)
{
	DRM_INIT_WAITQUEUE(&lock->queue);
	atomic_set(&lock->write_lock_pending, 0);
	atomic_set(&lock->readers, 0);
}

void drm_bo_read_unlock(struct drm_bo_lock *lock)
{
	if (unlikely(atomic_add_negative(-1, &lock->readers)))
		BUG();
	if (atomic_read(&lock->readers) == 0)
		wake_up_interruptible(&lock->queue);
}

EXPORT_SYMBOL(drm_bo_read_unlock);

int drm_bo_read_lock(struct drm_bo_lock *lock)
{
	while (unlikely(atomic_read(&lock->write_lock_pending) != 0)) {
		int ret;
		ret = wait_event_interruptible
		    (lock->queue, atomic_read(&lock->write_lock_pending) == 0);
		if (ret)
			return -EAGAIN;
	}

	while (unlikely(!atomic_add_unless(&lock->readers, 1, -1))) {
		int ret;
		ret = wait_event_interruptible
		    (lock->queue, atomic_add_unless(&lock->readers, 1, -1));
		if (ret)
			return -EAGAIN;
	}
	return 0;
}

EXPORT_SYMBOL(drm_bo_read_lock);

static int __drm_bo_write_unlock(struct drm_bo_lock *lock)
{
	if (unlikely(atomic_cmpxchg(&lock->readers, -1, 0) != -1))
		return -EINVAL;
	if (unlikely(atomic_cmpxchg(&lock->write_lock_pending, 1, 0) != 1))
		return -EINVAL;
	wake_up_interruptible(&lock->queue);
	return 0;
}

static void drm_bo_write_lock_remove(struct drm_file *file_priv,
				     struct drm_user_object *item)
{
	struct drm_bo_lock *lock = container_of(item, struct drm_bo_lock, base);
	int ret;

	ret = __drm_bo_write_unlock(lock);
	BUG_ON(ret);
}

int drm_bo_write_lock(struct drm_bo_lock *lock, struct drm_file *file_priv)
{
	int ret = 0;
	struct drm_device *dev;

	if (unlikely(atomic_cmpxchg(&lock->write_lock_pending, 0, 1) != 0)) {
		return -EINVAL;
	}

	while (unlikely(atomic_cmpxchg(&lock->readers, 0, -1) != 0)) {
		ret = wait_event_interruptible
		    (lock->queue, atomic_cmpxchg(&lock->readers, 0, -1) == 0);

		if (ret) {
			atomic_set(&lock->write_lock_pending, 0);
			wake_up_interruptible(&lock->queue);
			return -EAGAIN;
		}
	}

	/*
	 * Add a dummy user-object, the destructor of which will
	 * make sure the lock is released if the client dies
	 * while holding it.
	 */

	dev = file_priv->head->dev;
	mutex_lock(&dev->struct_mutex);
	ret = drm_add_user_object(file_priv, &lock->base, 0);
	lock->base.remove = &drm_bo_write_lock_remove;
	lock->base.type = drm_lock_type;
	if (ret) {
		(void)__drm_bo_write_unlock(lock);
	}
	mutex_unlock(&dev->struct_mutex);

	return ret;
}

int drm_bo_write_unlock(struct drm_bo_lock *lock, struct drm_file *file_priv)
{
	struct drm_device *dev = file_priv->head->dev;
	struct drm_ref_object *ro;

	mutex_lock(&dev->struct_mutex);

	if (lock->base.owner != file_priv) {
		mutex_unlock(&dev->struct_mutex);
		return -EINVAL;
	}
	ro = drm_lookup_ref_object(file_priv, &lock->base, _DRM_REF_USE);
	BUG_ON(!ro);
	drm_remove_ref_object(file_priv, ro);
	lock->base.owner = NULL;

	mutex_unlock(&dev->struct_mutex);
	return 0;
}
