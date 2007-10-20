#include "drmP.h"

void drm_bo_init_lock(struct drm_bo_lock *lock)
{
	DRM_INIT_WAITQUEUE(&lock->queue);
	atomic_set(&lock->write_lock_pending, 0);
	atomic_set(&lock->readers, 0);
	
}

void drm_bo_read_unlock(struct drm_bo_lock *lock)
{
	if (unlikely(atomic_add_negative(-1, &lock->readers) == 0))
		BUG();
	if (atomic_read(&lock->readers) == 0)
		wake_up_interruptible(&lock->queue);
}

int drm_bo_read_lock(struct drm_bo_lock *lock)
{
	while( unlikely(atomic_read(&lock->write_lock_pending) != 0)) {
		int ret;
		ret = wait_event_interruptible
			(lock->queue, 
			 atomic_read(&lock->write_lock_pending) == 0);
		if (ret)
			return -EAGAIN;
	}

	while( unlikely (!atomic_add_unless(&lock->readers, 1, -1))) {
		int ret;
		ret = wait_event_interruptible
			(lock->queue, 
			 atomic_add_unless(&lock->readers, 1, -1));
		if (ret)
			return -EAGAIN;
	}
	return 0;
}
		
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
	struct drm_bo_lock *lock = 
		container_of(item, struct drm_bo_lock, base);
	int ret;

	ret = __drm_bo_write_unlock(lock);
	BUG_ON(ret);
}
		
int drm_bo_write_lock(struct drm_bo_lock *lock, struct drm_file *file_priv)
{
	int ret = 0;
	struct drm_device *dev;

	if (unlikely(atomic_cmpxchg(&lock->write_lock_pending, 0, 1) != 0)) 
		return -EINVAL;

	while(unlikely(atomic_cmpxchg(&lock->readers, 0, -1) != 0)) {
		ret = wait_event_interruptible
			(lock->queue,
			 atomic_cmpxchg(&lock->readers, 0, -1) == 0);

		if (ret) {
			atomic_set(&lock->write_lock_pending, 0);
			wake_up_interruptible(&lock->queue);
			return -EAGAIN;
		}
	}
	
	dev = file_priv->head->dev;
	mutex_lock(&dev->struct_mutex);
	ret = drm_add_user_object(file_priv, &lock->base, 0);
	lock->base.remove = &drm_bo_write_lock_remove;
	lock->base.type = drm_lock_type;
	if (ret) 
		(void) __drm_bo_write_unlock(lock);		
	mutex_unlock(&dev->struct_mutex);

	return ret;
}


int drm_bo_write_unlock(struct drm_bo_lock *lock, struct drm_file *file_priv)
{
	return drm_user_object_unref(file_priv, lock->base.hash.key, 
				     drm_lock_type);
}
