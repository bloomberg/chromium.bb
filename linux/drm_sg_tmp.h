/**
 * \file drm_sg_tmp.h 
 * IOCTLs to manage scatter/gather memory
 *
 * \author Gareth Hughes <gareth@valinux.com>
 */

/*
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
 */


#if __HAVE_SG


#define __NO_VERSION__
#include <linux/config.h>
#include <linux/vmalloc.h>
#include "drmP.h"


/**
 * Define to non-zero to verify that each page points to its virtual address,
 * and vice versa, in sg_alloc().
 */
#define DEBUG_SCATTER 0


/** 
 * Free scatter/gather memory. 
 *
 * \param entry scatter/gather memory entry to free, as returned by sg_alloc().
 */
void DRM(sg_free)( drm_sg_mem_t *entry )
{
	struct page *page;
	int i;

	for ( i = 0 ; i < entry->pages ; i++ ) {
		page = entry->pagelist[i];
		if ( page )
			ClearPageReserved( page );
	}

	vfree( entry->virtual );

	DRM(free)( entry->busaddr,
		   entry->pages * sizeof(*entry->busaddr),
		   DRM_MEM_PAGES );
	DRM(free)( entry->pagelist,
		   entry->pages * sizeof(*entry->pagelist),
		   DRM_MEM_PAGES );
	DRM(free)( entry,
		   sizeof(*entry),
		   DRM_MEM_SGLISTS );
}

/** 
 * Allocate scatter/gather memory. 
 *
 * \param size size of memory to allocate.
 * \return pointer to a drm_sg_mem structure on success or NULL on failure.
 */
drm_sg_mem_t * DRM(sg_alloc)( unsigned long size )
{
	drm_sg_mem_t *entry;
	unsigned long pages, i, j;

	DRM_DEBUG( "%s\n", __FUNCTION__ );

	entry = DRM(alloc)( sizeof(*entry), DRM_MEM_SGLISTS );
	if ( !entry )
		return NULL;

   	memset( entry, 0, sizeof(*entry) );

	pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
	DRM_DEBUG( "sg size=%ld pages=%ld\n", size, pages );

	entry->pages = pages;
	entry->pagelist = DRM(alloc)( pages * sizeof(*entry->pagelist),
				     DRM_MEM_PAGES );
	if ( !entry->pagelist ) {
		DRM(free)( entry, sizeof(*entry), DRM_MEM_SGLISTS );
		return NULL;
	}

	memset(entry->pagelist, 0, pages * sizeof(*entry->pagelist));

	entry->busaddr = DRM(alloc)( pages * sizeof(*entry->busaddr),
				     DRM_MEM_PAGES );
	if ( !entry->busaddr ) {
		DRM(free)( entry->pagelist,
			   entry->pages * sizeof(*entry->pagelist),
			   DRM_MEM_PAGES );
		DRM(free)( entry,
			   sizeof(*entry),
			   DRM_MEM_SGLISTS );
		return NULL;
	}
	memset( (void *)entry->busaddr, 0, pages * sizeof(*entry->busaddr) );

	entry->virtual = vmalloc_32( pages << PAGE_SHIFT );
	if ( !entry->virtual ) {
		DRM(free)( entry->busaddr,
			   entry->pages * sizeof(*entry->busaddr),
			   DRM_MEM_PAGES );
		DRM(free)( entry->pagelist,
			   entry->pages * sizeof(*entry->pagelist),
			   DRM_MEM_PAGES );
		DRM(free)( entry,
			   sizeof(*entry),
			   DRM_MEM_SGLISTS );
		return NULL;
	}

	/* This also forces the mapping of COW pages, so our page list
	 * will be valid.  Please don't remove it...
	 */
	memset( entry->virtual, 0, pages << PAGE_SHIFT );

	entry->handle = (unsigned long)entry->virtual;

	DRM_DEBUG( "sg alloc handle  = %08lx\n", entry->handle );
	DRM_DEBUG( "sg alloc virtual = %p\n", entry->virtual );

	for ( i = entry->handle, j = 0 ; j < pages ; i += PAGE_SIZE, j++ ) {
		entry->pagelist[j] = vmalloc_to_page((void *)i);
		if (!entry->pagelist[j])
			goto failed;
		SetPageReserved(entry->pagelist[j]);
	}

#if DEBUG_SCATTER
	{
	int error = 0;

	for ( i = 0 ; i < pages ; i++ ) {
		unsigned long *tmp;

		tmp = page_address( entry->pagelist[i] );
		for ( j = 0 ;
		      j < PAGE_SIZE / sizeof(unsigned long) ;
		      j++, tmp++ ) {
			*tmp = 0xcafebabe;
		}
		tmp = (unsigned long *)((u8 *)entry->virtual +
					(PAGE_SIZE * i));
		for( j = 0 ;
		     j < PAGE_SIZE / sizeof(unsigned long) ;
		     j++, tmp++ ) {
			if ( *tmp != 0xcafebabe && error == 0 ) {
				error = 1;
				DRM_ERROR( "Scatter allocation error, "
					   "pagelist does not match "
					   "virtual mapping\n" );
			}
		}
		tmp = page_address( entry->pagelist[i] );
		for(j = 0 ;
		    j < PAGE_SIZE / sizeof(unsigned long) ;
		    j++, tmp++) {
			*tmp = 0;
		}
	}
	if (error == 0)
		DRM_ERROR( "Scatter allocation matches pagelist\n" );
	}
#endif

	return entry;

 failed:
	DRM(sg_free)( entry );
	return NULL;
}


/**********************************************************************/
/** \name Ioctl's 
 *
 * These expose the scatter/gather memory functions to user-space, 
 * doing the necessary parameter verification and book-keeping to avoid memory
 * leaks.
 *
 * In the current implementation only one scatter/gather memory block can be
 * allocated per device at a given instance.
 *
 * The information about the currently allocated scatter/gather memory block is
 * stored in drm_device_t::sg.
 */
/*@{*/

/** 
 * Wrapper ioctl around sg_alloc().
 * 
 * \param inode device inode.
 * \param filp file pointer.
 * \param cmd command.
 * \param arg user argument, pointing to a drm_scatter_gather structure.
 * \return zero on success or a negative number on failure.
 */
int DRM(sg_alloc_ioctl)( struct inode *inode, struct file *filp,
		   unsigned int cmd, unsigned long arg )
{
	drm_file_t *priv = filp->private_data;
	drm_device_t *dev = priv->dev;
	drm_scatter_gather_t request;
	drm_sg_mem_t *entry;

	DRM_DEBUG( "%s\n", __FUNCTION__ );

	if ( dev->sg )
		return -EINVAL;

	if ( copy_from_user( &request,
			     (drm_scatter_gather_t *)arg,
			     sizeof(request) ) )
		return -EFAULT;

	entry = DRM(sg_alloc)( request.size  );
	if ( !entry )
		return -ENOMEM;

	request.handle = entry->handle;

	if ( copy_to_user( (drm_scatter_gather_t *)arg,
			   &request,
			   sizeof(request) ) ) {
		DRM(sg_free)( entry );
		return -EFAULT;
	}

	dev->sg = entry;

	return 0;
}

/** Wrapper around sg_free().
 * 
 * \param inode device inode.
 * \param filp file pointer.
 * \param cmd command.
 * \param arg user argument, pointing to a drm_scatter_gather structure.
 * \return zero on success or a negative number on failure.
 */
int DRM(sg_free_ioctl)( struct inode *inode, struct file *filp,
		 unsigned int cmd, unsigned long arg )
{
	drm_file_t *priv = filp->private_data;
	drm_device_t *dev = priv->dev;
	drm_scatter_gather_t request;
	drm_sg_mem_t *entry;

	if ( copy_from_user( &request,
			     (drm_scatter_gather_t *)arg,
			     sizeof(request) ) )
		return -EFAULT;

	entry = dev->sg;
	dev->sg = NULL;

	if ( !entry || entry->handle != request.handle )
		return -EINVAL;

	DRM_DEBUG( "sg free virtual  = %p\n", entry->virtual );

	DRM(sg_free)( entry );

	return 0;
}

/*@}*/


/**
 * Called by takedown() to free the scatter/gather memory resources associated
 * with a given device, i.e., to free drm_device_t::sg.
 */
void DRM(sg_cleanup)(drm_device_t *dev)
{
	if ( dev->sg ) {
		DRM(sg_free)( dev->sg );
		dev->sg = NULL;
	}
}


#endif
