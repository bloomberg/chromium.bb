/**
 * \file radeon_drv.c
 * ATI Radeon driver
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
 * VA LINUX SYSTEMS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "drmP.h"
#include "drm.h"
#include "radeon_drm.h"
#include "radeon_drv.h"

#include "drm_pciids.h"

int radeon_no_wb;
int radeon_dynclks = -1;
int radeon_r4xx_atom = 0;
int radeon_agpmode = 0;

MODULE_PARM_DESC(no_wb, "Disable AGP writeback for scratch registers\n");
module_param_named(no_wb, radeon_no_wb, int, 0444);

int radeon_modeset = 0;
module_param_named(modeset, radeon_modeset, int, 0400);

MODULE_PARM_DESC(dynclks, "Disable/Enable dynamic clocks");
module_param_named(dynclks, radeon_dynclks, int, 0444);

MODULE_PARM_DESC(r4xx_atom, "Enable ATOMBIOS modesetting for R4xx");
module_param_named(r4xx_atom, radeon_r4xx_atom, int, 0444);

MODULE_PARM_DESC(agpmode, "AGP Mode (-1 == PCI)");
module_param_named(agpmode, radeon_agpmode, int, 0444);


static int dri_library_name(struct drm_device * dev, char * buf)
{
	drm_radeon_private_t *dev_priv = dev->dev_private;
	int family;

	if (!dev_priv)
		return 0;

	family = dev_priv->flags & RADEON_FAMILY_MASK;
	return snprintf(buf, PAGE_SIZE, "%s\n",
		(family < CHIP_R200) ? "radeon" :
		((family < CHIP_R300) ? "r200" :
		"r300"));
}

static struct pci_device_id pciidlist[] = {
	radeon_PCI_IDS
};

extern struct drm_fence_driver radeon_fence_driver;

static uint32_t radeon_mem_prios[] = {DRM_BO_MEM_VRAM, DRM_BO_MEM_TT, DRM_BO_MEM_LOCAL};
static uint32_t radeon_busy_prios[] = {DRM_BO_MEM_TT, DRM_BO_MEM_VRAM, DRM_BO_MEM_LOCAL};

static struct drm_bo_driver radeon_bo_driver = {
	.mem_type_prio = radeon_mem_prios,
	.mem_busy_prio = radeon_busy_prios,
	.num_mem_type_prio = sizeof(radeon_mem_prios)/sizeof(uint32_t),
	.num_mem_busy_prio = sizeof(radeon_busy_prios)/sizeof(uint32_t),
	.create_ttm_backend_entry = radeon_create_ttm_backend_entry,
	.fence_type = radeon_fence_types,
	.invalidate_caches = radeon_invalidate_caches,
	.init_mem_type = radeon_init_mem_type,
	.move = radeon_move,
	.evict_flags = radeon_evict_flags,
};

static int probe(struct pci_dev *pdev, const struct pci_device_id *ent);
static struct drm_driver driver = {
	.driver_features =
	    DRIVER_USE_AGP | DRIVER_USE_MTRR | DRIVER_PCI_DMA | DRIVER_SG |
	    DRIVER_HAVE_IRQ | DRIVER_HAVE_DMA | DRIVER_IRQ_SHARED | DRIVER_GEM,
	.dev_priv_size = sizeof(drm_radeon_buf_priv_t),
	.load = radeon_driver_load,
	.firstopen = radeon_driver_firstopen,
	.open = radeon_driver_open,
	.preclose = radeon_driver_preclose,
	.postclose = radeon_driver_postclose,
	.lastclose = radeon_driver_lastclose,
	.unload = radeon_driver_unload,
	.suspend = radeon_suspend,
	.resume = radeon_resume,
	.get_vblank_counter = radeon_get_vblank_counter,
	.enable_vblank = radeon_enable_vblank,
	.disable_vblank = radeon_disable_vblank,
	.dri_library_name = dri_library_name,
	.irq_preinstall = radeon_driver_irq_preinstall,
	.irq_postinstall = radeon_driver_irq_postinstall,
	.irq_uninstall = radeon_driver_irq_uninstall,
	.irq_handler = radeon_driver_irq_handler,
	.reclaim_buffers = drm_core_reclaim_buffers,
	.get_map_ofs = drm_core_get_map_ofs,
	.get_reg_ofs = drm_core_get_reg_ofs,
	.ioctls = radeon_ioctls,
	.gem_init_object = radeon_gem_init_object,
	.gem_free_object = radeon_gem_free_object,
	.dma_ioctl = radeon_cp_buffers,
	.master_create = radeon_master_create,
	.master_destroy = radeon_master_destroy,
	.proc_init = radeon_gem_proc_init,
	.proc_cleanup = radeon_gem_proc_cleanup,
	.fops = {
		.owner = THIS_MODULE,
		.open = drm_open,
		.release = drm_release,
		.ioctl = drm_ioctl,
		.mmap = drm_mmap,
		.poll = drm_poll,
		.fasync = drm_fasync,
#if defined(CONFIG_COMPAT) && LINUX_VERSION_CODE > KERNEL_VERSION(2,6,9)
		.compat_ioctl = radeon_compat_ioctl,
#endif
		},
	.pci_driver = {
		.name = DRIVER_NAME,
		.id_table = pciidlist,
		.probe = probe,
		.remove = __devexit_p(drm_cleanup_pci),
	},

	.fence_driver = &radeon_fence_driver,
	.bo_driver = &radeon_bo_driver,

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

static int __init radeon_init(void)
{
	driver.num_ioctls = radeon_max_ioctl;

	if (radeon_modeset == 1)
		driver.driver_features |= DRIVER_MODESET;

	return drm_init(&driver, pciidlist);
}

static void __exit radeon_exit(void)
{
	drm_exit(&driver);
}

module_init(radeon_init);
module_exit(radeon_exit);

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL and additional rights");
