/**
 * \file drm_pm.h 
 * Power management support
 *
 * \author José Fonseca <jrfonseca@tungstengraphics.com>
 */

/*
 * Copyright 2004 Tungsten Graphics, Inc., Cedar Park, Texas.
 * All rights reserved.
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
 * TUNGSTEN GRAPHICS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#define __NO_VERSION__
#include "drmP.h"


#include <linux/device.h>
#include <linux/sysdev.h>

static int drm_suspend(struct sys_device *sysdev, u32 state)
{
	drm_device_t *dev = (drm_device_t *)sysdev;
	
	DRM_DEBUG("%s state=%d\n", __FUNCTION__, state);
	
        if (dev->driver->power)
		return dev->driver->power(dev, state);
	else
		return 0;
}

static int drm_resume(struct sys_device *sysdev)
{
	drm_device_t *dev = (drm_device_t *)sysdev;
	
	DRM_DEBUG("%s\n", __FUNCTION__);
	
        if (dev->driver->power)
		return dev->driver->power(dev, 0);
	else
		return 0;
}

static struct sysdev_class drm_sysdev_class = {
	set_kset_name("drm"),
	.resume		= drm_resume,
	.suspend	= drm_suspend,
};


/**
 * Initialize the Power Management data.
 * 
 * \param dev DRM device.
 * \return zero on success or a negative value on failure.
 */
int drm_pm_setup(drm_device_t *dev)
{
	int error;
	
	DRM_DEBUG("%s\n", __FUNCTION__);
	
	dev->sysdev.id = dev->primary.minor;
	dev->sysdev.cls = &drm_sysdev_class;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,4)
	error = sys_device_register(&dev->sysdev);
#else
	error = sysdev_register(&dev->sysdev);
#endif
	if(!error)
		dev->sysdev_registered = 1;
	return error;
}

/**
 * Cleanup the Power Management resources.
 *
 * \param dev DRM device.
 */
void drm_pm_takedown(drm_device_t *dev)
{
	DRM_DEBUG("%s\n", __FUNCTION__);
	
	if(dev->sysdev_registered) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,4)
		sys_device_unregister(&dev->sysdev);
#else
		sysdev_unregister(&dev->sysdev);
#endif
		dev->sysdev_registered = 0;
	}
}

int drm_pm_init(void)
{
	DRM_DEBUG("%s\n", __FUNCTION__);
	
	return sysdev_class_register(&drm_sysdev_class);
}

void drm_pm_cleanup(void)
{
	DRM_DEBUG("%s\n", __FUNCTION__);
	
	sysdev_class_unregister(&drm_sysdev_class);
}
