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

#include <linux/spinlock.h>
#include <linux/idr.h>

/** @file mmfs_drv.h
 * This file provides structure definitions and function prototypes for mmfs.
 */

/**
 * This structure defines the mmfs memory object, which will be used by the
 * DRM for its buffer objects.
 */
struct mmfs_object {
	/** File representing the shmem storage */
	struct file *filp;

	spinlock_t lock;

	size_t size;
	/** Reference count of this object, protected by object_lock */
	int refcount;
};

/**
 * This structure defines the process (actually per-fd) mapping of object
 * handles to mmfs objects.
 */
struct mmfs_file {
	/** Mapping of object handles to object pointers. */
	struct idr object_idr;
	/**
	 * Lock for synchronization of access to object->refcount and
	 * object_idr.  See note in mmfs_unreference_ioctl.
	 */
	spinlock_t delete_lock;
};

void mmfs_object_reference(struct mmfs_object *obj);
void mmfs_object_unreference(struct mmfs_object *obj);
