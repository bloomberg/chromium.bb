/* agpsupport.c -- DRM support for AGP/GART backend
 * Created: Mon Dec 13 09:56:45 1999 by faith@precisioninsight.com
 *
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
 * PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 * 
 * Author: Rickard E. (Rik) Faith <faith@valinux.com>
 *
 */

#define __NO_VERSION__
#include "drmP.h"

#ifdef DRM_AGP

#include <pci/agpvar.h>

MODULE_DEPEND(drm, agp, 1, 1, 1);

int
drm_agp_info(dev_t kdev, u_long cmd, caddr_t data,
	     int flags, struct proc *p)
{
	drm_device_t	*dev	= kdev->si_drv1;
	struct agp_info *kern;
	drm_agp_info_t   info;

	if (!dev->agp->acquired) return EINVAL;

	kern                   = &dev->agp->info;
	agp_get_info(dev->agp->agpdev, kern);
	info.agp_version_major = 1;
	info.agp_version_minor = 0;
	info.mode              = kern->ai_mode;
	info.aperture_base     = kern->ai_aperture_base;
	info.aperture_size     = kern->ai_aperture_size;
	info.memory_allowed    = kern->ai_memory_allowed;
	info.memory_used       = kern->ai_memory_used;
	info.id_vendor         = kern->ai_devid & 0xffff;
	info.id_device         = kern->ai_devid >> 16;

	*(drm_agp_info_t *) data = info;
	return 0;
}

int
drm_agp_acquire(dev_t kdev, u_long cmd, caddr_t data,
		int flags, struct proc *p)
{
	drm_device_t *dev = kdev->si_drv1;
	int          retcode;

	if (dev->agp->acquired) return EINVAL;
	retcode = agp_acquire(dev->agp->agpdev);
	if (retcode) return retcode;
	dev->agp->acquired = 1;
	return 0;
}

int
drm_agp_release(dev_t kdev, u_long cmd, caddr_t data,
		int flags, struct proc *p)
{
	drm_device_t *dev = kdev->si_drv1;

	if (!dev->agp->acquired) return EINVAL;
	agp_release(dev->agp->agpdev);
	dev->agp->acquired = 0;
	return 0;
	
}

int
drm_agp_enable(dev_t kdev, u_long cmd, caddr_t data,
	       int flags, struct proc *p)
{
	drm_device_t   *dev = kdev->si_drv1;
	drm_agp_mode_t mode;

	if (!dev->agp->acquired) return EINVAL;

	mode = *(drm_agp_mode_t *) data;
	
	dev->agp->mode    = mode.mode;
	agp_enable(dev->agp->agpdev, mode.mode);
	dev->agp->base    = dev->agp->info.ai_aperture_base;
	dev->agp->enabled = 1;
	return 0;
}

int drm_agp_alloc(dev_t kdev, u_long cmd, caddr_t data,
		  int flags, struct proc *p)
{
	drm_device_t     *dev = kdev->si_drv1;
	drm_agp_buffer_t request;
	drm_agp_mem_t    *entry;
	void	         *handle;
	unsigned long    pages;
	u_int32_t	 type;
	struct agp_memory_info info;

	if (!dev->agp->acquired) return EINVAL;

	request = *(drm_agp_buffer_t *) data;

	if (!(entry = drm_alloc(sizeof(*entry), DRM_MEM_AGPLISTS)))
		return ENOMEM;
   
   	memset(entry, 0, sizeof(*entry));

	pages = (request.size + PAGE_SIZE - 1) / PAGE_SIZE;
	type = (u_int32_t) request.type;

	if (!(handle = drm_alloc_agp(pages, type))) {
		drm_free(entry, sizeof(*entry), DRM_MEM_AGPLISTS);
		return ENOMEM;
	}
	
	entry->handle    = handle;
	entry->bound     = 0;
	entry->pages     = pages;
	entry->prev      = NULL;
	entry->next      = dev->agp->memory;
	if (dev->agp->memory) dev->agp->memory->prev = entry;
	dev->agp->memory = entry;

	agp_memory_info(dev->agp->agpdev, entry->handle, &info);

	request.handle   = (unsigned long) entry->handle;
        request.physical = info.ami_physical;

	*(drm_agp_buffer_t *) data = request;

	return 0;
}

static drm_agp_mem_t *
drm_agp_lookup_entry(drm_device_t *dev, void *handle)
{
	drm_agp_mem_t *entry;

	for (entry = dev->agp->memory; entry; entry = entry->next) {
		if (entry->handle == handle) return entry;
	}
	return NULL;
}

int
drm_agp_unbind(dev_t kdev, u_long cmd, caddr_t data,
	       int flags, struct proc *p)
{
	drm_device_t      *dev = kdev->si_drv1;
	drm_agp_binding_t request;
	drm_agp_mem_t     *entry;

	if (!dev->agp->acquired) return EINVAL;
	request = *(drm_agp_binding_t *) data;
	if (!(entry = drm_agp_lookup_entry(dev, (void *) request.handle)))
		return EINVAL;
	if (!entry->bound) return EINVAL;
	return drm_unbind_agp(entry->handle);
}

int drm_agp_bind(dev_t kdev, u_long cmd, caddr_t data,
		 int flags, struct proc *p)
{
	drm_device_t      *dev = kdev->si_drv1;
	drm_agp_binding_t request;
	drm_agp_mem_t     *entry;
	int               retcode;
	int               page;
	
	if (!dev->agp->acquired) return EINVAL;
	request = *(drm_agp_binding_t *) data;
	if (!(entry = drm_agp_lookup_entry(dev, (void *) request.handle)))
		return EINVAL;
	if (entry->bound) return EINVAL;
	page = (request.offset + PAGE_SIZE - 1) / PAGE_SIZE;
	if ((retcode = drm_bind_agp(entry->handle, page))) return retcode;
	entry->bound = dev->agp->base + (page << PAGE_SHIFT);
	return 0;
}

int drm_agp_free(dev_t kdev, u_long cmd, caddr_t data,
		 int flags, struct proc *p)
{
	drm_device_t     *dev = kdev->si_drv1;
	drm_agp_buffer_t request;
	drm_agp_mem_t    *entry;
	
	if (!dev->agp->acquired) return EINVAL;
	request = *(drm_agp_buffer_t *) data;
	if (!(entry = drm_agp_lookup_entry(dev, (void*) request.handle)))
		return EINVAL;
	if (entry->bound) drm_unbind_agp(entry->handle);
   
	if (entry->prev) entry->prev->next = entry->next;
	else             dev->agp->memory  = entry->next;
	if (entry->next) entry->next->prev = entry->prev;
	drm_free_agp(entry->handle, entry->pages);
	drm_free(entry, sizeof(*entry), DRM_MEM_AGPLISTS);
	return 0;
}

drm_agp_head_t *drm_agp_init(void)
{
	device_t agpdev;
	drm_agp_head_t *head   = NULL;
	int      agp_available = 1;
   
	agpdev = agp_find_device();
	if (!agpdev)
		agp_available = 0;

	DRM_DEBUG("agp_available = %d\n", agp_available);

	if (agp_available) {
		if (!(head = drm_alloc(sizeof(*head), DRM_MEM_AGPLISTS)))
			return NULL;
		memset((void *)head, 0, sizeof(*head));
		head->agpdev = agpdev;
		agp_get_info(agpdev, &head->info);
		head->memory = NULL;
#if 0				/* bogus */
		switch (head->agp_info.chipset) {
		case INTEL_GENERIC:  head->chipset = "Intel";          break;
		case INTEL_LX:       head->chipset = "Intel 440LX";    break;
		case INTEL_BX:       head->chipset = "Intel 440BX";    break;
		case INTEL_GX:       head->chipset = "Intel 440GX";    break;
		case INTEL_I810:     head->chipset = "Intel i810";     break;
		case VIA_GENERIC:    head->chipset = "VIA";            break;
		case VIA_VP3:        head->chipset = "VIA VP3";        break;
		case VIA_MVP3:       head->chipset = "VIA MVP3";       break;
		case VIA_APOLLO_PRO: head->chipset = "VIA Apollo Pro"; break;
		case SIS_GENERIC:    head->chipset = "SiS";            break;
		case AMD_GENERIC:    head->chipset = "AMD";            break;
		case AMD_IRONGATE:   head->chipset = "AMD Irongate";   break;
		case ALI_GENERIC:    head->chipset = "ALi";            break;
		case ALI_M1541:      head->chipset = "ALi M1541";      break;
		default:
		}
#endif
		DRM_INFO("AGP at 0x%08x %dMB\n",
			 head->info.ai_aperture_base,
			 head->info.ai_aperture_size >> 20);
	}
	return head;
}

#endif /* DRM_AGP */
