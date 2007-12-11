/**
 * \file drm_agpsupport.c
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

#include "drmP.h"
#include <linux/module.h>

#if __OS_HAS_AGP

/**
 * Get AGP information.
 *
 * \param inode device inode.
 * \param file_priv DRM file private.
 * \param cmd command.
 * \param arg pointer to a (output) drm_agp_info structure.
 * \return zero on success or a negative number on failure.
 *
 * Verifies the AGP device has been initialized and acquired and fills in the
 * drm_agp_info structure with the information in drm_agp_head::agp_info.
 */
int drm_agp_info(struct drm_device *dev, struct drm_agp_info *info)
{
	DRM_AGP_KERN *kern;

	if (!dev->agp || !dev->agp->acquired)
		return -EINVAL;

	kern = &dev->agp->agp_info;
	info->agp_version_major = kern->version.major;
	info->agp_version_minor = kern->version.minor;
	info->mode = kern->mode;
	info->aperture_base = kern->aper_base;
	info->aperture_size = kern->aper_size * 1024 * 1024;
	info->memory_allowed = kern->max_memory << PAGE_SHIFT;
	info->memory_used = kern->current_memory << PAGE_SHIFT;
	info->id_vendor = kern->device->vendor;
	info->id_device = kern->device->device;

	return 0;
}
EXPORT_SYMBOL(drm_agp_info);

int drm_agp_info_ioctl(struct drm_device *dev, void *data,
		       struct drm_file *file_priv)
{
	struct drm_agp_info *info = data;
	int err;

	err = drm_agp_info(dev, info);
	if (err)
		return err;

	return 0;
}

/**
 * Acquire the AGP device.
 *
 * \param dev DRM device that is to acquire AGP.
 * \return zero on success or a negative number on failure.
 *
 * Verifies the AGP device hasn't been acquired before and calls
 * \c agp_backend_acquire.
 */
int drm_agp_acquire(struct drm_device * dev)
{
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,11)
	int retcode;
#endif

	if (!dev->agp)
		return -ENODEV;
	if (dev->agp->acquired)
		return -EBUSY;
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,11)
	if ((retcode = agp_backend_acquire()))
		return retcode;
#else
	if (!(dev->agp->bridge = agp_backend_acquire(dev->pdev)))
		return -ENODEV;
#endif

	dev->agp->acquired = 1;
	return 0;
}
EXPORT_SYMBOL(drm_agp_acquire);

/**
 * Acquire the AGP device (ioctl).
 *
 * \param inode device inode.
 * \param file_priv DRM file private.
 * \param cmd command.
 * \param arg user argument.
 * \return zero on success or a negative number on failure.
 *
 * Verifies the AGP device hasn't been acquired before and calls
 * \c agp_backend_acquire.
 */
int drm_agp_acquire_ioctl(struct drm_device *dev, void *data,
			  struct drm_file *file_priv)
{
	return drm_agp_acquire((struct drm_device *) file_priv->head->dev);
}

/**
 * Release the AGP device.
 *
 * \param dev DRM device that is to release AGP.
 * \return zero on success or a negative number on failure.
 *
 * Verifies the AGP device has been acquired and calls \c agp_backend_release.
 */
int drm_agp_release(struct drm_device *dev)
{
	if (!dev->agp || !dev->agp->acquired)
		return -EINVAL;
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,11)
	agp_backend_release();
#else
	agp_backend_release(dev->agp->bridge);
#endif
	dev->agp->acquired = 0;
	return 0;

}
EXPORT_SYMBOL(drm_agp_release);

int drm_agp_release_ioctl(struct drm_device *dev, void *data,
			  struct drm_file *file_priv)
{
	return drm_agp_release(dev);
}

/**
 * Enable the AGP bus.
 *
 * \param dev DRM device that has previously acquired AGP.
 * \param mode Requested AGP mode.
 * \return zero on success or a negative number on failure.
 *
 * Verifies the AGP device has been acquired but not enabled, and calls
 * \c agp_enable.
 */
int drm_agp_enable(struct drm_device *dev, struct drm_agp_mode mode)
{
	if (!dev->agp || !dev->agp->acquired)
		return -EINVAL;

	dev->agp->mode = mode.mode;
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,11)
	agp_enable(mode.mode);
#else
	agp_enable(dev->agp->bridge, mode.mode);
#endif
	dev->agp->enabled = 1;
	return 0;
}
EXPORT_SYMBOL(drm_agp_enable);

int drm_agp_enable_ioctl(struct drm_device *dev, void *data,
			 struct drm_file *file_priv)
{
	struct drm_agp_mode *mode = data;

	return drm_agp_enable(dev, *mode);
}

/**
 * Allocate AGP memory.
 *
 * \param inode device inode.
 * \param file_priv file private pointer.
 * \param cmd command.
 * \param arg pointer to a drm_agp_buffer structure.
 * \return zero on success or a negative number on failure.
 *
 * Verifies the AGP device is present and has been acquired, allocates the
 * memory via alloc_agp() and creates a drm_agp_mem entry for it.
 */
int drm_agp_alloc(struct drm_device *dev, struct drm_agp_buffer *request)
{
	struct drm_agp_mem *entry;
	DRM_AGP_MEM *memory;
	unsigned long pages;
	u32 type;

	if (!dev->agp || !dev->agp->acquired)
		return -EINVAL;
	if (!(entry = drm_alloc(sizeof(*entry), DRM_MEM_AGPLISTS)))
		return -ENOMEM;

	memset(entry, 0, sizeof(*entry));

	pages = (request->size + PAGE_SIZE - 1) / PAGE_SIZE;
	type = (u32) request->type;
	if (!(memory = drm_alloc_agp(dev, pages, type))) {
		drm_free(entry, sizeof(*entry), DRM_MEM_AGPLISTS);
		return -ENOMEM;
	}

	entry->handle = (unsigned long)memory->key + 1;
	entry->memory = memory;
	entry->bound = 0;
	entry->pages = pages;
	list_add(&entry->head, &dev->agp->memory);

	request->handle = entry->handle;
	request->physical = memory->physical;

	return 0;
}
EXPORT_SYMBOL(drm_agp_alloc);


int drm_agp_alloc_ioctl(struct drm_device *dev, void *data,
			struct drm_file *file_priv)
{
	struct drm_agp_buffer *request = data;

	return drm_agp_alloc(dev, request);
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
static struct drm_agp_mem *drm_agp_lookup_entry(struct drm_device * dev,
					   unsigned long handle)
{
	struct drm_agp_mem *entry;

	list_for_each_entry(entry, &dev->agp->memory, head) {
		if (entry->handle == handle)
			return entry;
	}
	return NULL;
}

/**
 * Unbind AGP memory from the GATT (ioctl).
 *
 * \param inode device inode.
 * \param file_priv DRM file private.
 * \param cmd command.
 * \param arg pointer to a drm_agp_binding structure.
 * \return zero on success or a negative number on failure.
 *
 * Verifies the AGP device is present and acquired, looks-up the AGP memory
 * entry and passes it to the unbind_agp() function.
 */
int drm_agp_unbind(struct drm_device *dev, struct drm_agp_binding *request)
{
	struct drm_agp_mem *entry;
	int ret;

	if (!dev->agp || !dev->agp->acquired)
		return -EINVAL;
	if (!(entry = drm_agp_lookup_entry(dev, request->handle)))
		return -EINVAL;
	if (!entry->bound)
		return -EINVAL;
	ret = drm_unbind_agp(entry->memory);
	if (ret == 0)
		entry->bound = 0;
	return ret;
}
EXPORT_SYMBOL(drm_agp_unbind);


int drm_agp_unbind_ioctl(struct drm_device *dev, void *data,
			 struct drm_file *file_priv)
{
	struct drm_agp_binding *request = data;

	return drm_agp_unbind(dev, request);
}


/**
 * Bind AGP memory into the GATT (ioctl)
 *
 * \param inode device inode.
 * \param file_priv DRM file private.
 * \param cmd command.
 * \param arg pointer to a drm_agp_binding structure.
 * \return zero on success or a negative number on failure.
 *
 * Verifies the AGP device is present and has been acquired and that no memory
 * is currently bound into the GATT. Looks-up the AGP memory entry and passes
 * it to bind_agp() function.
 */
int drm_agp_bind(struct drm_device *dev, struct drm_agp_binding *request)
{
	struct drm_agp_mem *entry;
	int retcode;
	int page;

	if (!dev->agp || !dev->agp->acquired)
		return -EINVAL;
	if (!(entry = drm_agp_lookup_entry(dev, request->handle)))
		return -EINVAL;
	if (entry->bound)
		return -EINVAL;
	page = (request->offset + PAGE_SIZE - 1) / PAGE_SIZE;
	if ((retcode = drm_bind_agp(entry->memory, page)))
		return retcode;
	entry->bound = dev->agp->base + (page << PAGE_SHIFT);
	DRM_DEBUG("base = 0x%lx entry->bound = 0x%lx\n",
		  dev->agp->base, entry->bound);
	return 0;
}
EXPORT_SYMBOL(drm_agp_bind);


int drm_agp_bind_ioctl(struct drm_device *dev, void *data,
		       struct drm_file *file_priv)
{
	struct drm_agp_binding *request = data;

	return drm_agp_bind(dev, request);
}


/**
 * Free AGP memory (ioctl).
 *
 * \param inode device inode.
 * \param file_priv DRM file private.
 * \param cmd command.
 * \param arg pointer to a drm_agp_buffer structure.
 * \return zero on success or a negative number on failure.
 *
 * Verifies the AGP device is present and has been acquired and looks up the
 * AGP memory entry. If the memory it's currently bound, unbind it via
 * unbind_agp(). Frees it via free_agp() as well as the entry itself
 * and unlinks from the doubly linked list it's inserted in.
 */
int drm_agp_free(struct drm_device *dev, struct drm_agp_buffer *request)
{
	struct drm_agp_mem *entry;

	if (!dev->agp || !dev->agp->acquired)
		return -EINVAL;
	if (!(entry = drm_agp_lookup_entry(dev, request->handle)))
		return -EINVAL;
	if (entry->bound)
		drm_unbind_agp(entry->memory);

	list_del(&entry->head);

	drm_free_agp(entry->memory, entry->pages);
	drm_free(entry, sizeof(*entry), DRM_MEM_AGPLISTS);
	return 0;
}
EXPORT_SYMBOL(drm_agp_free);



int drm_agp_free_ioctl(struct drm_device *dev, void *data,
		       struct drm_file *file_priv)
{
	struct drm_agp_buffer *request = data;

	return drm_agp_free(dev, request);
}


/**
 * Initialize the AGP resources.
 *
 * \return pointer to a drm_agp_head structure.
 *
 * Gets the drm_agp_t structure which is made available by the agpgart module
 * via the inter_module_* functions. Creates and initializes a drm_agp_head
 * structure.
 */
struct drm_agp_head *drm_agp_init(struct drm_device *dev)
{
	struct drm_agp_head *head = NULL;

	if (!(head = drm_alloc(sizeof(*head), DRM_MEM_AGPLISTS)))
		return NULL;
	memset((void *)head, 0, sizeof(*head));

#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,11)
	agp_copy_info(&head->agp_info);
#else
	head->bridge = agp_find_bridge(dev->pdev);
	if (!head->bridge) {
		if (!(head->bridge = agp_backend_acquire(dev->pdev))) {
			drm_free(head, sizeof(*head), DRM_MEM_AGPLISTS);
			return NULL;
		}
		agp_copy_info(head->bridge, &head->agp_info);
		agp_backend_release(head->bridge);
	} else {
		agp_copy_info(head->bridge, &head->agp_info);
	}
#endif
	if (head->agp_info.chipset == NOT_SUPPORTED) {
		drm_free(head, sizeof(*head), DRM_MEM_AGPLISTS);
		return NULL;
	}
	INIT_LIST_HEAD(&head->memory);
	head->cant_use_aperture = head->agp_info.cant_use_aperture;
	head->page_mask = head->agp_info.page_mask;
	head->base = head->agp_info.aper_base;
	return head;
}

/** Calls agp_allocate_memory() */
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,11)
DRM_AGP_MEM *drm_agp_allocate_memory(size_t pages, u32 type)
{
	return agp_allocate_memory(pages, type);
}
#else
DRM_AGP_MEM *drm_agp_allocate_memory(struct agp_bridge_data *bridge,
				     size_t pages, u32 type)
{
	return agp_allocate_memory(bridge, pages, type);
}
#endif

/** Calls agp_free_memory() */
int drm_agp_free_memory(DRM_AGP_MEM * handle)
{
	if (!handle)
		return 0;
	agp_free_memory(handle);
	return 1;
}

/** Calls agp_bind_memory() */
int drm_agp_bind_memory(DRM_AGP_MEM * handle, off_t start)
{
	if (!handle)
		return -EINVAL;
	return agp_bind_memory(handle, start);
}
EXPORT_SYMBOL(drm_agp_bind_memory);

/** Calls agp_unbind_memory() */
int drm_agp_unbind_memory(DRM_AGP_MEM * handle)
{
	if (!handle)
		return -EINVAL;
	return agp_unbind_memory(handle);
}



/*
 * AGP ttm backend interface.
 */

#ifndef AGP_USER_TYPES
#define AGP_USER_TYPES (1 << 16)
#define AGP_USER_MEMORY (AGP_USER_TYPES)
#define AGP_USER_CACHED_MEMORY (AGP_USER_TYPES + 1)
#endif
#define AGP_REQUIRED_MAJOR 0
#define AGP_REQUIRED_MINOR 102

static int drm_agp_needs_unbind_cache_adjust(struct drm_ttm_backend *backend)
{
	return ((backend->flags & DRM_BE_FLAG_BOUND_CACHED) ? 0 : 1);
}


static int drm_agp_populate(struct drm_ttm_backend *backend,
			    unsigned long num_pages, struct page **pages)
{
	struct drm_agp_ttm_backend *agp_be =
		container_of(backend, struct drm_agp_ttm_backend, backend);
	struct page **cur_page, **last_page = pages + num_pages;
	DRM_AGP_MEM *mem;

	if (drm_alloc_memctl(num_pages * sizeof(void *)))
		return -1;

	DRM_DEBUG("drm_agp_populate_ttm\n");
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,11)
	mem = drm_agp_allocate_memory(num_pages, AGP_USER_MEMORY);
#else
	mem = drm_agp_allocate_memory(agp_be->bridge, num_pages, AGP_USER_MEMORY);
#endif
	if (!mem) {
		drm_free_memctl(num_pages * sizeof(void *));
		return -1;
	}

	DRM_DEBUG("Current page count is %ld\n", (long) mem->page_count);
	mem->page_count = 0;
	for (cur_page = pages; cur_page < last_page; ++cur_page)
		mem->memory[mem->page_count++] = phys_to_gart(page_to_phys(*cur_page));
	agp_be->mem = mem;
	return 0;
}

static int drm_agp_bind_ttm(struct drm_ttm_backend *backend,
			    struct drm_bo_mem_reg *bo_mem)
{
	struct drm_agp_ttm_backend *agp_be =
		container_of(backend, struct drm_agp_ttm_backend, backend);
	DRM_AGP_MEM *mem = agp_be->mem;
	int ret;
	int snooped = (bo_mem->flags & DRM_BO_FLAG_CACHED) && !(bo_mem->flags & DRM_BO_FLAG_CACHED_MAPPED);

	DRM_DEBUG("drm_agp_bind_ttm\n");
	mem->is_flushed = TRUE;
	mem->type = AGP_USER_MEMORY;
	/* CACHED MAPPED implies not snooped memory */
	if (snooped)
		mem->type = AGP_USER_CACHED_MEMORY;

	ret = drm_agp_bind_memory(mem, bo_mem->mm_node->start);
	if (ret)
		DRM_ERROR("AGP Bind memory failed\n");

	DRM_FLAG_MASKED(backend->flags, (bo_mem->flags & DRM_BO_FLAG_CACHED) ?
			DRM_BE_FLAG_BOUND_CACHED : 0,
			DRM_BE_FLAG_BOUND_CACHED);
	return ret;
}

static int drm_agp_unbind_ttm(struct drm_ttm_backend *backend)
{
	struct drm_agp_ttm_backend *agp_be =
		container_of(backend, struct drm_agp_ttm_backend, backend);

	DRM_DEBUG("drm_agp_unbind_ttm\n");
	if (agp_be->mem->is_bound)
		return drm_agp_unbind_memory(agp_be->mem);
	else
		return 0;
}

static void drm_agp_clear_ttm(struct drm_ttm_backend *backend)
{
	struct drm_agp_ttm_backend *agp_be =
		container_of(backend, struct drm_agp_ttm_backend, backend);
	DRM_AGP_MEM *mem = agp_be->mem;

	DRM_DEBUG("drm_agp_clear_ttm\n");
	if (mem) {
		unsigned long num_pages = mem->page_count;
		backend->func->unbind(backend);
		agp_free_memory(mem);
		drm_free_memctl(num_pages * sizeof(void *));
	}
	agp_be->mem = NULL;
}

static void drm_agp_destroy_ttm(struct drm_ttm_backend *backend)
{
	struct drm_agp_ttm_backend *agp_be;

	if (backend) {
		DRM_DEBUG("drm_agp_destroy_ttm\n");
		agp_be = container_of(backend, struct drm_agp_ttm_backend, backend);
		if (agp_be) {
			if (agp_be->mem)
				backend->func->clear(backend);
			drm_ctl_free(agp_be, sizeof(*agp_be), DRM_MEM_TTM);
		}
	}
}

static struct drm_ttm_backend_func agp_ttm_backend = {
	.needs_ub_cache_adjust = drm_agp_needs_unbind_cache_adjust,
	.populate = drm_agp_populate,
	.clear = drm_agp_clear_ttm,
	.bind = drm_agp_bind_ttm,
	.unbind = drm_agp_unbind_ttm,
	.destroy =  drm_agp_destroy_ttm,
};

struct drm_ttm_backend *drm_agp_init_ttm(struct drm_device *dev)
{

	struct drm_agp_ttm_backend *agp_be;
	struct agp_kern_info *info;

	if (!dev->agp) {
		DRM_ERROR("AGP is not initialized.\n");
		return NULL;
	}
	info = &dev->agp->agp_info;

	if (info->version.major != AGP_REQUIRED_MAJOR ||
	    info->version.minor < AGP_REQUIRED_MINOR) {
		DRM_ERROR("Wrong agpgart version %d.%d\n"
			  "\tYou need at least version %d.%d.\n",
			  info->version.major,
			  info->version.minor,
			  AGP_REQUIRED_MAJOR,
			  AGP_REQUIRED_MINOR);
		return NULL;
	}


	agp_be = drm_ctl_calloc(1, sizeof(*agp_be), DRM_MEM_TTM);
	if (!agp_be)
		return NULL;

	agp_be->mem = NULL;

	agp_be->bridge = dev->agp->bridge;
	agp_be->populated = FALSE;
	agp_be->backend.func = &agp_ttm_backend;
	agp_be->backend.dev = dev;

	return &agp_be->backend;
}
EXPORT_SYMBOL(drm_agp_init_ttm);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,25)
void drm_agp_chipset_flush(struct drm_device *dev)
{
	agp_flush_chipset(dev->agp->bridge);
}
EXPORT_SYMBOL(drm_agp_flush_chipset);
#endif

#endif				/* __OS_HAS_AGP */
