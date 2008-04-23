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
#include "mmfs.h"

/** @file mmfs.c
 *
 * This file provides the filesystem for memory manager objects used by the
 * DRM.
 *
 * The goal is to have swap-backed object allocation managed through
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

static struct mmfs_object *
mmfs_object_alloc(size_t size)
{
	struct mmfs_object *obj;

	BUG_ON((size & (PAGE_SIZE - 1)) != 0);

	obj = kcalloc(1, sizeof(*obj), GFP_KERNEL);

	obj->filp = shmem_file_setup("mmfs object", size, 0);
	if (IS_ERR(obj->filp)) {
		kfree(obj);
		return NULL;
	}

	obj->refcount = 1;

	return obj;
}

/**
 * Removes the mapping from handle to filp for this object.
 */
static int
mmfs_handle_delete(struct mmfs_file *mmfs_filp, int handle)
{
	struct mmfs_object *obj;

	/* This is gross. The idr system doesn't let us try a delete and
	 * return an error code.  It just spews if you fail at deleting.
	 * So, we have to grab a lock around finding the object and then
	 * doing the delete on it and dropping the refcount, or the user
	 * could race us to double-decrement the refcount and cause a
	 * use-after-free later.  Given the frequency of our handle lookups,
	 * we may want to use ida for number allocation and a hash table
	 * for the pointers, anyway.
	 */
	spin_lock(&mmfs_filp->table_lock);

	/* Check if we currently have a reference on the object */
	obj = idr_find(&mmfs_filp->object_idr, handle);
	if (obj == NULL) {
		spin_unlock(&mmfs_filp->table_lock);
		return -EINVAL;
	}

	/* Release reference and decrement refcount. */
	idr_remove(&mmfs_filp->object_idr, handle);
	mmfs_object_unreference(obj);

	spin_unlock(&mmfs_filp->table_lock);

	return 0;
}

/** Returns a reference to the object named by the handle. */
static struct mmfs_object *
mmfs_object_lookup(struct mmfs_file *mmfs_filp, int handle)
{
	struct mmfs_object *obj;

	spin_lock(&mmfs_filp->table_lock);

	/* Check if we currently have a reference on the object */
	obj = idr_find(&mmfs_filp->object_idr, handle);
	if (obj == NULL) {
		spin_unlock(&mmfs_filp->table_lock);
		return NULL;
	}

	mmfs_object_reference(obj);

	spin_unlock(&mmfs_filp->table_lock);

	return obj;
}


/**
 * Allocates a new mmfs object and returns a handle to it.
 */
static int
mmfs_alloc_ioctl(struct inode *inode, struct file *filp,
		 unsigned int cmd, unsigned long arg)
{
	struct mmfs_file *mmfs_filp = filp->private_data;
	struct mmfs_alloc_args args;
	struct mmfs_object *obj;
	int handle, ret;

	if (copy_from_user(&args, (void __user *)arg, sizeof(args)))
		return -EFAULT;

	/* Round requested size up to page size */
	args.size = (args.size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

	/* Allocate the new object */
	obj = mmfs_object_alloc(args.size);
	if (obj == NULL)
		return -ENOMEM;

	/* Get the user-visible handle using idr.
	 *
	 * I'm not really sure why the idr api needs us to do this in two
	 * repeating steps.  It handles internal locking of its data
	 * structure, yet insists that we keep its memory allocation step
	 * separate from its slot-finding step for locking purposes.
	 */
	do {
		if (idr_pre_get(&mmfs_filp->object_idr, GFP_KERNEL) == 0) {
			kfree(obj);
			return -EFAULT;
		}

		ret = idr_get_new(&mmfs_filp->object_idr, obj, &handle);
	} while (ret == -EAGAIN);

	if (ret != 0) {
		mmfs_object_unreference(obj);
		return -EFAULT;
	}

	args.handle = handle;

	if (copy_to_user((void __user *)arg, &args, sizeof(args))) {
		mmfs_handle_delete(mmfs_filp, args.handle);
		return -EFAULT;
	}

	return 0;
}

/**
 * Releases the handle to an mmfs object.
 */
static int
mmfs_unreference_ioctl(struct inode *inode, struct file *filp,
		       unsigned int cmd, unsigned long arg)
{
	struct mmfs_file *mmfs_filp = filp->private_data;
	struct mmfs_unreference_args args;
	int ret;

	if (copy_from_user(&args, (void __user *)arg, sizeof(args)))
		return -EFAULT;

	ret = mmfs_handle_delete(mmfs_filp, args.handle);

	return ret;
}

/**
 * Reads data from the object referenced by handle.
 *
 * On error, the contents of *data are undefined.
 */
static int
mmfs_pread_ioctl(struct inode *inode, struct file *filp,
		 unsigned int cmd, unsigned long arg)
{
	struct mmfs_file *mmfs_filp = filp->private_data;
	struct mmfs_pread_args args;
	struct mmfs_object *obj;
	ssize_t read;
	loff_t offset;

	if (copy_from_user(&args, (void __user *)arg, sizeof(args)))
		return -EFAULT;

	obj = mmfs_object_lookup(mmfs_filp, args.handle);
	if (obj == NULL)
		return -EINVAL;

	offset = args.offset;

	read = obj->filp->f_op->read(obj->filp, (char __user *)args.data,
				     args.size, &offset);
	if (read != args.size) {
		mmfs_object_unreference(obj);
		if (read < 0)
			return read;
		else
			return -EINVAL;
	}

	mmfs_object_unreference(obj);

	return 0;
}

/**
 * Maps the contents of an object, returning the address it is mapped
 * into.
 *
 * While the mapping holds a reference on the contents of the object, it doesn't
 * imply a ref on the object itself.
 */
static int
mmfs_mmap_ioctl(struct inode *inode, struct file *filp,
		 unsigned int cmd, unsigned long arg)
{
	struct mmfs_file *mmfs_filp = filp->private_data;
	struct mmfs_mmap_args args;
	struct mmfs_object *obj;
	loff_t offset;

	if (copy_from_user(&args, (void __user *)arg, sizeof(args)))
		return -EFAULT;

	obj = mmfs_object_lookup(mmfs_filp, args.handle);
	if (obj == NULL)
		return -EINVAL;

	offset = args.offset;

	down_write(&current->mm->mmap_sem);
	args.addr = (void *)do_mmap(obj->filp, 0, args.size,
				    PROT_READ | PROT_WRITE, MAP_SHARED,
				    args.offset);
	up_write(&current->mm->mmap_sem);

	mmfs_object_unreference(obj);

	if (copy_to_user((void __user *)arg, &args, sizeof(args)))
		return -EFAULT;

	return 0;
}

/**
 * Writes data to the object referenced by handle.
 *
 * On error, the contents of the buffer that were to be modified are undefined.
 */
static int
mmfs_pwrite_ioctl(struct inode *inode, struct file *filp,
		 unsigned int cmd, unsigned long arg)
{
	struct mmfs_file *mmfs_filp = filp->private_data;
	struct mmfs_pwrite_args args;
	struct mmfs_object *obj;
	ssize_t written;
	loff_t offset;

	if (copy_from_user(&args, (void __user *)arg, sizeof(args)))
		return -EFAULT;

	obj = mmfs_object_lookup(mmfs_filp, args.handle);
	if (obj == NULL)
		return -EINVAL;

	offset = args.offset;

	written = obj->filp->f_op->write(obj->filp, (char __user *)args.data,
					 args.size, &offset);
	if (written != args.size) {
		mmfs_object_unreference(obj);
		if (written < 0)
			return written;
		else
			return -EINVAL;
	}

	mmfs_object_unreference(obj);

	return 0;
}

static int
mmfs_ioctl(struct inode *inode, struct file *filp,
	   unsigned int cmd, unsigned long arg)
{

	switch (cmd) {
	case MMFS_IOCTL_ALLOC:
		return mmfs_alloc_ioctl(inode, filp, cmd, arg);
	case MMFS_IOCTL_UNREFERENCE:
		return mmfs_unreference_ioctl(inode, filp, cmd, arg);
	case MMFS_IOCTL_PREAD:
		return mmfs_pread_ioctl(inode, filp, cmd, arg);
	case MMFS_IOCTL_PWRITE:
		return mmfs_pwrite_ioctl(inode, filp, cmd, arg);
	case MMFS_IOCTL_MMAP:
		return mmfs_mmap_ioctl(inode, filp, cmd, arg);
	default:
		return -EINVAL;
	}
}

/**
 * Sets up the file private for keeping track of our mappings of handles to
 * mmfs objects.
 */
int
mmfs_open(struct inode *inode, struct file *filp)
{
	struct mmfs_file *mmfs_filp;

	if (filp->f_flags & O_EXCL)
		return -EBUSY;	/* No exclusive opens */

	mmfs_filp = kcalloc(1, sizeof(*mmfs_filp), GFP_KERNEL);
	if (mmfs_filp == NULL)
		return -ENOMEM;
	filp->private_data = mmfs_filp;

	idr_init(&mmfs_filp->object_idr);

	return 0;
}

/** Called at device close to release the file's references on objects. */
static int
mmfs_object_release(int id, void *ptr, void *data)
{
	struct mmfs_object *obj = ptr;

	mmfs_object_unreference(obj);

	return 0;
}

/**
 * Called at close time when the filp is going away.
 *
 * Releases any remaining references on objects by this filp.
 */
int
mmfs_close(struct inode *inode, struct file *filp)
{
	struct mmfs_file *mmfs_filp = filp->private_data;

	idr_for_each(&mmfs_filp->object_idr, &mmfs_object_release, NULL);

	idr_destroy(&mmfs_filp->object_idr);

	kfree(mmfs_filp);
	filp->private_data = NULL;

	return 0;
}

void
mmfs_object_reference(struct mmfs_object *obj)
{
	spin_lock(&obj->lock);
	obj->refcount++;
	spin_unlock(&obj->lock);
}

void
mmfs_object_unreference(struct mmfs_object *obj)
{
	spin_lock(&obj->lock);
	obj->refcount--;
	spin_unlock(&obj->lock);
	if (obj->refcount == 0) {
		fput(obj->filp);
		kfree(obj);
	}
}

/** File operations structure */
static const struct file_operations mmfs_dev_fops = {
	.owner = THIS_MODULE,
	.open = mmfs_open,
	.release = mmfs_close,
	.ioctl = mmfs_ioctl,
};

static int __init mmfs_init(void)
{
	int ret;

	ret = register_chrdev(MMFS_DEVICE_MAJOR, "mmfs", &mmfs_dev_fops);
	if (ret != 0)
		return ret;

	return 0;
}

static void __exit mmfs_exit(void)
{
	unregister_chrdev(MMFS_DEVICE_MAJOR, "mmfs");
}

module_init(mmfs_init);
module_exit(mmfs_exit);
MODULE_LICENSE("GPL and additional rights");
