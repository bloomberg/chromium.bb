/*
 * Copyright © 2008 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 * Authors:
 *    Eric Anholt <eric@anholt.net>
 *
 */

#include <linux/types.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/module.h>
#include <linux/mman.h>
#include <linux/pagemap.h>
#include "drmP.h"

/** @file drm_gem.c
 *
 * This file provides some of the base ioctls and library routines for
 * the graphics memory manager implemented by each device driver.
 *
 * Because various devices have different requirements in terms of
 * synchronization and migration strategies, implementing that is left up to
 * the driver, and all that the general API provides should be generic --
 * allocating objects, reading/writing data with the cpu, freeing objects.
 * Even there, platform-dependent optimizations for reading/writing data with
 * the CPU mean we'll likely hook those out to driver-specific calls.  However,
 * the DRI2 implementation wants to have at least allocate/mmap be generic.
 *
 * The goal was to have swap-backed object allocation managed through
 * struct file.  However, file descriptors as handles to a struct file have
 * two major failings:
 * - Process limits prevent more than 1024 or so being used at a time by
 *   default.
 * - Inability to allocate high fds will aggravate the X Server's select()
 *   handling, and likely that of many GL client applications as well.
 *
 * This led to a plan of using our own integer IDs (called handles, following
 * DRM terminology) to mimic fds, and implement the fd syscalls we need as
 * ioctls.  The objects themselves will still include the struct file so
 * that we can transition to fds if the required kernel infrastructure shows
 * up at a later data, and as our interface with shmfs for memory allocation.
 */

static struct drm_gem_object *
drm_gem_object_alloc(struct drm_device *dev, size_t size)
{
	struct drm_gem_object *obj;

	BUG_ON((size & (PAGE_SIZE - 1)) != 0);

	obj = kcalloc(1, sizeof(*obj), GFP_KERNEL);

	obj->dev = dev;
	obj->filp = shmem_file_setup("drm mm object", size, 0);
	if (IS_ERR(obj->filp)) {
		kfree(obj);
		return NULL;
	}

	kref_init (&obj->refcount);
	kref_init (&obj->handlecount);
	obj->size = size;

	if (dev->driver->gem_init_object != NULL &&
	    dev->driver->gem_init_object(obj) != 0) {
		fput(obj->filp);
		kfree(obj);
		return NULL;
	}
	return obj;
}

/**
 * Removes the mapping from handle to filp for this object.
 */
static int
drm_gem_handle_delete(struct drm_file *filp, int handle)
{
	struct drm_gem_object *obj;

	/* This is gross. The idr system doesn't let us try a delete and
	 * return an error code.  It just spews if you fail at deleting.
	 * So, we have to grab a lock around finding the object and then
	 * doing the delete on it and dropping the refcount, or the user
	 * could race us to double-decrement the refcount and cause a
	 * use-after-free later.  Given the frequency of our handle lookups,
	 * we may want to use ida for number allocation and a hash table
	 * for the pointers, anyway.
	 */
	spin_lock(&filp->table_lock);

	/* Check if we currently have a reference on the object */
	obj = idr_find(&filp->object_idr, handle);
	if (obj == NULL) {
		spin_unlock(&filp->table_lock);
		return -EINVAL;
	}

	/* Release reference and decrement refcount. */
	idr_remove(&filp->object_idr, handle);
	spin_unlock(&filp->table_lock);

	drm_gem_object_handle_unreference(obj);

	return 0;
}

/**
 * Create a handle for this object. This adds a handle reference
 * to the object, which includes a regular reference count. Callers
 * will likely want to dereference the object afterwards.
 */
static int
drm_gem_handle_create (struct drm_file *file_priv,
		       struct drm_gem_object *obj,
		       int *handlep)
{
	int	ret;

	/*
	 * Get the user-visible handle using idr.
	 */
again:
	/* ensure there is space available to allocate a handle */
	if (idr_pre_get(&file_priv->object_idr, GFP_KERNEL) == 0) {
		kfree(obj);
		return -ENOMEM;
	}
	/* do the allocation under our spinlock */
	spin_lock (&file_priv->table_lock);
	ret = idr_get_new_above(&file_priv->object_idr, obj, 1, handlep);
	spin_unlock (&file_priv->table_lock);
	if (ret == -EAGAIN)
		goto again;

	if (ret != 0)
		return ret;
	
	drm_gem_object_handle_reference (obj);
	return 0;
}

/** Returns a reference to the object named by the handle. */
struct drm_gem_object *
drm_gem_object_lookup(struct drm_device *dev, struct drm_file *filp,
		      int handle)
{
	struct drm_gem_object *obj;

	spin_lock(&filp->table_lock);

	/* Check if we currently have a reference on the object */
	obj = idr_find(&filp->object_idr, handle);
	if (obj == NULL) {
		spin_unlock(&filp->table_lock);
		return NULL;
	}

	drm_gem_object_reference(obj);

	spin_unlock(&filp->table_lock);

	return obj;
}
EXPORT_SYMBOL(drm_gem_object_lookup);

/**
 * Allocates a new mm object and returns a handle to it.
 */
int
drm_gem_alloc_ioctl(struct drm_device *dev, void *data,
		    struct drm_file *file_priv)
{
	struct drm_gem_alloc *args = data;
	struct drm_gem_object *obj;
	int handle, ret;

	/* Round requested size up to page size */
	args->size = (args->size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

	/* Allocate the new object */
	obj = drm_gem_object_alloc(dev, args->size);
	if (obj == NULL)
		return -ENOMEM;

	ret = drm_gem_handle_create (file_priv, obj, &handle);
	drm_gem_object_handle_unreference(obj);

	if (ret)
		return ret;

	args->handle = handle;

	return 0;
}

/**
 * Releases the handle to an mm object.
 */
int
drm_gem_unreference_ioctl(struct drm_device *dev, void *data,
			  struct drm_file *file_priv)
{
	struct drm_gem_unreference *args = data;
	int ret;

	ret = drm_gem_handle_delete(file_priv, args->handle);

	return ret;
}

/**
 * Reads data from the object referenced by handle.
 *
 * On error, the contents of *data are undefined.
 */
int
drm_gem_pread_ioctl(struct drm_device *dev, void *data,
		    struct drm_file *file_priv)
{
	struct drm_gem_pread *args = data;
	struct drm_gem_object *obj;
	ssize_t read;
	loff_t offset;

	obj = drm_gem_object_lookup(dev, file_priv, args->handle);
	if (obj == NULL)
		return -EINVAL;

	offset = args->offset;

	read = obj->filp->f_op->read(obj->filp, (char __user *)(uintptr_t)args->data_ptr,
				     args->size, &offset);
	if (read != args->size) {
		drm_gem_object_unreference(obj);
		if (read < 0)
			return read;
		else
			return -EINVAL;
	}

	drm_gem_object_unreference(obj);

	return 0;
}

/**
 * Maps the contents of an object, returning the address it is mapped
 * into.
 *
 * While the mapping holds a reference on the contents of the object, it doesn't
 * imply a ref on the object itself.
 */
int
drm_gem_mmap_ioctl(struct drm_device *dev, void *data,
		   struct drm_file *file_priv)
{
	struct drm_gem_mmap *args = data;
	struct drm_gem_object *obj;
	loff_t offset;

	obj = drm_gem_object_lookup(dev, file_priv, args->handle);
	if (obj == NULL)
		return -EINVAL;

	offset = args->offset;

	down_write(&current->mm->mmap_sem);
	args->addr_ptr = (uint64_t) do_mmap(obj->filp, 0, args->size,
				    PROT_READ | PROT_WRITE, MAP_SHARED,
				    args->offset);
	up_write(&current->mm->mmap_sem);

	drm_gem_object_unreference(obj);

	return 0;
}

/**
 * Writes data to the object referenced by handle.
 *
 * On error, the contents of the buffer that were to be modified are undefined.
 */
int
drm_gem_pwrite_ioctl(struct drm_device *dev, void *data,
		     struct drm_file *file_priv)
{
	struct drm_gem_pwrite *args = data;
	struct drm_gem_object *obj;
	ssize_t written;
	loff_t offset;

	obj = drm_gem_object_lookup(dev, file_priv, args->handle);
	if (obj == NULL)
		return -EINVAL;

	offset = args->offset;

	written = obj->filp->f_op->write(obj->filp, (char __user *)(uintptr_t) args->data_ptr,
					 args->size, &offset);
	if (written != args->size) {
		drm_gem_object_unreference(obj);
		if (written < 0)
			return written;
		else
			return -EINVAL;
	}

	drm_gem_object_unreference(obj);

	return 0;
}

/**
 * Create a global name for an object, returning the name.
 *
 * Note that the name does not hold a reference; when the object
 * is freed, the name goes away.
 */
int
drm_gem_name_ioctl(struct drm_device *dev, void *data,
		   struct drm_file *file_priv)
{
	struct drm_gem_name *args = data;
	struct drm_gem_object *obj;
	int ret;

	obj = drm_gem_object_lookup(dev, file_priv, args->handle);
	if (obj == NULL)
		return -EINVAL;

again:
	if (idr_pre_get(&dev->object_name_idr, GFP_KERNEL) == 0) {
		return -ENOMEM;
	}
	spin_lock(&dev->object_name_lock);
	if (obj->name) {
		spin_unlock (&dev->object_name_lock);
		return -EEXIST;
	}
	ret = idr_get_new_above (&dev->object_name_idr, obj, 1,
				 &obj->name);
	spin_unlock (&dev->object_name_lock);
	if (ret == -EAGAIN)
		goto again;

	if (ret != 0) {
		drm_gem_object_unreference(obj);
		return ret;
	}

	/* 
	 * Leave the reference from the lookup around as the
	 * name table now holds one
	 */
	args->name = (uint64_t) obj->name;

	return 0;
}

/**
 * Open an object using the global name, returning a handle and the size.
 *
 * This handle (of course) holds a reference to the object, so the object
 * will not go away until the handle is deleted.
 */
int
drm_gem_open_ioctl(struct drm_device *dev, void *data,
		   struct drm_file *file_priv)
{
	struct drm_gem_open *args = data;
	struct drm_gem_object *obj;
	int ret;
	int handle;

	spin_lock (&dev->object_name_lock);
	obj = idr_find (&dev->object_name_idr, (int) args->name);
	if (obj)
		drm_gem_object_reference (obj);
	spin_unlock (&dev->object_name_lock);
	if (!obj)
		return -ENOENT;

	ret = drm_gem_handle_create (file_priv, obj, &handle);
	drm_gem_object_unreference (obj);
	if (ret)
		return ret;

	args->handle = handle;
	args->size = obj->size;

	return 0;
}

/**
 * Called at device open time, sets up the structure for handling refcounting
 * of mm objects.
 */
void
drm_gem_open(struct drm_device *dev, struct drm_file *file_private)
{
	idr_init(&file_private->object_idr);
}

/** Called at device close to release the file's handle references on objects. */
static int
drm_gem_object_release_handle (int id, void *ptr, void *data)
{
	struct drm_gem_object *obj = ptr;

	drm_gem_object_handle_unreference(obj);

	return 0;
}

/**
 * Called at close time when the filp is going away.
 *
 * Releases any remaining references on objects by this filp.
 */
void
drm_gem_release(struct drm_device *dev, struct drm_file *file_private)
{
	idr_for_each(&file_private->object_idr, &drm_gem_object_release_handle, NULL);

	idr_destroy(&file_private->object_idr);
}

/**
 * Called after the last reference to the object has been lost.
 *
 * Frees the object
 */
void
drm_gem_object_free (struct kref *kref)
{
	struct drm_gem_object *obj = (struct drm_gem_object *) kref;
	struct drm_device *dev = obj->dev;

	if (dev->driver->gem_free_object != NULL)
		dev->driver->gem_free_object(obj);

	fput(obj->filp);

	kfree(obj);
}
EXPORT_SYMBOL(drm_gem_object_free);

/**
 * Called after the last handle to the object has been closed
 *
 * Removes any name for the object. Note that this must be
 * called before drm_gem_object_free or we'll be touching
 * freed memory
 */
void
drm_gem_object_handle_free (struct kref *kref)
{
	struct drm_gem_object *obj = container_of (kref, struct drm_gem_object, handlecount);
	struct drm_device *dev = obj->dev;

	/* Remove any name for this object */
	spin_lock (&dev->object_name_lock);
	if (obj->name) {
		idr_remove (&dev->object_name_idr, obj->name);
		spin_unlock (&dev->object_name_lock);
		/*
		 * The object name held a reference to this object, drop
		 * that now.
		 */
		drm_gem_object_unreference (obj);
	} else
		spin_unlock (&dev->object_name_lock);
	
}
EXPORT_SYMBOL(drm_gem_object_handle_free);
