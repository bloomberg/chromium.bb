
/****************************************************************************
 * Copyright (C) 2003-2006 by XGI Technology, Taiwan.			
 *																			*
 * All Rights Reserved.														*
 *																			*
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the	
 * "Software"), to deal in the Software without restriction, including	
 * without limitation on the rights to use, copy, modify, merge,	
 * publish, distribute, sublicense, and/or sell copies of the Software,	
 * and to permit persons to whom the Software is furnished to do so,	
 * subject to the following conditions:					
 *																			*
 * The above copyright notice and this permission notice (including the	
 * next paragraph) shall be included in all copies or substantial	
 * portions of the Software.						
 *																			*
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,	
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF	
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND		
 * NON-INFRINGEMENT.  IN NO EVENT SHALL XGI AND/OR			
 * ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,		
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,		
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER			
 * DEALINGS IN THE SOFTWARE.												
 ***************************************************************************/

#include "drmP.h"
#include "drm.h"
#include "xgi_drv.h"
#include "xgi_regs.h"
#include "xgi_misc.h"
#include "xgi_cmdlist.h"

#include "drm_pciids.h"

static struct pci_device_id pciidlist[] = {
	xgi_PCI_IDS
};

static int xgi_bootstrap(DRM_IOCTL_ARGS);

static drm_ioctl_desc_t xgi_ioctls[] = {
	[DRM_IOCTL_NR(DRM_XGI_BOOTSTRAP)] = {xgi_bootstrap, DRM_AUTH},

	[DRM_IOCTL_NR(DRM_XGI_FB_ALLOC)] = {xgi_fb_alloc_ioctl, DRM_AUTH},
	[DRM_IOCTL_NR(DRM_XGI_FB_FREE)] = {xgi_fb_free_ioctl, DRM_AUTH},

	[DRM_IOCTL_NR(DRM_XGI_PCIE_ALLOC)] = {xgi_pcie_alloc_ioctl, DRM_AUTH},
	[DRM_IOCTL_NR(DRM_XGI_PCIE_FREE)] = {xgi_pcie_free_ioctl, DRM_AUTH},

	[DRM_IOCTL_NR(DRM_XGI_GE_RESET)] = {xgi_ge_reset_ioctl, DRM_AUTH},
	[DRM_IOCTL_NR(DRM_XGI_DUMP_REGISTER)] = {xgi_dump_register_ioctl, DRM_AUTH},
	[DRM_IOCTL_NR(DRM_XGI_DEBUG_INFO)] = {xgi_restore_registers_ioctl, DRM_AUTH},
	[DRM_IOCTL_NR(DRM_XGI_SUBMIT_CMDLIST)] = {xgi_submit_cmdlist_ioctl, DRM_AUTH},
	[DRM_IOCTL_NR(DRM_XGI_TEST_RWINKERNEL)] = {xgi_test_rwinkernel_ioctl, DRM_AUTH},
	[DRM_IOCTL_NR(DRM_XGI_STATE_CHANGE)] = {xgi_state_change_ioctl, DRM_AUTH},
};

static const int xgi_max_ioctl = DRM_ARRAY_SIZE(xgi_ioctls);

static int probe(struct pci_dev *pdev, const struct pci_device_id *ent);
static int xgi_driver_load(struct drm_device *dev, unsigned long flags);
static int xgi_driver_unload(struct drm_device *dev);
static void xgi_driver_preclose(struct drm_device * dev, DRMFILE filp);
static irqreturn_t xgi_kern_isr(DRM_IRQ_ARGS);


static struct drm_driver driver = {
	.driver_features =
		DRIVER_PCI_DMA | DRIVER_HAVE_DMA | DRIVER_HAVE_IRQ |
		DRIVER_IRQ_SHARED | DRIVER_SG,
	.dev_priv_size = sizeof(struct xgi_info),
	.load = xgi_driver_load,
	.unload = xgi_driver_unload,
	.preclose = xgi_driver_preclose,
	.dma_quiescent = NULL,
	.irq_preinstall = NULL,
	.irq_postinstall = NULL,
	.irq_uninstall = NULL,
	.irq_handler = xgi_kern_isr,
	.reclaim_buffers = drm_core_reclaim_buffers,
	.get_map_ofs = drm_core_get_map_ofs,
	.get_reg_ofs = drm_core_get_reg_ofs,
	.ioctls = xgi_ioctls,
	.dma_ioctl = NULL,

	.fops = {
		.owner = THIS_MODULE,
		.open = drm_open,
		.release = drm_release,
		.ioctl = drm_ioctl,
		.mmap = drm_mmap,
		.poll = drm_poll,
		.fasync = drm_fasync,
	},

	.pci_driver = {
		.name = DRIVER_NAME,
		.id_table = pciidlist,
		.probe = probe,
		.remove = __devexit_p(drm_cleanup_pci),
	},

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


static int __init xgi_init(void)
{
	driver.num_ioctls = xgi_max_ioctl;
	return drm_init(&driver, pciidlist);
}

static void __exit xgi_exit(void)
{
	drm_exit(&driver);
}

module_init(xgi_init);
module_exit(xgi_exit);

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL and additional rights");


void xgi_kern_isr_bh(struct drm_device *dev);

/*
 * verify access to pci config space wasn't disabled behind our back
 * unfortunately, XFree86 enables/disables memory access in pci config space at
 * various times (such as restoring initial pci config space settings during vt
 * switches or when doing mulicard). As a result, all of our register accesses
 * are garbage at this point. add a check to see if access was disabled and
 * reenable any such access.
 */
#define XGI_CHECK_PCI_CONFIG(xgi) \
    xgi_check_pci_config(xgi, __LINE__)

static inline void xgi_check_pci_config(struct xgi_info * info, int line)
{
	u16 cmd;
	bool flag = 0;

	pci_read_config_word(info->dev->pdev, PCI_COMMAND, &cmd);
	if (!(cmd & PCI_COMMAND_MASTER)) {
		DRM_INFO("restoring bus mastering! (%d)\n", line);
		cmd |= PCI_COMMAND_MASTER;
		flag = 1;
	}

	if (!(cmd & PCI_COMMAND_MEMORY)) {
		DRM_INFO("restoring MEM access! (%d)\n", line);
		cmd |= PCI_COMMAND_MEMORY;
		flag = 1;
	}

	if (flag)
		pci_write_config_word(info->dev->pdev, PCI_COMMAND, cmd);
}


int xgi_bootstrap(DRM_IOCTL_ARGS)
{
	DRM_DEVICE;
	struct xgi_info *info = dev->dev_private;
	struct xgi_bootstrap bs;
	int err;


	DRM_COPY_FROM_USER_IOCTL(bs, (struct xgi_bootstrap __user *) data,
				 sizeof(bs));

	if (info->bootstrap_done) {
		return 0;
	}

	xgi_enable_mmio(info);

	info->pcie.size = bs.gart_size * (1024 * 1024);

	/* Init the resource manager */
	err = xgi_pcie_heap_init(info);
	if (err) {
		DRM_ERROR("xgi_pcie_heap_init() failed\n");
		return err;
	}

	/* Alloc 1M bytes for cmdbuffer which is flush2D batch array */
	xgi_cmdlist_initialize(info, 0x100000);

	info->bootstrap_done = 1;
	return 0;
}


void xgi_driver_preclose(struct drm_device * dev, DRMFILE filp)
{
	struct xgi_info * info = dev->dev_private;

	xgi_pcie_free_all(info, filp);
	xgi_fb_free_all(info, filp);
}


/*
 * driver receives an interrupt if someone waiting, then hand it off.
 */
irqreturn_t xgi_kern_isr(DRM_IRQ_ARGS)
{
	struct drm_device *dev = (struct drm_device *) arg;
//	struct xgi_info *info = dev->dev_private;
	u32 need_to_run_bottom_half = 0;

	//DRM_INFO("xgi_kern_isr \n");

	//XGI_CHECK_PCI_CONFIG(info);

	//xgi_dvi_irq_handler(info);

	if (need_to_run_bottom_half) {
		drm_locked_tasklet(dev, xgi_kern_isr_bh);
	}

	return IRQ_HANDLED;
}

void xgi_kern_isr_bh(struct drm_device *dev)
{
	struct xgi_info *info = dev->dev_private;

	DRM_INFO("xgi_kern_isr_bh \n");

	//xgi_dvi_irq_handler(info);

	XGI_CHECK_PCI_CONFIG(info);
}

int xgi_driver_load(struct drm_device *dev, unsigned long flags)
{
	struct xgi_info *info;
	int err;


	info = drm_alloc(sizeof(*info), DRM_MEM_DRIVER);
	if (!info)
		return DRM_ERR(ENOMEM);

	(void) memset(info, 0, sizeof(*info));
	dev->dev_private = info;
	info->dev = dev;

	sema_init(&info->fb_sem, 1);
	sema_init(&info->pcie_sem, 1);

	info->mmio.base = drm_get_resource_start(dev, 1);
	info->mmio.size = drm_get_resource_len(dev, 1);

	DRM_INFO("mmio base: 0x%lx, size: 0x%x\n",
		 (unsigned long) info->mmio.base, info->mmio.size);


	if ((info->mmio.base == 0) || (info->mmio.size == 0)) {
		DRM_ERROR("mmio appears to be wrong: 0x%lx 0x%x\n",
			  (unsigned long) info->mmio.base, info->mmio.size);
		return DRM_ERR(EINVAL);
	}


	err = drm_addmap(dev, info->mmio.base, info->mmio.size,
			 _DRM_REGISTERS, _DRM_KERNEL | _DRM_READ_ONLY,
			 &info->mmio_map);
	if (err) {
		DRM_ERROR("Unable to map MMIO region: %d\n", err);
		return err;
	}

	xgi_enable_mmio(info);
	//xgi_enable_ge(info);

	info->fb.base = drm_get_resource_start(dev, 0);
	info->fb.size = drm_get_resource_len(dev, 0);

	DRM_INFO("fb   base: 0x%lx, size: 0x%x\n",
		 (unsigned long) info->fb.base, info->fb.size);

	info->fb.size = IN3CFB(info->mmio_map, 0x54) * 8 * 1024 * 1024;

	DRM_INFO("fb   base: 0x%lx, size: 0x%x (probed)\n",
		 (unsigned long) info->fb.base, info->fb.size);


	if ((info->fb.base == 0) || (info->fb.size == 0)) {
		DRM_ERROR("frame buffer appears to be wrong: 0x%lx 0x%x\n",
			  (unsigned long) info->fb.base, info->fb.size);
		return DRM_ERR(EINVAL);
	}



	xgi_mem_block_cache = kmem_cache_create("xgi_mem_block",
						sizeof(struct xgi_mem_block),
						0,
						SLAB_HWCACHE_ALIGN,
						NULL, NULL);
	if (xgi_mem_block_cache == NULL) {
		return DRM_ERR(ENOMEM);
	}


	/* Init the resource manager */
	err = xgi_fb_heap_init(info);
	if (err) {
		DRM_ERROR("xgi_fb_heap_init() failed\n");
		return err;
	}

	return 0;
}

int xgi_driver_unload(struct drm_device *dev)
{
	struct xgi_info * info = dev->dev_private;

	xgi_cmdlist_cleanup(info);
	if (info->fb_map != NULL) {
		drm_rmmap(info->dev, info->fb_map);
	}

	if (info->mmio_map != NULL) {
		drm_rmmap(info->dev, info->mmio_map);
	}

	xgi_mem_heap_cleanup(&info->fb_heap);
	xgi_mem_heap_cleanup(&info->pcie_heap);
	xgi_pcie_lut_cleanup(info);

	if (xgi_mem_block_cache) {
		kmem_cache_destroy(xgi_mem_block_cache);
		xgi_mem_block_cache = NULL;
	}

	return 0;
}
