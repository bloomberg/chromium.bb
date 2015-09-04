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

#include "freedreno_drmif.h"
#include "freedreno_priv.h"

static pthread_mutex_t table_lock = PTHREAD_MUTEX_INITIALIZER;

static void bo_del(struct fd_bo *bo);

/* set buffer name, and add to table, call w/ table_lock held: */
static void set_name(struct fd_bo *bo, uint32_t name)
{
	bo->name = name;
	/* add ourself into the handle table: */
	drmHashInsert(bo->dev->name_table, name, bo);
}

/* lookup a buffer, call w/ table_lock held: */
static struct fd_bo * lookup_bo(void *tbl, uint32_t key)
{
	struct fd_bo *bo = NULL;
	if (!drmHashLookup(tbl, key, (void **)&bo)) {
		/* found, incr refcnt and return: */
		bo = fd_bo_ref(bo);

		/* don't break the bucket if this bo was found in one */
		list_delinit(&bo->list);
	}
	return bo;
}

/* allocate a new buffer object, call w/ table_lock held */
static struct fd_bo * bo_from_handle(struct fd_device *dev,
		uint32_t size, uint32_t handle)
{
	struct fd_bo *bo;

	bo = dev->funcs->bo_from_handle(dev, size, handle);
	if (!bo) {
		struct drm_gem_close req = {
				.handle = handle,
		};
		drmIoctl(dev->fd, DRM_IOCTL_GEM_CLOSE, &req);
		return NULL;
	}
	bo->dev = fd_device_ref(dev);
	bo->size = size;
	bo->handle = handle;
	atomic_set(&bo->refcnt, 1);
	list_inithead(&bo->list);
	/* add ourself into the handle table: */
	drmHashInsert(dev->handle_table, handle, bo);
	return bo;
}

/* Frees older cached buffers.  Called under table_lock */
drm_private void fd_cleanup_bo_cache(struct fd_device *dev, time_t time)
{
	int i;

	if (dev->time == time)
		return;

	for (i = 0; i < dev->num_buckets; i++) {
		struct fd_bo_bucket *bucket = &dev->cache_bucket[i];
		struct fd_bo *bo;

		while (!LIST_IS_EMPTY(&bucket->list)) {
			bo = LIST_ENTRY(struct fd_bo, bucket->list.next, list);

			/* keep things in cache for at least 1 second: */
			if (time && ((time - bo->free_time) <= 1))
				break;

			list_del(&bo->list);
			bo_del(bo);
		}
	}

	dev->time = time;
}

static struct fd_bo_bucket * get_bucket(struct fd_device *dev, uint32_t size)
{
	int i;

	/* hmm, this is what intel does, but I suppose we could calculate our
	 * way to the correct bucket size rather than looping..
	 */
	for (i = 0; i < dev->num_buckets; i++) {
		struct fd_bo_bucket *bucket = &dev->cache_bucket[i];
		if (bucket->size >= size) {
			return bucket;
		}
	}

	return NULL;
}

static int is_idle(struct fd_bo *bo)
{
	return fd_bo_cpu_prep(bo, NULL,
			DRM_FREEDRENO_PREP_READ |
			DRM_FREEDRENO_PREP_WRITE |
			DRM_FREEDRENO_PREP_NOSYNC) == 0;
}

static struct fd_bo *find_in_bucket(struct fd_device *dev,
		struct fd_bo_bucket *bucket, uint32_t flags)
{
	struct fd_bo *bo = NULL;

	/* TODO .. if we had an ALLOC_FOR_RENDER flag like intel, we could
	 * skip the busy check.. if it is only going to be a render target
	 * then we probably don't need to stall..
	 *
	 * NOTE that intel takes ALLOC_FOR_RENDER bo's from the list tail
	 * (MRU, since likely to be in GPU cache), rather than head (LRU)..
	 */
	pthread_mutex_lock(&table_lock);
	while (!LIST_IS_EMPTY(&bucket->list)) {
		bo = LIST_ENTRY(struct fd_bo, bucket->list.next, list);
		if (0 /* TODO: if madvise tells us bo is gone... */) {
			list_del(&bo->list);
			bo_del(bo);
			bo = NULL;
			continue;
		}
		/* TODO check for compatible flags? */
		if (is_idle(bo)) {
			list_del(&bo->list);
			break;
		}
		bo = NULL;
		break;
	}
	pthread_mutex_unlock(&table_lock);

	return bo;
}


struct fd_bo *
fd_bo_new(struct fd_device *dev, uint32_t size, uint32_t flags)
{
	struct fd_bo *bo = NULL;
	struct fd_bo_bucket *bucket;
	uint32_t handle;
	int ret;

	size = ALIGN(size, 4096);
	bucket = get_bucket(dev, size);

	/* see if we can be green and recycle: */
	if (bucket) {
		size = bucket->size;
		bo = find_in_bucket(dev, bucket, flags);
		if (bo) {
			atomic_set(&bo->refcnt, 1);
			fd_device_ref(bo->dev);
			return bo;
		}
	}

	ret = dev->funcs->bo_new_handle(dev, size, flags, &handle);
	if (ret)
		return NULL;

	pthread_mutex_lock(&table_lock);
	bo = bo_from_handle(dev, size, handle);
	bo->bo_reuse = 1;
	pthread_mutex_unlock(&table_lock);

	return bo;
}

struct fd_bo *
fd_bo_from_handle(struct fd_device *dev, uint32_t handle, uint32_t size)
{
	struct fd_bo *bo = NULL;

	pthread_mutex_lock(&table_lock);

	bo = lookup_bo(dev->handle_table, handle);
	if (bo)
		goto out_unlock;

	bo = bo_from_handle(dev, size, handle);

out_unlock:
	pthread_mutex_unlock(&table_lock);

	return bo;
}

struct fd_bo *
fd_bo_from_dmabuf(struct fd_device *dev, int fd)
{
	int ret, size;
	uint32_t handle;
	struct fd_bo *bo;

	pthread_mutex_lock(&table_lock);
	ret = drmPrimeFDToHandle(dev->fd, fd, &handle);
	if (ret) {
		return NULL;
	}

	bo = lookup_bo(dev->handle_table, handle);
	if (bo)
		goto out_unlock;

	/* lseek() to get bo size */
	size = lseek(fd, 0, SEEK_END);
	lseek(fd, 0, SEEK_CUR);

	bo = bo_from_handle(dev, size, handle);

out_unlock:
	pthread_mutex_unlock(&table_lock);

	return bo;
}

struct fd_bo * fd_bo_from_name(struct fd_device *dev, uint32_t name)
{
	struct drm_gem_open req = {
			.name = name,
	};
	struct fd_bo *bo;

	pthread_mutex_lock(&table_lock);

	/* check name table first, to see if bo is already open: */
	bo = lookup_bo(dev->name_table, name);
	if (bo)
		goto out_unlock;

	if (drmIoctl(dev->fd, DRM_IOCTL_GEM_OPEN, &req)) {
		ERROR_MSG("gem-open failed: %s", strerror(errno));
		goto out_unlock;
	}

	bo = lookup_bo(dev->handle_table, req.handle);
	if (bo)
		goto out_unlock;

	bo = bo_from_handle(dev, req.size, req.handle);
	if (bo)
		set_name(bo, name);

out_unlock:
	pthread_mutex_unlock(&table_lock);

	return bo;
}

struct fd_bo * fd_bo_ref(struct fd_bo *bo)
{
	atomic_inc(&bo->refcnt);
	return bo;
}

void fd_bo_del(struct fd_bo *bo)
{
	struct fd_device *dev = bo->dev;

	if (!atomic_dec_and_test(&bo->refcnt))
		return;

	pthread_mutex_lock(&table_lock);

	if (bo->bo_reuse) {
		struct fd_bo_bucket *bucket = get_bucket(dev, bo->size);

		/* see if we can be green and recycle: */
		if (bucket) {
			struct timespec time;

			clock_gettime(CLOCK_MONOTONIC, &time);

			bo->free_time = time.tv_sec;
			list_addtail(&bo->list, &bucket->list);
			fd_cleanup_bo_cache(dev, time.tv_sec);

			/* bo's in the bucket cache don't have a ref and
			 * don't hold a ref to the dev:
			 */

			goto out;
		}
	}

	bo_del(bo);
out:
	fd_device_del_locked(dev);
	pthread_mutex_unlock(&table_lock);
}

/* Called under table_lock */
static void bo_del(struct fd_bo *bo)
{
	if (bo->map)
		drm_munmap(bo->map, bo->size);

	/* TODO probably bo's in bucket list get removed from
	 * handle table??
	 */

	if (bo->handle) {
		struct drm_gem_close req = {
				.handle = bo->handle,
		};
		drmHashDelete(bo->dev->handle_table, bo->handle);
		if (bo->name)
			drmHashDelete(bo->dev->name_table, bo->name);
		drmIoctl(bo->dev->fd, DRM_IOCTL_GEM_CLOSE, &req);
	}

	bo->funcs->destroy(bo);
}

int fd_bo_get_name(struct fd_bo *bo, uint32_t *name)
{
	if (!bo->name) {
		struct drm_gem_flink req = {
				.handle = bo->handle,
		};
		int ret;

		ret = drmIoctl(bo->dev->fd, DRM_IOCTL_GEM_FLINK, &req);
		if (ret) {
			return ret;
		}

		pthread_mutex_lock(&table_lock);
		set_name(bo, req.name);
		pthread_mutex_unlock(&table_lock);
		bo->bo_reuse = 0;
	}

	*name = bo->name;

	return 0;
}

uint32_t fd_bo_handle(struct fd_bo *bo)
{
	return bo->handle;
}

int fd_bo_dmabuf(struct fd_bo *bo)
{
	int ret, prime_fd;

	ret = drmPrimeHandleToFD(bo->dev->fd, bo->handle, DRM_CLOEXEC,
			&prime_fd);
	if (ret) {
		ERROR_MSG("failed to get dmabuf fd: %d", ret);
		return ret;
	}

	bo->bo_reuse = 0;

	return prime_fd;
}

uint32_t fd_bo_size(struct fd_bo *bo)
{
	return bo->size;
}

void * fd_bo_map(struct fd_bo *bo)
{
	if (!bo->map) {
		uint64_t offset;
		int ret;

		ret = bo->funcs->offset(bo, &offset);
		if (ret) {
			return NULL;
		}

		bo->map = drm_mmap(0, bo->size, PROT_READ | PROT_WRITE, MAP_SHARED,
				bo->dev->fd, offset);
		if (bo->map == MAP_FAILED) {
			ERROR_MSG("mmap failed: %s", strerror(errno));
			bo->map = NULL;
		}
	}
	return bo->map;
}

/* a bit odd to take the pipe as an arg, but it's a, umm, quirk of kgsl.. */
int fd_bo_cpu_prep(struct fd_bo *bo, struct fd_pipe *pipe, uint32_t op)
{
	return bo->funcs->cpu_prep(bo, pipe, op);
}

void fd_bo_cpu_fini(struct fd_bo *bo)
{
	bo->funcs->cpu_fini(bo);
}
