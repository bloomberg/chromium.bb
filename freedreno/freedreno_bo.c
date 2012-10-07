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

#include "freedreno_drmif.h"
#include "freedreno_priv.h"

#include <linux/fb.h>

static struct fd_bo * bo_from_handle(struct fd_device *dev,
		uint32_t size, uint32_t handle)
{
	unsigned i;
	struct fd_bo *bo = calloc(1, sizeof(*bo));
	if (!bo)
		return NULL;
	bo->dev = dev;
	bo->size = size;
	bo->handle = handle;
	atomic_set(&bo->refcnt, 1);
	for (i = 0; i < ARRAY_SIZE(bo->list); i++)
		list_inithead(&bo->list[i]);
	return bo;
}

static int set_memtype(struct fd_bo *bo, uint32_t flags)
{
	struct drm_kgsl_gem_memtype req = {
			.handle = bo->handle,
			.type = flags & DRM_FREEDRENO_GEM_TYPE_MEM_MASK,
	};

	return drmCommandWrite(bo->dev->fd, DRM_KGSL_GEM_SETMEMTYPE,
			&req, sizeof(req));
}

static int bo_alloc(struct fd_bo *bo)
{
	if (!bo->offset) {
		struct drm_kgsl_gem_alloc req = {
				.handle = bo->handle,
		};
		int ret;

		/* if the buffer is already backed by pages then this
		 * doesn't actually do anything (other than giving us
		 * the offset)
		 */
		ret = drmCommandWriteRead(bo->dev->fd, DRM_KGSL_GEM_ALLOC,
				&req, sizeof(req));
		if (ret) {
			ERROR_MSG("alloc failed: %s", strerror(errno));
			return ret;
		}

		bo->offset = req.offset;
	}
	return 0;
}

struct fd_bo * fd_bo_new(struct fd_device *dev,
		uint32_t size, uint32_t flags)
{
	struct drm_kgsl_gem_create req = {
			.size = ALIGN(size, 4096),
	};
	struct fd_bo *bo = NULL;

	if (drmCommandWriteRead(dev->fd, DRM_KGSL_GEM_CREATE,
			&req, sizeof(req))) {
		return NULL;
	}

	bo = bo_from_handle(dev, size, req.handle);
	if (!bo) {
		goto fail;
	}

	if (set_memtype(bo, flags)) {
		goto fail;
	}

	return bo;
fail:
	if (bo)
		fd_bo_del(bo);
	return NULL;
}

/* don't use this... it is just needed to get a bo from the
 * framebuffer (pre-dmabuf)
 */
struct fd_bo * fd_bo_from_fbdev(struct fd_pipe *pipe,
		int fbfd, uint32_t size)
{
	struct drm_kgsl_gem_create_fd req = {
			.fd = fbfd,
	};
	struct fd_bo *bo;

	if (drmCommandWriteRead(pipe->dev->fd, DRM_KGSL_GEM_CREATE_FD,
			&req, sizeof(req))) {
		return NULL;
	}

	bo = bo_from_handle(pipe->dev, size, req.handle);

	/* this is fugly, but works around a bug in the kernel..
	 * priv->memdesc.size never gets set, so getbufinfo ioctl
	 * thinks the buffer hasn't be allocate and fails
	 */
	if (bo && !fd_bo_gpuaddr(bo, 0)) {
		void *fbmem = mmap(NULL, size, PROT_READ | PROT_WRITE,
				MAP_SHARED, fbfd, 0);
		struct kgsl_map_user_mem req = {
				.memtype = KGSL_USER_MEM_TYPE_ADDR,
				.len     = size,
				.offset  = 0,
				.hostptr = (unsigned long)fbmem,
		};
		int ret;
		ret = ioctl(pipe->fd, IOCTL_KGSL_MAP_USER_MEM, &req);
		if (ret) {
			ERROR_MSG("mapping user mem failed: %s",
					strerror(errno));
			goto fail;
		}
		bo->gpuaddr = req.gpuaddr;
		bo->map = fbmem;
	}

	return bo;
fail:
	if (bo)
		fd_bo_del(bo);
	return NULL;
}

struct fd_bo * fd_bo_from_name(struct fd_device *dev, uint32_t name)
{
	struct drm_gem_open req = {
			.name = name,
	};

	if (drmIoctl(dev->fd, DRM_IOCTL_GEM_OPEN, &req)) {
		return NULL;
	}

	return bo_from_handle(dev, req.size, req.handle);
}

struct fd_bo * fd_bo_ref(struct fd_bo *bo)
{
	atomic_inc(&bo->refcnt);
	return bo;
}

void fd_bo_del(struct fd_bo *bo)
{
	if (!atomic_dec_and_test(&bo->refcnt))
		return;

	if (bo->map)
		munmap(bo->map, bo->size);

	if (bo->handle) {
		struct drm_gem_close req = {
				.handle = bo->handle,
		};
		drmIoctl(bo->dev->fd, DRM_IOCTL_GEM_CLOSE, &req);
	}

	free(bo);
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

		bo->name = req.name;
	}

	*name = bo->name;

	return 0;
}

uint32_t fd_bo_handle(struct fd_bo *bo)
{
	return bo->handle;
}

uint32_t fd_bo_size(struct fd_bo *bo)
{
	return bo->size;
}

void * fd_bo_map(struct fd_bo *bo)
{
	if (!bo->map) {
		int ret;

		ret = bo_alloc(bo);
		if (ret) {
			return NULL;
		}

		bo->map = mmap(0, bo->size, PROT_READ | PROT_WRITE, MAP_SHARED,
				bo->dev->fd, bo->offset);
		if (bo->map == MAP_FAILED) {
			ERROR_MSG("mmap failed: %s", strerror(errno));
			bo->map = NULL;
		}
	}
	return bo->map;
}

uint32_t fd_bo_gpuaddr(struct fd_bo *bo, uint32_t offset)
{
	if (!bo->gpuaddr) {
		struct drm_kgsl_gem_bufinfo req = {
				.handle = bo->handle,
		};
		int ret;

		ret = bo_alloc(bo);
		if (ret) {
			return ret;
		}

		ret = drmCommandWriteRead(bo->dev->fd, DRM_KGSL_GEM_GET_BUFINFO,
				&req, sizeof(req));
		if (ret) {
			ERROR_MSG("get bufinfo failed: %s", strerror(errno));
			return 0;
		}

		bo->gpuaddr = req.gpuaddr[0];
	}
	return bo->gpuaddr + offset;
}
