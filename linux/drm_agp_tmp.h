/**
 * \file drm_agp_tmp.h 
 * DRM support for AGP/GART backend
 *    
 * \author Rickard E. (Rik) Faith <faith@valinux.com>
 * \author Gareth Hughes <gareth@valinux.com>
 */

/*
 * Copyright 1999 Precision Insight, Inc., Cedar Park, Texas.
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
 * VA LINUX SYSTEMS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */



#if __REALLY_HAVE_AGP

#define __NO_VERSION__
#include "drmP.h"
#include <linux/module.h>


/**
 * Pointer to the drm_agp_t structure made available by the AGPGART module.
 */
static const drm_agp_t *drm_agp = NULL;


/**********************************************************************/
/** 
 * \name drm_agp wrappers
 *
 * These functions are thin wrappers around the respective methods in drm_agp
 * which are exposed by the AGPGART module.
 *
 */
/* TODO: Put these in a header inline. */
/*@{*/

/**
 * Acquire the AGP device.
 */
int DRM(agp_acquire)(void)
{
	return drm_agp->acquire();
}

/**
 * Release the AGP device.
 */
void DRM(agp_release)(void)
{
	if (drm_agp->release)
		drm_agp->release();
}

/**
 * Enable the AGP bus.
 */
void DRM(agp_enable)(unsigned long mode)
{
	if (drm_agp->enable)
		drm_agp->enable(mode);
}

/** 
 * Allocate AGP memory.
 * 
 * Not meant to be called directly.  Use agp_alloc() instead, which can provide
 * some debugging features.
 */
agp_memory *DRM(agp_allocate_memory)(size_t pages, u32 type)
{
	if (!drm_agp->allocate_memory) 
		return NULL;
	return drm_agp->allocate_memory(pages, type);
}

/** 
 * Free AGP memory.
 * 
 * Not meant to be called directly.  Use agp_free() instead, which can provide
 * some debugging features.
 */
int DRM(agp_free_memory)(agp_memory *handle)
{
	if (!handle || !drm_agp->free_memory) 
		return 0;
	drm_agp->free_memory(handle);
	return 1;
}

/** 
 * Bind AGP memory.
 *
 * Not meant to be called directly.  Use agp_bind() instead, which can provide
 * some debugging features.
 */
int DRM(agp_bind_memory)(agp_memory *handle, off_t start)
{
	if (!handle || !drm_agp->bind_memory) 
		return -EINVAL;
	return drm_agp->bind_memory(handle, start);
}

/** 
 * Bind AGP memory.
 *
 * Not meant to be called directly.  Use agp_unbind() instead, which can provide
 * some debugging features.
 */
int DRM(agp_unbind_memory)(agp_memory *handle)
{
	if (!handle || !drm_agp->unbind_memory)
		return -EINVAL;
	return drm_agp->unbind_memory(handle);
}

/**@}*/


/**********************************************************************/
/** \name Ioctl's */
/*@{*/

/**
 * Acquire the AGP device.
 *
 * \param inode device inode.
 * \param filp file pointer.
 * \param cmd command.
 * \param arg user argument.
 * \return zero on success or a negative number on failure. 
 *
 * Verifies the AGP device hasn't been acquired before and calls
 * drm_acquire().
 */
int DRM(agp_acquire_ioctl)(struct inode *inode, struct file *filp,
		     unsigned int cmd, unsigned long arg)
{
	drm_file_t	 *priv	 = filp->private_data;
	drm_device_t	 *dev	 = priv->dev;
	int              retcode;

	if (!dev->agp || dev->agp->acquired || !drm_agp->acquire)
		return -EINVAL;
	if ((retcode = DRM(agp_acquire)())) return retcode;
	dev->agp->acquired = 1;
	return 0;
}

/**
 * Release the AGP device.
 *
 * \param inode device inode.
 * \param filp file pointer.
 * \param cmd command.
 * \param arg user argument.
 * \return zero on success or a negative number on failure.
 *
 * Verifies the AGP device has been acquired and calls drm_agp->release().
 */
int DRM(agp_release_ioctl)(struct inode *inode, struct file *filp,
		     unsigned int cmd, unsigned long arg)
{
	drm_file_t	 *priv	 = filp->private_data;
	drm_device_t	 *dev	 = priv->dev;

	if (!dev->agp || !dev->agp->acquired || !drm_agp->release)
		return -EINVAL;
	drm_agp->release();
	dev->agp->acquired = 0;
	return 0;

}

/**
 * Get AGP information.
 *
 * \param inode device inode.
 * \param filp file pointer.
 * \param cmd command.
 * \param arg pointer to an (output) drm_agp_info structure.
 * \return zero on success or a negative number on failure.
 *
 * Verifies the AGP device has been initialized and acquired and fills in the
 * drm_agp_info structure with the information in drm_agp_head::agp_info.
 */
int DRM(agp_info_ioctl)(struct inode *inode, struct file *filp,
		  unsigned int cmd, unsigned long arg)
{
	drm_file_t	 *priv	 = filp->private_data;
	drm_device_t	 *dev	 = priv->dev;
	agp_kern_info    *kern;
	drm_agp_info_t   info;

	if (!dev->agp || !dev->agp->acquired || !drm_agp->copy_info)
		return -EINVAL;

	kern                   = &dev->agp->agp_info;
	info.agp_version_major = kern->version.major;
	info.agp_version_minor = kern->version.minor;
	info.mode              = kern->mode;
	info.aperture_base     = kern->aper_base;
	info.aperture_size     = kern->aper_size * 1024 * 1024;
	info.memory_allowed    = kern->max_memory << PAGE_SHIFT;
	info.memory_used       = kern->current_memory << PAGE_SHIFT;
	info.id_vendor         = kern->device->vendor;
	info.id_device         = kern->device->device;

	if (copy_to_user((drm_agp_info_t *)arg, &info, sizeof(info)))
		return -EFAULT;
	return 0;
}

/**
 * Enable the AGP bus.
 * 
 * \param inode device inode.
 * \param filp file pointer.
 * \param cmd command.
 * \param arg pointer to a drm_agp_mode structure.
 * \return zero on success or a negative number on failure.
 *
 * Verifies the AGP device has been acquired, and calls
 * agp_enable().
 */
int DRM(agp_enable_ioctl)(struct inode *inode, struct file *filp,
		    unsigned int cmd, unsigned long arg)
{
	drm_file_t	 *priv	 = filp->private_data;
	drm_device_t	 *dev	 = priv->dev;
	drm_agp_mode_t   mode;

	if (!dev->agp || !dev->agp->acquired || !drm_agp->enable)
		return -EINVAL;

	if (copy_from_user(&mode, (drm_agp_mode_t *)arg, sizeof(mode)))
		return -EFAULT;

	dev->agp->mode    = mode.mode;
	DRM(agp_enable)(mode.mode);
	dev->agp->base    = dev->agp->agp_info.aper_base;
	dev->agp->enabled = 1;
	return 0;
}

/**
 * Allocate AGP memory.
 *
 * \param inode device inode.
 * \param filp file pointer.
 * \param cmd command.
 * \param arg pointer to a drm_agp_buffer structure.
 * \return zero on success or a negative number on failure.
 * 
 * Verifies the AGP device is present and has been acquired, allocates the
 * memory via agp_alloc() and creates a drm_agp_mem entry for it.
 */
int DRM(agp_alloc_ioctl)(struct inode *inode, struct file *filp,
		   unsigned int cmd, unsigned long arg)
{
	drm_file_t	 *priv	 = filp->private_data;
	drm_device_t	 *dev	 = priv->dev;
	drm_agp_buffer_t request;
	drm_agp_mem_t    *entry;
	agp_memory       *memory;
	unsigned long    pages;
	u32 		 type;

	if (!dev->agp || !dev->agp->acquired) return -EINVAL;
	if (copy_from_user(&request, (drm_agp_buffer_t *)arg, sizeof(request)))
		return -EFAULT;
	if (!(entry = DRM(alloc)(sizeof(*entry), DRM_MEM_AGPLISTS)))
		return -ENOMEM;

   	memset(entry, 0, sizeof(*entry));

	pages = (request.size + PAGE_SIZE - 1) / PAGE_SIZE;
	type = (u32) request.type;

	if (!(memory = DRM(agp_alloc)(pages, type))) {
		DRM(free)(entry, sizeof(*entry), DRM_MEM_AGPLISTS);
		return -ENOMEM;
	}

	entry->handle    = (unsigned long)memory->key;
	entry->memory    = memory;
	entry->bound     = 0;
	entry->pages     = pages;
	entry->prev      = NULL;
	entry->next      = dev->agp->memory;
	if (dev->agp->memory) dev->agp->memory->prev = entry;
	dev->agp->memory = entry;

	request.handle   = entry->handle;
        request.physical = memory->physical;

	if (copy_to_user((drm_agp_buffer_t *)arg, &request, sizeof(request))) {
		dev->agp->memory       = entry->next;
		dev->agp->memory->prev = NULL;
		DRM(agp_free)(memory, pages);
		DRM(free)(entry, sizeof(*entry), DRM_MEM_AGPLISTS);
		return -EFAULT;
	}
	return 0;
}

/**
 * Search for the AGP memory entry associated with a handle.
 *
 * \param dev DRM device structure.
 * \param handle AGP memory handle.
 * \return pointer to the drm_agp_mem structure associated with \p handle.
 * 
 * Walks through drm_agp_head::memory until finding a matching handle.
 */
static drm_agp_mem_t *DRM(agp_lookup_entry)(drm_device_t *dev,
					    unsigned long handle)
{
	drm_agp_mem_t *entry;

	for (entry = dev->agp->memory; entry; entry = entry->next) {
		if (entry->handle == handle) return entry;
	}
	return NULL;
}

/**
 * Unbind AGP memory from the GATT.
 *
 * \param inode device inode.
 * \param filp file pointer.
 * \param cmd command.
 * \param arg pointer to a drm_agp_binding structure.
 * \return zero on success or a negative number on failure.
 *
 * Verifies the AGP device is present and acquired, looks-up the AGP memory
 * entry and passes it to the agp_unbind() function.
 */
int DRM(agp_unbind_ioctl)(struct inode *inode, struct file *filp,
		    unsigned int cmd, unsigned long arg)
{
	drm_file_t	  *priv	 = filp->private_data;
	drm_device_t	  *dev	 = priv->dev;
	drm_agp_binding_t request;
	drm_agp_mem_t     *entry;
	int ret;

	if (!dev->agp || !dev->agp->acquired) return -EINVAL;
	if (copy_from_user(&request, (drm_agp_binding_t *)arg, sizeof(request)))
		return -EFAULT;
	if (!(entry = DRM(agp_lookup_entry)(dev, request.handle)))
		return -EINVAL;
	if (!entry->bound) return -EINVAL;
	ret = DRM(agp_unbind)(entry->memory);
	if (ret == 0)
	    entry->bound = 0;
	return ret;
}

/**
 * Bind AGP memory into the GATT
 *
 * \param inode device inode.
 * \param filp file pointer.
 * \param cmd command.
 * \param arg pointer to a drm_agp_binding structure.
 * \return zero on success or a negative number on failure.
 *
 * Verifies the AGP device is present and has been acquired and that no memory
 * is currently bound into the GATT. Looks-up the AGP memory entry and passes
 * it to agp_bind() function.
 */
int DRM(agp_bind_ioctl)(struct inode *inode, struct file *filp,
		  unsigned int cmd, unsigned long arg)
{
	drm_file_t	  *priv	 = filp->private_data;
	drm_device_t	  *dev	 = priv->dev;
	drm_agp_binding_t request;
	drm_agp_mem_t     *entry;
	int               retcode;
	int               page;

	if (!dev->agp || !dev->agp->acquired || !drm_agp->bind_memory)
		return -EINVAL;
	if (copy_from_user(&request, (drm_agp_binding_t *)arg, sizeof(request)))
		return -EFAULT;
	if (!(entry = DRM(agp_lookup_entry)(dev, request.handle)))
		return -EINVAL;
	if (entry->bound) return -EINVAL;
	page = (request.offset + PAGE_SIZE - 1) / PAGE_SIZE;
	if ((retcode = DRM(agp_bind)(entry->memory, page))) return retcode;
	entry->bound = dev->agp->base + (page << PAGE_SHIFT);
	DRM_DEBUG("base = 0x%lx entry->bound = 0x%lx\n",
		  dev->agp->base, entry->bound);
	return 0;
}

/**
 * Free AGP memory.
 *
 * \param inode device inode.
 * \param filp file pointer.
 * \param cmd command.
 * \param arg pointer to a drm_agp_buffer structure.
 * \return zero on success or a negative number on failure.
 *
 * Verifies the AGP device is present and has been acquired and looks up the
 * AGP memory entry. If the memory it's currently bound, unbind it via
 * agp_unbind(). Frees it via agp_free() as well as the entry itself
 * and unlinks from the doubly linked list it's inserted in.
 */
int DRM(agp_free_ioctl)(struct inode *inode, struct file *filp,
		  unsigned int cmd, unsigned long arg)
{
	drm_file_t	 *priv	 = filp->private_data;
	drm_device_t	 *dev	 = priv->dev;
	drm_agp_buffer_t request;
	drm_agp_mem_t    *entry;

	if (!dev->agp || !dev->agp->acquired) return -EINVAL;
	if (copy_from_user(&request, (drm_agp_buffer_t *)arg, sizeof(request)))
		return -EFAULT;
	if (!(entry = DRM(agp_lookup_entry)(dev, request.handle)))
		return -EINVAL;
	if (entry->bound) DRM(agp_unbind)(entry->memory);

	if (entry->prev) entry->prev->next = entry->next;
	else             dev->agp->memory  = entry->next;
	if (entry->next) entry->next->prev = entry->prev;
	DRM(agp_free)(entry->memory, entry->pages);
	DRM(free)(entry, sizeof(*entry), DRM_MEM_AGPLISTS);
	return 0;
}

/*@}*/


/**********************************************************************/
/** \name Initialization and cleanup */
/*@{*/

/**
 * Initialize the global AGP resources.
 *
 * Gets the drm_agp_t structure which is made available by the agpgart module
 * via the inter_module_* functions.
 */
void DRM(agp_init)(void)
{
	drm_agp = (drm_agp_t *)inter_module_get("drm_agp");
}

/**
 * Free the global AGP resources.
 *
 * Releases the pointer in ::drm_agp.
 */
void DRM(agp_cleanup)(void)
{
	if (drm_agp) {
		inter_module_put("drm_agp");
		drm_agp = NULL;
	}
}

/**
 * Initialize the device AGP resources.
 *
 * Creates and initializes a drm_agp_head structure in drm_device_t::agp.
 */
void DRM(agp_init_dev)(drm_device_t *dev)
{
	drm_agp_head_t *head         = NULL;

	if (!drm_agp)
		return;

	if (!(head = DRM(alloc)(sizeof(*head), DRM_MEM_AGPLISTS)))
		return;

	memset((void *)head, 0, sizeof(*head));
	drm_agp->copy_info(&head->agp_info);
	if (head->agp_info.chipset == NOT_SUPPORTED) {
		DRM(free)(head, sizeof(*head), DRM_MEM_AGPLISTS);
		return;
	}
	head->memory = NULL;
#if LINUX_VERSION_CODE <= 0x020408
	head->cant_use_aperture = 0;
	head->page_mask = ~(0xfff);
#else
	head->cant_use_aperture = head->agp_info.cant_use_aperture;
	head->page_mask = head->agp_info.page_mask;
#endif

	DRM_INFO("AGP %d.%d aperture @ 0x%08lx %ZuMB\n",
		 head->agp_info.version.major,
		 head->agp_info.version.minor,
		 head->agp_info.aper_base,
		 head->agp_info.aper_size);

	dev->agp = head;
}

/**
 * Free the device AGP resources.
 */
void DRM(agp_cleanup_dev)(drm_device_t *dev)
{
	if ( dev->agp ) {
		drm_agp_mem_t *entry;
		drm_agp_mem_t *nexte;

				/* Remove AGP resources, but leave dev->agp
                                   intact until drv_cleanup is called. */
		for ( entry = dev->agp->memory ; entry ; entry = nexte ) {
			nexte = entry->next;
			if ( entry->bound ) DRM(agp_unbind)( entry->memory );
			DRM(agp_free)( entry->memory, entry->pages );
			DRM(free)( entry, sizeof(*entry), DRM_MEM_AGPLISTS );
		}
		dev->agp->memory = NULL;

		if ( dev->agp->acquired )
			DRM(agp_release)();

		dev->agp->acquired = 0;
		dev->agp->enabled  = 0;

		DRM(free)( dev->agp, sizeof(*dev->agp), DRM_MEM_AGPLISTS );
		dev->agp = NULL;
	}
}

/*@}*/


#endif /* __REALLY_HAVE_AGP */
