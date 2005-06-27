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


#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,11)
static int drm_suspend(struct sys_device *sysdev, u32 state)
#else
static int drm_suspend(struct sys_device *sysdev, pm_message_t state)
#endif
{
	struct drm_device *dev = 
			container_of(sysdev, struct drm_device, sysdev);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,12)
	int event = state.event;
#else
	int event = state;
#endif

	DRM_DEBUG("event=%d\n", event);

        if (dev->driver->power)
		return dev->driver->power(dev, event);
	else
		return 0;
}

static int drm_resume(struct sys_device *sysdev)
{
	struct drm_device *dev = 
			container_of(sysdev, struct drm_device, sysdev);

	DRM_DEBUG("\n");

        if (dev->driver->power)
		return dev->driver->power(dev, 0);
	else
		return 0;
}

static int shutdown(struct sys_device *sysdev)
{
	return 0;
}

static atomic_t sysdev_loaded = ATOMIC_INIT(-1);
static struct sysdev_class drm_sysdev_class = {
	set_kset_name("drm"),
	.resume		= drm_resume,
	.suspend	= drm_suspend,
	.shutdown	= shutdown,
};


/**
 * Initialize the Power Management data.
 * 
 * \param dev DRM device.
 * \return zero on success or a negative value on failure.
 */
int drm_pm_setup(drm_device_t *dev)
{
	int rc;

	if (atomic_read(&sysdev_loaded) == -1)
		return 0;

	DRM_DEBUG("\n");

	dev->sysdev.id = dev->primary.minor;
	dev->sysdev.cls = &drm_sysdev_class;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,4)
	rc = sys_device_register(&dev->sysdev);
#else
	rc = sysdev_register(&dev->sysdev);
#endif
	return rc;
}

/**
 * Cleanup the Power Management resources.
 *
 * \param dev DRM device.
 */
void drm_pm_takedown(drm_device_t *dev)
{
	if (atomic_read(&sysdev_loaded) == -1)
		return;

	DRM_DEBUG("\n");

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,4)
	sys_device_unregister(&dev->sysdev);
#else
	sysdev_unregister(&dev->sysdev);
#endif
}

int drm_pm_init(void)
{
	int rc;
	DRM_DEBUG("\n");

	/* triggers on -1 to 0 transition */
	if (!atomic_inc_and_test(&sysdev_loaded))
		return 0;

	if ((rc = sysdev_class_register(&drm_sysdev_class))) {
		/* reset it back to -1 */
		atomic_dec(&sysdev_loaded);
	} else {
		/* inc it up to 1 so that unload will trigger on 1->0 */
		atomic_inc(&sysdev_loaded);
		DRM_DEBUG("registered\n");
	}
	return rc;
}

void __exit drm_pm_exit(void)
{
	DRM_DEBUG("\n");
	/* triggers on the 1 to 0 transistion */
	if (atomic_dec_and_test(&sysdev_loaded)) {
		sysdev_class_unregister(&drm_sysdev_class);
		DRM_DEBUG("unregisted\n");
	}
}
