/* i915_drv.c -- i830,i845,i855,i865,i915 driver -*- linux-c -*-
 */
/*
 *
 * Copyright 2003 Tungsten Graphics, Inc., Cedar Park, Texas.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL TUNGSTEN GRAPHICS AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include "drmP.h"
#include "drm.h"
#include "i915_drm.h"
#include "i915_drv.h"

#include "drm_pciids.h"

static struct pci_device_id pciidlist[] = {
	i915_PCI_IDS
};

#ifdef I915_HAVE_FENCE
extern struct drm_fence_driver i915_fence_driver;
#endif

#ifdef I915_HAVE_BUFFER

static uint32_t i915_mem_prios[] = {DRM_BO_MEM_PRIV0, DRM_BO_MEM_TT, DRM_BO_MEM_LOCAL};
static uint32_t i915_busy_prios[] = {DRM_BO_MEM_TT, DRM_BO_MEM_PRIV0, DRM_BO_MEM_LOCAL};

static struct drm_bo_driver i915_bo_driver = {
	.mem_type_prio = i915_mem_prios,
	.mem_busy_prio = i915_busy_prios,
	.num_mem_type_prio = sizeof(i915_mem_prios)/sizeof(uint32_t),
	.num_mem_busy_prio = sizeof(i915_busy_prios)/sizeof(uint32_t),
	.create_ttm_backend_entry = i915_create_ttm_backend_entry,
	.fence_type = i915_fence_type,
	.invalidate_caches = i915_invalidate_caches,
	.init_mem_type = i915_init_mem_type,
	.evict_flags = i915_evict_flags,
	.move = i915_move,
	.ttm_cache_flush = i915_flush_ttm,
	.command_stream_barrier = NULL,
};
#endif

static int i915_suspend(struct drm_device *dev, pm_message_t state)
{
	struct drm_i915_private *dev_priv = dev->dev_private;

	if (!dev || !dev_priv) {
		printk(KERN_ERR "dev: %p, dev_priv: %p\n", dev, dev_priv);
		printk(KERN_ERR "DRM not initialized, aborting suspend.\n");
		return -ENODEV;
	}

	if (state.event == PM_EVENT_PRETHAW)
		return 0;

	pci_save_state(dev->pdev);

	i915_save_state(dev);

	if (state.event == PM_EVENT_SUSPEND) {
		/* Shut down the device */
		pci_disable_device(dev->pdev);
		pci_set_power_state(dev->pdev, PCI_D3hot);
	}

	return 0;
}

static int i915_resume(struct drm_device *dev)
{
	pci_set_power_state(dev->pdev, PCI_D0);
	pci_restore_state(dev->pdev);
	if (pci_enable_device(dev->pdev))
		return -1;
	pci_set_master(dev->pdev);

	i915_restore_state(dev);

	return 0;
}

static int probe(struct pci_dev *pdev, const struct pci_device_id *ent);
static struct drm_driver driver = {
	/* don't use mtrr's here, the Xserver or user space app should
	 * deal with them for intel hardware.
	 */
	.driver_features =
	    DRIVER_USE_AGP | DRIVER_REQUIRE_AGP | /* DRIVER_USE_MTRR | */
	    DRIVER_HAVE_IRQ | DRIVER_IRQ_SHARED,
	.load = i915_driver_load,
	.unload = i915_driver_unload,
	.firstopen = i915_driver_firstopen,
	.lastclose = i915_driver_lastclose,
	.preclose = i915_driver_preclose,
	.suspend = i915_suspend,
	.resume = i915_resume,
	.device_is_agp = i915_driver_device_is_agp,
	.get_vblank_counter = i915_get_vblank_counter,
	.enable_vblank = i915_enable_vblank,
	.disable_vblank = i915_disable_vblank,
	.irq_preinstall = i915_driver_irq_preinstall,
	.irq_postinstall = i915_driver_irq_postinstall,
	.irq_uninstall = i915_driver_irq_uninstall,
	.irq_handler = i915_driver_irq_handler,
	.reclaim_buffers = drm_core_reclaim_buffers,
	.get_map_ofs = drm_core_get_map_ofs,
	.get_reg_ofs = drm_core_get_reg_ofs,
	.ioctls = i915_ioctls,
	.fops = {
		.owner = THIS_MODULE,
		.open = drm_open,
		.release = drm_release,
		.ioctl = drm_ioctl,
		.mmap = drm_mmap,
		.poll = drm_poll,
		.fasync = drm_fasync,
#if defined(CONFIG_COMPAT) && LINUX_VERSION_CODE > KERNEL_VERSION(2,6,9)
		.compat_ioctl = i915_compat_ioctl,
#endif
		},
	.pci_driver = {
		.name = DRIVER_NAME,
		.id_table = pciidlist,
		.probe = probe,
		.remove = __devexit_p(drm_cleanup_pci),
		},
#ifdef I915_HAVE_FENCE
	.fence_driver = &i915_fence_driver,
#endif
#ifdef I915_HAVE_BUFFER
	.bo_driver = &i915_bo_driver,
#endif
	.name = DRIVER_NAME,
	.desc = DRIVER_DESC,
	.date = DRIVER_DATE,
	.major = DRIVER_MAJOR,
	.minor = DRIVER_MINOR,
	.patchlevel = DRIVER_PATCHLEVEL,
};

static int probe(struct pci_dev *pdev, const struct pci_device_id *ent)
{
	return drm_get_dev(pdev, ent, &driver);
}

static int __init i915_init(void)
{
	driver.num_ioctls = i915_max_ioctl;
	return drm_init(&driver, pciidlist);
}

static void __exit i915_exit(void)
{
	drm_exit(&driver);
}

module_init(i915_init);
module_exit(i915_exit);

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL and additional rights");
