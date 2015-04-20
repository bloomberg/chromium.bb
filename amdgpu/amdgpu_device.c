/*
 * Copyright 2014 Advanced Micro Devices, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
*/

/**
 * \file amdgpu_device.c
 *
 *  Implementation of functions for AMD GPU device
 *
 *
 */

#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "xf86drm.h"
#include "amdgpu_drm.h"
#include "amdgpu_internal.h"
#include "util_hash_table.h"

#define PTR_TO_UINT(x) ((unsigned)((intptr_t)(x)))
#define UINT_TO_PTR(x) ((void *)((intptr_t)(x)))
#define RENDERNODE_MINOR_MASK 0xff7f

pthread_mutex_t fd_mutex = PTHREAD_MUTEX_INITIALIZER;
static struct util_hash_table *fd_tab;

static unsigned handle_hash(void *key)
{
	return PTR_TO_UINT(key);
}

static int handle_compare(void *key1, void *key2)
{
	return PTR_TO_UINT(key1) != PTR_TO_UINT(key2);
}

static unsigned fd_hash(void *key)
{
	int fd = PTR_TO_UINT(key);
	struct stat stat;
	fstat(fd, &stat);

	if (!S_ISCHR(stat.st_mode))
		return stat.st_dev ^ stat.st_ino;
	else
		return stat.st_dev ^ (stat.st_rdev & RENDERNODE_MINOR_MASK);
}

static int fd_compare(void *key1, void *key2)
{
	int fd1 = PTR_TO_UINT(key1);
	int fd2 = PTR_TO_UINT(key2);
	struct stat stat1, stat2;
	fstat(fd1, &stat1);
	fstat(fd2, &stat2);

	if (!S_ISCHR(stat1.st_mode) || !S_ISCHR(stat2.st_mode))
		return stat1.st_dev != stat2.st_dev ||
			stat1.st_ino != stat2.st_ino;
        else
		return major(stat1.st_rdev) != major(stat2.st_rdev) ||
			(minor(stat1.st_rdev) & RENDERNODE_MINOR_MASK) !=
			(minor(stat2.st_rdev) & RENDERNODE_MINOR_MASK);
}

/**
* Get the authenticated form fd,
*
* \param   fd   - \c [in]  File descriptor for AMD GPU device
* \param   auth - \c [out] Pointer to output the fd is authenticated or not
*                          A render node fd, output auth = 0
*                          A legacy fd, get the authenticated for compatibility root
*
* \return   0 on success\n
*          >0 - AMD specific error code\n
*          <0 - Negative POSIX Error code
*/
static int amdgpu_get_auth(int fd, int *auth)
{
	int r = 0;
	drm_client_t client;

	if (drmGetNodeTypeFromFd(fd) == DRM_NODE_RENDER)
		*auth = 0;
	else {
		client.idx = 0;
		r = drmIoctl(fd, DRM_IOCTL_GET_CLIENT, &client);
		if (!r)
			*auth = client.auth;
	}
	return r;
}

int amdgpu_device_initialize(int fd,
			     uint32_t *major_version,
			     uint32_t *minor_version,
			     amdgpu_device_handle *device_handle)
{
	struct amdgpu_device *dev;
	drmVersionPtr version;
	int r;
	int flag_auth = 0;
	int flag_authexist=0;
	uint32_t accel_working;

	*device_handle = NULL;

	pthread_mutex_lock(&fd_mutex);
	if (!fd_tab)
		fd_tab = util_hash_table_create(fd_hash, fd_compare);
	r = amdgpu_get_auth(fd, &flag_auth);
	if (r) {
		pthread_mutex_unlock(&fd_mutex);
		return r;
	}
	dev = util_hash_table_get(fd_tab, UINT_TO_PTR(fd));
	if (dev) {
		r = amdgpu_get_auth(dev->fd, &flag_authexist);
		if (r) {
			pthread_mutex_unlock(&fd_mutex);
			return r;
		}
		if ((flag_auth) && (!flag_authexist)) {
			dev->flink_fd = fd;
		}
		*major_version = dev->major_version;
		*minor_version = dev->minor_version;
		amdgpu_device_reference(device_handle, dev);
		pthread_mutex_unlock(&fd_mutex);
		return 0;
	}

	dev = calloc(1, sizeof(struct amdgpu_device));
	if (!dev) {
		pthread_mutex_unlock(&fd_mutex);
		return -ENOMEM;
	}

	atomic_set(&dev->refcount, 1);

	version = drmGetVersion(fd);
	if (version->version_major != 3) {
		fprintf(stderr, "%s: DRM version is %d.%d.%d but this driver is "
			"only compatible with 3.x.x.\n",
			__func__,
			version->version_major,
			version->version_minor,
			version->version_patchlevel);
		drmFreeVersion(version);
		r = -EBADF;
		goto cleanup;
	}

	dev->fd = fd;
	dev->flink_fd = fd;
	dev->major_version = version->version_major;
	dev->minor_version = version->version_minor;
	drmFreeVersion(version);

	dev->bo_flink_names = util_hash_table_create(handle_hash,
						     handle_compare);
	dev->bo_handles = util_hash_table_create(handle_hash, handle_compare);
	dev->bo_vas = util_hash_table_create(handle_hash, handle_compare);
	pthread_mutex_init(&dev->bo_table_mutex, NULL);

	/* Check if acceleration is working. */
	r = amdgpu_query_info(dev, AMDGPU_INFO_ACCEL_WORKING, 4, &accel_working);
	if (r)
		goto cleanup;
	if (!accel_working) {
		r = -EBADF;
		goto cleanup;
	}

	r = amdgpu_query_gpu_info_init(dev);
	if (r)
		goto cleanup;

	amdgpu_vamgr_init(dev);

	*major_version = dev->major_version;
	*minor_version = dev->minor_version;
	*device_handle = dev;
	util_hash_table_set(fd_tab, UINT_TO_PTR(fd), dev);
	pthread_mutex_unlock(&fd_mutex);

	return 0;

cleanup:
	free(dev);
	pthread_mutex_unlock(&fd_mutex);
	return r;
}

void amdgpu_device_free_internal(amdgpu_device_handle dev)
{
	util_hash_table_destroy(dev->bo_flink_names);
	util_hash_table_destroy(dev->bo_handles);
	util_hash_table_destroy(dev->bo_vas);
	pthread_mutex_destroy(&dev->bo_table_mutex);
	pthread_mutex_destroy(&(dev->vamgr.bo_va_mutex));
	util_hash_table_remove(fd_tab, UINT_TO_PTR(dev->fd));
	free(dev);
}

int amdgpu_device_deinitialize(amdgpu_device_handle dev)
{
	amdgpu_device_reference(&dev, NULL);
	return 0;
}

void amdgpu_device_reference(struct amdgpu_device **dst,
			     struct amdgpu_device *src)
{
	if (update_references(&(*dst)->refcount, &src->refcount))
		amdgpu_device_free_internal(*dst);
	*dst = src;
}
