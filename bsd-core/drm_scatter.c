/* drm_scatter.h -- IOCTLs to manage scatter/gather memory -*- linux-c -*-
 * Created: Mon Dec 18 23:20:54 2000 by gareth@valinux.com
 *
 * Copyright 2000 VA Linux Systems, Inc., Sunnyvale, California.
 * All Rights Reserved.
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
 * PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *   Gareth Hughes <gareth@valinux.com>
 *   Eric Anholt <anholt@FreeBSD.org>
 *
 */

#include "drmP.h"

#define DEBUG_SCATTER 0

void drm_sg_cleanup(drm_sg_mem_t *entry)
{
	free(entry->virtual, M_DRM);

	free(entry->busaddr, M_DRM);
	free(entry, M_DRM);
}

int drm_sg_alloc(DRM_IOCTL_ARGS)
{
	DRM_DEVICE;
	drm_scatter_gather_t request;
	drm_sg_mem_t *entry;
	unsigned long pages;

	DRM_DEBUG( "%s\n", __FUNCTION__ );

	if ( dev->sg )
		return EINVAL;

	DRM_COPY_FROM_USER_IOCTL(request, (drm_scatter_gather_t *)data,
			     sizeof(request) );

	entry = malloc(sizeof(*entry), M_DRM, M_NOWAIT | M_ZERO);
	if ( !entry )
		return ENOMEM;

	pages = round_page(request.size) / PAGE_SIZE;
	DRM_DEBUG( "sg size=%ld pages=%ld\n", request.size, pages );

	entry->pages = pages;

	entry->busaddr = malloc(pages * sizeof(*entry->busaddr), M_DRM,
	    M_NOWAIT | M_ZERO);
	if ( !entry->busaddr ) {
		free(entry, M_DRM);
		return ENOMEM;
	}

	entry->virtual = malloc(pages << PAGE_SHIFT, M_DRM, M_WAITOK | M_ZERO);
	if ( !entry->virtual ) {
		free(entry->busaddr, M_DRM);
		free(entry, M_DRM);
		return ENOMEM;
	}

	entry->handle = (unsigned long)entry->virtual;

	DRM_DEBUG( "sg alloc handle  = %08lx\n", entry->handle );
	DRM_DEBUG( "sg alloc virtual = %p\n", entry->virtual );

	request.handle = entry->handle;

	DRM_COPY_TO_USER_IOCTL( (drm_scatter_gather_t *)data,
			   request,
			   sizeof(request) );

	dev->sg = entry;

	return 0;

	drm_sg_cleanup(entry);
	return ENOMEM;
}

int drm_sg_free(DRM_IOCTL_ARGS)
{
	DRM_DEVICE;
	drm_scatter_gather_t request;
	drm_sg_mem_t *entry;

	DRM_COPY_FROM_USER_IOCTL( request, (drm_scatter_gather_t *)data,
			     sizeof(request) );

	entry = dev->sg;
	dev->sg = NULL;

	if ( !entry || entry->handle != request.handle )
		return EINVAL;

	DRM_DEBUG( "sg free virtual  = %p\n", entry->virtual );

	drm_sg_cleanup(entry);

	return 0;
}
