/* drm_ioctl.h -- IOCTL processing for DRM -*- linux-c -*-
 * Created: Fri Jan  8 09:01:26 1999 by faith@valinux.com
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
 * VA LINUX SYSTEMS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *    Rickard E. (Rik) Faith <faith@valinux.com>
 *    Gareth Hughes <gareth@valinux.com>
 */

#define __NO_VERSION__
#include "drmP.h"
#include <sys/bus.h>
#include <pci/pcivar.h>

int DRM(irq_busid)( DRM_OS_IOCTL )
{
	drm_irq_busid_t id;
	devclass_t pci;
	device_t bus, dev;
	device_t *kids;
	int error, i, num_kids;

	DRM_OS_KRNFROMUSR( id, (drm_irq_busid_t *)data, sizeof(id) );

	pci = devclass_find("pci");
	if (!pci)
		return ENOENT;
	bus = devclass_get_device(pci, id.busnum);
	if (!bus)
		return ENOENT;
	error = device_get_children(bus, &kids, &num_kids);
	if (error)
		return error;

	dev = 0;
	for (i = 0; i < num_kids; i++) {
		dev = kids[i];
		if (pci_get_slot(dev) == id.devnum
		    && pci_get_function(dev) == id.funcnum)
			break;
	}

	free(kids, M_TEMP);

	if (i != num_kids)
		id.irq = pci_get_irq(dev);
	else
		id.irq = 0;
	DRM_DEBUG("%d:%d:%d => IRQ %d\n",
		  id.busnum, id.devnum, id.funcnum, id.irq);
	
	DRM_OS_KRNTOUSR( (drm_irq_busid_t *)data, id, sizeof(id) );

	return 0;
}

int DRM(getunique)( DRM_OS_IOCTL )
{
	DRM_OS_DEVICE;
	drm_unique_t	 u;

	DRM_OS_KRNFROMUSR( u, (drm_unique_t *)data, sizeof(u) );

	if (u.unique_len >= dev->unique_len) {
		if (DRM_OS_COPYTOUSR(u.unique, dev->unique, dev->unique_len))
			DRM_OS_RETURN(EFAULT);
	}
	u.unique_len = dev->unique_len;

	DRM_OS_KRNTOUSR( (drm_unique_t *)data, u, sizeof(u) );

	return 0;
}

int DRM(setunique)( DRM_OS_IOCTL )
{
	DRM_OS_DEVICE;
	drm_unique_t	 u;

	if (dev->unique_len || dev->unique)
		DRM_OS_RETURN(EBUSY);

	DRM_OS_KRNFROMUSR( u, (drm_unique_t *)data, sizeof(u) );

	if (!u.unique_len || u.unique_len > 1024)
		DRM_OS_RETURN(EINVAL);

	dev->unique_len = u.unique_len;
	dev->unique	= DRM(alloc)(u.unique_len + 1, DRM_MEM_DRIVER);

	if(!dev->unique) DRM_OS_RETURN(ENOMEM);

	if (DRM_OS_COPYFROMUSR(dev->unique, u.unique, dev->unique_len))
		DRM_OS_RETURN(EFAULT);

	dev->unique[dev->unique_len] = '\0';

	dev->devname = DRM(alloc)(strlen(dev->name) + strlen(dev->unique) + 2,
				  DRM_MEM_DRIVER);
	if(!dev->devname) {
		DRM(free)(dev->devname, sizeof(*dev->devname), DRM_MEM_DRIVER);
		DRM_OS_RETURN(ENOMEM);
	}
	sprintf(dev->devname, "%s@%s", dev->name, dev->unique);


	return 0;
}


int DRM(getmap)( DRM_OS_IOCTL )
{
	DRM_OS_DEVICE;
	drm_map_t    map;
	drm_map_t    *mapinlist;
	drm_map_list_entry_t *list;
	int          idx;
	int	     i = 0;

	DRM_OS_KRNFROMUSR( map, (drm_map_t *)data, sizeof(map) );

	idx = map.offset;

	DRM_OS_LOCK;
	if (idx < 0 || idx >= dev->map_count) {
		DRM_OS_UNLOCK;
		DRM_OS_RETURN(EINVAL);
	}

	TAILQ_FOREACH(list, dev->maplist, link) {
		mapinlist = list->map;
		if (i==idx) {
			map.offset = mapinlist->offset;
			map.size   = mapinlist->size;
			map.type   = mapinlist->type;
			map.flags  = mapinlist->flags;
			map.handle = mapinlist->handle;
			map.mtrr   = mapinlist->mtrr;
			break;
		}
		i++;
	}

	DRM_OS_UNLOCK;

 	if (!list)
		return EINVAL;

	DRM_OS_KRNTOUSR( (drm_map_t *)data, map, sizeof(map) );

	return 0;
}

int DRM(getclient)( DRM_OS_IOCTL )
{
	DRM_OS_DEVICE;
	drm_client_t client;
	drm_file_t   *pt;
	int          idx;
	int          i = 0;

	DRM_OS_KRNFROMUSR( client, (drm_client_t *)data, sizeof(client) );

	idx = client.idx;
	DRM_OS_LOCK;
	TAILQ_FOREACH(pt, &dev->files, link) {
		if (i==idx)
		{
			client.auth  = pt->authenticated;
			client.pid   = pt->pid;
			client.uid   = pt->uid;
			client.magic = pt->magic;
			client.iocs  = pt->ioctl_count;
			DRM_OS_UNLOCK;

			*(drm_client_t *)data = client;
			return 0;
		}
		i++;
	}
	DRM_OS_UNLOCK;

	DRM_OS_KRNTOUSR( (drm_client_t *)data, client, sizeof(client) );

	return 0;
}

int DRM(getstats)( DRM_OS_IOCTL )
{
	DRM_OS_DEVICE;
	drm_stats_t  stats;
	int          i;

	memset(&stats, 0, sizeof(stats));
	
	DRM_OS_LOCK;

	for (i = 0; i < dev->counters; i++) {
		if (dev->types[i] == _DRM_STAT_LOCK)
			stats.data[i].value
				= (dev->lock.hw_lock
				   ? dev->lock.hw_lock->lock : 0);
		else 
			stats.data[i].value = atomic_read(&dev->counts[i]);
		stats.data[i].type  = dev->types[i];
	}
	
	stats.count = dev->counters;

	DRM_OS_UNLOCK;

	DRM_OS_KRNTOUSR( (drm_stats_t *)data, stats, sizeof(stats) );

	return 0;
}
