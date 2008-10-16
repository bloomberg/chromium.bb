/*
 * Copyright 2007-8 Advanced Micro Devices, Inc.
 * Copyright 2008 Red Hat Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors: Dave Airlie
 *          Alex Deucher
 */
#include "drmP.h"
#include "radeon_drm.h"
#include "radeon_drv.h"

#include "atom.h"

#include "drm_crtc_helper.h"

int radeon_suspend(struct drm_device *dev, pm_message_t state)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;
	struct drm_framebuffer *fb;
	int i;

	if (!dev || !dev_priv) {
		return -ENODEV;
	}

	if (state.event == PM_EVENT_PRETHAW)
		return 0;

	if (!drm_core_check_feature(dev, DRIVER_MODESET))
		return 0;

	/* unpin the front buffers */
	list_for_each_entry(fb, &dev->mode_config.fb_kernel_list, filp_head) {
		struct radeon_framebuffer *radeon_fb = to_radeon_framebuffer(fb);

		if (!radeon_fb)
			continue;

		if (!radeon_fb->base.mm_private)
			continue;
		
		radeon_gem_object_unpin(radeon_fb->base.mm_private);
	}

	if (!(dev_priv->flags & RADEON_IS_IGP))
		drm_bo_evict_mm(dev, DRM_BO_MEM_VRAM, 0);

	dev_priv->pmregs.crtc_ext_cntl = RADEON_READ(RADEON_CRTC_EXT_CNTL);
	for (i = 0; i < 8; i++)
		dev_priv->pmregs.bios_scratch[i] = RADEON_READ(RADEON_BIOS_0_SCRATCH + (i * 4));

	radeon_modeset_cp_suspend(dev);

	if (dev_priv->flags & RADEON_IS_PCIE) {
		memcpy_fromio(dev_priv->mm.pcie_table_backup, dev_priv->mm.pcie_table.kmap.virtual, RADEON_PCIGART_TABLE_SIZE);
	}

	pci_save_state(dev->pdev);

	if (state.event == PM_EVENT_SUSPEND) {
		/* Shut down the device */
		pci_disable_device(dev->pdev);
		pci_set_power_state(dev->pdev, PCI_D3hot);
	}
	return 0;
}

int radeon_resume(struct drm_device *dev)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;
	struct drm_framebuffer *fb;
	int i;
	u32 tmp;

	if (!drm_core_check_feature(dev, DRIVER_MODESET))
		return 0;

	pci_set_power_state(dev->pdev, PCI_D0);
	pci_restore_state(dev->pdev);
	if (pci_enable_device(dev->pdev))
		return -1;
	pci_set_master(dev->pdev);

	/* Turn on bus mastering */
	tmp = RADEON_READ(RADEON_BUS_CNTL) & ~RADEON_BUS_MASTER_DIS;
	RADEON_WRITE(RADEON_BUS_CNTL, tmp);

	/* on atom cards re init the whole card 
	   and set the modes again */

	if (dev_priv->is_atom_bios) {
		struct atom_context *ctx = dev_priv->mode_info.atom_context;
		atom_asic_init(ctx);
	} else {
		radeon_combios_asic_init(dev);
	}

	for (i = 0; i < 8; i++)
		RADEON_WRITE(RADEON_BIOS_0_SCRATCH + (i * 4), dev_priv->pmregs.bios_scratch[i]);

	/* VGA render mayhaps */
	if (dev_priv->chip_family >= CHIP_RS600) {
		uint32_t tmp;

		RADEON_WRITE(AVIVO_D1VGA_CONTROL, 0);
		RADEON_WRITE(AVIVO_D2VGA_CONTROL, 0);
		tmp = RADEON_READ(0x300);
		tmp &= ~(3 << 16);
		RADEON_WRITE(0x300, tmp);
		RADEON_WRITE(0x308, (1 << 8));
		RADEON_WRITE(0x310, dev_priv->fb_location);
		RADEON_WRITE(0x594, 0);
	}

	RADEON_WRITE(RADEON_CRTC_EXT_CNTL, dev_priv->pmregs.crtc_ext_cntl);

	radeon_static_clocks_init(dev);
	
	radeon_init_memory_map(dev);

	if (dev_priv->flags & RADEON_IS_PCIE) {
		memcpy_toio(dev_priv->mm.pcie_table.kmap.virtual, dev_priv->mm.pcie_table_backup, RADEON_PCIGART_TABLE_SIZE);
	}

	if (dev_priv->mm.ring.kmap.virtual)
		memset(dev_priv->mm.ring.kmap.virtual, 0, RADEON_DEFAULT_RING_SIZE);

	if (dev_priv->mm.ring_read.kmap.virtual)
		memset(dev_priv->mm.ring_read.kmap.virtual, 0, PAGE_SIZE);

	radeon_modeset_cp_resume(dev);

	/* reset swi reg */
	RADEON_WRITE(RADEON_LAST_SWI_REG, dev_priv->counter);

//	radeon_enable_interrupt(dev);

	/* reset the context for userspace */
	if (dev->primary->master) {
		struct drm_radeon_master_private *master_priv = dev->primary->master->driver_priv;
		if (master_priv->sarea_priv)
			master_priv->sarea_priv->ctx_owner = 0;
	}

	/* unpin the front buffers */
	list_for_each_entry(fb, &dev->mode_config.fb_kernel_list, filp_head) {
		
		struct radeon_framebuffer *radeon_fb = to_radeon_framebuffer(fb);

		if (!radeon_fb)
			continue;

		if (!radeon_fb->base.mm_private)
			continue;
		
		radeon_gem_object_pin(radeon_fb->base.mm_private,
				      PAGE_SIZE, RADEON_GEM_DOMAIN_VRAM);
	}
	/* blat the mode back in */
	drm_helper_resume_force_mode(dev);

	return 0;
}

bool radeon_set_pcie_lanes(struct drm_device *dev, int lanes)
{
	drm_radeon_private_t *dev_priv = dev->dev_private;
	uint32_t link_width_cntl, mask;

	/* FIXME wait for idle */


	switch (lanes) {
	case 0:
		mask = RADEON_PCIE_LC_LINK_WIDTH_X0;
		break;
	case 1:
		mask = RADEON_PCIE_LC_LINK_WIDTH_X1;
		break;
	case 2:
		mask = RADEON_PCIE_LC_LINK_WIDTH_X2;
		break;
	case 4:
		mask = RADEON_PCIE_LC_LINK_WIDTH_X4;
		break;
	case 8:
		mask = RADEON_PCIE_LC_LINK_WIDTH_X8;
		break;
	case 12:
		mask = RADEON_PCIE_LC_LINK_WIDTH_X12;
		break;
	case 16:
	default:
		mask = RADEON_PCIE_LC_LINK_WIDTH_X16;
		break;
	}

	link_width_cntl = RADEON_READ_PCIE(dev_priv, RADEON_PCIE_LC_LINK_WIDTH_CNTL);

	if ((link_width_cntl & RADEON_PCIE_LC_LINK_WIDTH_RD_MASK) ==
	    (mask << RADEON_PCIE_LC_LINK_WIDTH_RD_SHIFT))
		return true;

	link_width_cntl &= ~(RADEON_PCIE_LC_LINK_WIDTH_MASK |
			     RADEON_PCIE_LC_RECONFIG_NOW |
			     RADEON_PCIE_LC_RECONFIG_LATER |
			     RADEON_PCIE_LC_SHORT_RECONFIG_EN);
	link_width_cntl |= mask;
	RADEON_WRITE_PCIE(RADEON_PCIE_LC_LINK_WIDTH_CNTL, link_width_cntl);
	RADEON_WRITE_PCIE(RADEON_PCIE_LC_LINK_WIDTH_CNTL, link_width_cntl | RADEON_PCIE_LC_RECONFIG_NOW);

	/* wait for lane set to complete */
	link_width_cntl = RADEON_READ_PCIE(dev_priv, RADEON_PCIE_LC_LINK_WIDTH_CNTL);
	while (link_width_cntl == 0xffffffff)
		link_width_cntl = RADEON_READ_PCIE(dev_priv, RADEON_PCIE_LC_LINK_WIDTH_CNTL);

	if ((link_width_cntl & RADEON_PCIE_LC_LINK_WIDTH_RD_MASK) ==
	    (mask << RADEON_PCIE_LC_LINK_WIDTH_RD_SHIFT))
		return true;
	else
		return false;
}

