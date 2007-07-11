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
static drm_fence_driver_t i915_fence_driver = {
	.num_classes = 1,
	.wrap_diff = (1U << (BREADCRUMB_BITS - 1)),
	.flush_diff = (1U << (BREADCRUMB_BITS - 2)),
	.sequence_mask = BREADCRUMB_MASK,
	.lazy_capable = 1,
	.emit = i915_fence_emit_sequence,
	.poke_flush = i915_poke_flush,
	.has_irq = i915_fence_has_irq,
};
#endif
#ifdef I915_HAVE_BUFFER

static uint32_t i915_mem_prios[] = {DRM_BO_MEM_PRIV0, DRM_BO_MEM_TT, DRM_BO_MEM_LOCAL};
static uint32_t i915_busy_prios[] = {DRM_BO_MEM_TT, DRM_BO_MEM_PRIV0, DRM_BO_MEM_LOCAL};

static drm_bo_driver_t i915_bo_driver = {
	.mem_type_prio = i915_mem_prios,
	.mem_busy_prio = i915_busy_prios,
	.num_mem_type_prio = sizeof(i915_mem_prios)/sizeof(uint32_t),
	.num_mem_busy_prio = sizeof(i915_busy_prios)/sizeof(uint32_t),
	.create_ttm_backend_entry = i915_create_ttm_backend_entry,
	.fence_type = i915_fence_types,
	.invalidate_caches = i915_invalidate_caches,
	.init_mem_type = i915_init_mem_type,
	.evict_mask = i915_evict_mask,
	.move = i915_move,
};
#endif

static int probe(struct pci_dev *pdev, const struct pci_device_id *ent);
static struct drm_driver driver = {
	/* don't use mtrr's here, the Xserver or user space app should
	 * deal with them for intel hardware.
	 */
	.driver_features =
	    DRIVER_USE_AGP | DRIVER_REQUIRE_AGP | /* DRIVER_USE_MTRR | */
	    DRIVER_HAVE_IRQ | DRIVER_IRQ_SHARED | DRIVER_IRQ_VBL |
	    DRIVER_IRQ_VBL2,
	.load = i915_driver_load,
	.firstopen = i915_driver_firstopen,
	.lastclose = i915_driver_lastclose,
	.preclose = i915_driver_preclose,
	.device_is_agp = i915_driver_device_is_agp,
	.vblank_wait = i915_driver_vblank_wait,
	.vblank_wait2 = i915_driver_vblank_wait2,
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
