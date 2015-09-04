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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "freedreno_drmif.h"
#include "freedreno_priv.h"

static pthread_mutex_t table_lock = PTHREAD_MUTEX_INITIALIZER;

struct fd_device * kgsl_device_new(int fd);
struct fd_device * msm_device_new(int fd);

static void
add_bucket(struct fd_device *dev, int size)
{
	unsigned int i = dev->num_buckets;

	assert(i < ARRAY_SIZE(dev->cache_bucket));

	list_inithead(&dev->cache_bucket[i].list);
	dev->cache_bucket[i].size = size;
	dev->num_buckets++;
}

static void
init_cache_buckets(struct fd_device *dev)
{
	unsigned long size, cache_max_size = 64 * 1024 * 1024;

	/* OK, so power of two buckets was too wasteful of memory.
	 * Give 3 other sizes between each power of two, to hopefully
	 * cover things accurately enough.  (The alternative is
	 * probably to just go for exact matching of sizes, and assume
	 * that for things like composited window resize the tiled
	 * width/height alignment and rounding of sizes to pages will
	 * get us useful cache hit rates anyway)
	 */
	add_bucket(dev, 4096);
	add_bucket(dev, 4096 * 2);
	add_bucket(dev, 4096 * 3);

	/* Initialize the linked lists for BO reuse cache. */
	for (size = 4 * 4096; size <= cache_max_size; size *= 2) {
		add_bucket(dev, size);
		add_bucket(dev, size + size * 1 / 4);
		add_bucket(dev, size + size * 2 / 4);
		add_bucket(dev, size + size * 3 / 4);
	}
}

struct fd_device * fd_device_new(int fd)
{
	struct fd_device *dev;
	drmVersionPtr version;

	/* figure out if we are kgsl or msm drm driver: */
	version = drmGetVersion(fd);
	if (!version) {
		ERROR_MSG("cannot get version: %s", strerror(errno));
		return NULL;
	}

	if (!strcmp(version->name, "msm")) {
		DEBUG_MSG("msm DRM device");
		dev = msm_device_new(fd);
#ifdef HAVE_FREEDRENO_KGSL
	} else if (!strcmp(version->name, "kgsl")) {
		DEBUG_MSG("kgsl DRM device");
		dev = kgsl_device_new(fd);
#endif
	} else {
		ERROR_MSG("unknown device: %s", version->name);
		dev = NULL;
	}
	drmFreeVersion(version);

	if (!dev)
		return NULL;

	atomic_set(&dev->refcnt, 1);
	dev->fd = fd;
	dev->handle_table = drmHashCreate();
	dev->name_table = drmHashCreate();
	init_cache_buckets(dev);

	return dev;
}

/* like fd_device_new() but creates it's own private dup() of the fd
 * which is close()d when the device is finalized.
 */
struct fd_device * fd_device_new_dup(int fd)
{
	struct fd_device *dev = fd_device_new(dup(fd));
	if (dev)
		dev->closefd = 1;
	return dev;
}

struct fd_device * fd_device_ref(struct fd_device *dev)
{
	atomic_inc(&dev->refcnt);
	return dev;
}

static void fd_device_del_impl(struct fd_device *dev)
{
	fd_cleanup_bo_cache(dev, 0);
	drmHashDestroy(dev->handle_table);
	drmHashDestroy(dev->name_table);
	if (dev->closefd)
		close(dev->fd);
	dev->funcs->destroy(dev);
}

drm_private void fd_device_del_locked(struct fd_device *dev)
{
	if (!atomic_dec_and_test(&dev->refcnt))
		return;
	fd_device_del_impl(dev);
}

void fd_device_del(struct fd_device *dev)
{
	if (!atomic_dec_and_test(&dev->refcnt))
		return;
	pthread_mutex_lock(&table_lock);
	fd_device_del_impl(dev);
	pthread_mutex_unlock(&table_lock);
}

int fd_device_fd(struct fd_device *dev)
{
	return dev->fd;
}
