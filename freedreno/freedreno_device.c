/* -*- mode: C; c-file-style: "k&r"; tab-width 4; indent-tabs-mode: t; -*- */

/*
 * Copyright (C) 2012 Rob Clark <robclark@freedesktop.org>
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
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Authors:
 *    Rob Clark <robclark@freedesktop.org>
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "freedreno_drmif.h"
#include "freedreno_priv.h"

static pthread_mutex_t table_lock = PTHREAD_MUTEX_INITIALIZER;
static void * dev_table;

struct fd_device * kgsl_device_new(int fd);
struct fd_device * msm_device_new(int fd);

static struct fd_device * fd_device_new_impl(int fd)
{
	struct fd_device *dev;
	drmVersionPtr version;

	/* figure out if we are kgsl or msm drm driver: */
	version = drmGetVersion(fd);
	if (!version) {
		ERROR_MSG("cannot get version: %s", strerror(errno));
		return NULL;
	}

	if (!strcmp(version->name, "kgsl")) {
		DEBUG_MSG("kgsl DRM device");
		dev = kgsl_device_new(fd);
	} else if (!strcmp(version->name, "msm")) {
		DEBUG_MSG("msm DRM device");
		dev = msm_device_new(fd);
	} else {
		ERROR_MSG("unknown device: %s", version->name);
		dev = NULL;
	}

	if (!dev)
		return NULL;

	atomic_set(&dev->refcnt, 1);
	dev->fd = fd;
	dev->handle_table = drmHashCreate();
	dev->name_table = drmHashCreate();

	return dev;
}

struct fd_device * fd_device_new(int fd)
{
	struct fd_device *dev = NULL;
	int key = fd;

	pthread_mutex_lock(&table_lock);

	if (!dev_table)
		dev_table = drmHashCreate();

	if (drmHashLookup(dev_table, key, (void **)&dev)) {
		dev = fd_device_new_impl(fd);
		if (dev)
			drmHashInsert(dev_table, key, dev);
	} else {
		dev = fd_device_ref(dev);
	}

	pthread_mutex_unlock(&table_lock);

	return dev;
}

struct fd_device * fd_device_ref(struct fd_device *dev)
{
	atomic_inc(&dev->refcnt);
	return dev;
}

void fd_device_del(struct fd_device *dev)
{
	if (!atomic_dec_and_test(&dev->refcnt))
		return;
	pthread_mutex_lock(&table_lock);
	drmHashDestroy(dev->handle_table);
	drmHashDestroy(dev->name_table);
	drmHashDelete(dev_table, dev->fd);
	pthread_mutex_unlock(&table_lock);
	dev->funcs->destroy(dev);
}
