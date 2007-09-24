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
#include "intel_drv.h"
#include "i915_drv.h"

#include "drm_pciids.h"

static struct pci_device_id pciidlist[] = {
	i915_PCI_IDS
};

#ifdef I915_HAVE_FENCE
static struct drm_fence_driver i915_fence_driver = {
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

static uint32_t i915_mem_prios[] = {DRM_BO_MEM_VRAM, DRM_BO_MEM_PRIV0, DRM_BO_MEM_TT, DRM_BO_MEM_LOCAL};
static uint32_t i915_busy_prios[] = {DRM_BO_MEM_TT, DRM_BO_MEM_PRIV0, DRM_BO_MEM_VRAM, DRM_BO_MEM_LOCAL};

static struct drm_bo_driver i915_bo_driver = {
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

static int i915_suspend(struct pci_dev *pdev, pm_message_t state)
{
	struct drm_device *dev = pci_get_drvdata(pdev);
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct drm_output *output;
	int i;

	pci_save_state(pdev);

	/* Save video mode information for native mode-setting. */
	dev_priv->saveDSPACNTR = I915_READ(DSPACNTR);
	dev_priv->savePIPEACONF = I915_READ(PIPEACONF);
	dev_priv->savePIPEASRC = I915_READ(PIPEASRC);
	dev_priv->saveFPA0 = I915_READ(FPA0);
	dev_priv->saveFPA1 = I915_READ(FPA1);
	dev_priv->saveDPLL_A = I915_READ(DPLL_A);
	if (IS_I965G(dev))
		dev_priv->saveDPLL_A_MD = I915_READ(DPLL_A_MD);
	dev_priv->saveHTOTAL_A = I915_READ(HTOTAL_A);
	dev_priv->saveHBLANK_A = I915_READ(HBLANK_A);
	dev_priv->saveHSYNC_A = I915_READ(HSYNC_A);
	dev_priv->saveVTOTAL_A = I915_READ(VTOTAL_A);
	dev_priv->saveVBLANK_A = I915_READ(VBLANK_A);
	dev_priv->saveVSYNC_A = I915_READ(VSYNC_A);
	dev_priv->saveDSPASTRIDE = I915_READ(DSPASTRIDE);
	dev_priv->saveDSPASIZE = I915_READ(DSPASIZE);
	dev_priv->saveDSPAPOS = I915_READ(DSPAPOS);
	dev_priv->saveDSPABASE = I915_READ(DSPABASE);

	for(i= 0; i < 256; i++)
		dev_priv->savePaletteA[i] = I915_READ(PALETTE_A + (i << 2));

	if(dev->mode_config.num_crtc == 2) {
		dev_priv->savePIPEBCONF = I915_READ(PIPEBCONF);
		dev_priv->savePIPEBSRC = I915_READ(PIPEBSRC);
		dev_priv->saveDSPBCNTR = I915_READ(DSPBCNTR);
		dev_priv->saveFPB0 = I915_READ(FPB0);
		dev_priv->saveFPB1 = I915_READ(FPB1);
		dev_priv->saveDPLL_B = I915_READ(DPLL_B);
		if (IS_I965G(dev))
			dev_priv->saveDPLL_B_MD = I915_READ(DPLL_B_MD);
		dev_priv->saveHTOTAL_B = I915_READ(HTOTAL_B);
		dev_priv->saveHBLANK_B = I915_READ(HBLANK_B);
		dev_priv->saveHSYNC_B = I915_READ(HSYNC_B);
		dev_priv->saveVTOTAL_B = I915_READ(VTOTAL_B);
		dev_priv->saveVBLANK_B = I915_READ(VBLANK_B);
		dev_priv->saveVSYNC_B = I915_READ(VSYNC_B);
		dev_priv->saveDSPBSTRIDE = I915_READ(DSPBSTRIDE);
		dev_priv->saveDSPBSIZE = I915_READ(DSPBSIZE);
		dev_priv->saveDSPBPOS = I915_READ(DSPBPOS);
		dev_priv->saveDSPBBASE = I915_READ(DSPBBASE);
		for(i= 0; i < 256; i++)
			dev_priv->savePaletteB[i] =
				I915_READ(PALETTE_B + (i << 2));
	}

	if (IS_I965G(dev)) {
		dev_priv->saveDSPASURF = I915_READ(DSPASURF);
		dev_priv->saveDSPBSURF = I915_READ(DSPBSURF);
	}

	dev_priv->saveVCLK_DIVISOR_VGA0 = I915_READ(VCLK_DIVISOR_VGA0);
	dev_priv->saveVCLK_DIVISOR_VGA1 = I915_READ(VCLK_DIVISOR_VGA1);
	dev_priv->saveVCLK_POST_DIV = I915_READ(VCLK_POST_DIV);
	dev_priv->saveVGACNTRL = I915_READ(VGACNTRL);

	for(i = 0; i < 7; i++) {
		dev_priv->saveSWF[i] = I915_READ(SWF0 + (i << 2));
		dev_priv->saveSWF[i+7] = I915_READ(SWF00 + (i << 2));
	}
	dev_priv->saveSWF[14] = I915_READ(SWF30);
	dev_priv->saveSWF[15] = I915_READ(SWF31);
	dev_priv->saveSWF[16] = I915_READ(SWF32);

	if (IS_MOBILE(dev) && !IS_I830(dev))
		dev_priv->saveLVDS = I915_READ(LVDS);
	dev_priv->savePFIT_CONTROL = I915_READ(PFIT_CONTROL);

	list_for_each_entry(output, &dev->mode_config.output_list, head)
		if (output->funcs->save)
			(*output->funcs->save) (output);

#if 0 /* FIXME: save VGA bits */
	vgaHWUnlock(hwp);
	vgaHWSave(pScrn, vgaReg, VGA_SR_FONTS);
#endif
	pci_disable_device(pdev);
	pci_set_power_state(pdev, PCI_D3hot);

	return 0;
}

static int i915_resume(struct pci_dev *pdev)
{
	struct drm_device *dev = pci_get_drvdata(pdev);
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct drm_output *output;
	struct drm_crtc *crtc;
	int i;

	pci_set_power_state(pdev, PCI_D0);
	pci_restore_state(pdev);
	if (pci_enable_device(pdev))
		return -1;

	/* Disable outputs */
	list_for_each_entry(output, &dev->mode_config.output_list, head)
		output->funcs->dpms(output, DPMSModeOff);

	i915_driver_wait_next_vblank(dev, 0);
   
	/* Disable pipes */
	list_for_each_entry(crtc, &dev->mode_config.crtc_list, head)
		crtc->funcs->dpms(crtc, DPMSModeOff);

	/* FIXME: wait for vblank on each pipe? */
	i915_driver_wait_next_vblank(dev, 0);

	if (IS_MOBILE(dev) && !IS_I830(dev))
		I915_WRITE(LVDS, dev_priv->saveLVDS);

	if (!IS_I830(dev) && !IS_845G(dev))
		I915_WRITE(PFIT_CONTROL, dev_priv->savePFIT_CONTROL);

	if (dev_priv->saveDPLL_A & DPLL_VCO_ENABLE) {
		I915_WRITE(DPLL_A, dev_priv->saveDPLL_A & ~DPLL_VCO_ENABLE);
		udelay(150);
	}
	I915_WRITE(FPA0, dev_priv->saveFPA0);
	I915_WRITE(FPA1, dev_priv->saveFPA1);
	I915_WRITE(DPLL_A, dev_priv->saveDPLL_A);
	udelay(150);
	if (IS_I965G(dev))
		I915_WRITE(DPLL_A_MD, dev_priv->saveDPLL_A_MD);
	else
		I915_WRITE(DPLL_A, dev_priv->saveDPLL_A);
	udelay(150);

	I915_WRITE(HTOTAL_A, dev_priv->saveHTOTAL_A);
	I915_WRITE(HBLANK_A, dev_priv->saveHBLANK_A);
	I915_WRITE(HSYNC_A, dev_priv->saveHSYNC_A);
	I915_WRITE(VTOTAL_A, dev_priv->saveVTOTAL_A);
	I915_WRITE(VBLANK_A, dev_priv->saveVBLANK_A);
	I915_WRITE(VSYNC_A, dev_priv->saveVSYNC_A);
   
	I915_WRITE(DSPASTRIDE, dev_priv->saveDSPASTRIDE);
	I915_WRITE(DSPASIZE, dev_priv->saveDSPASIZE);
	I915_WRITE(DSPAPOS, dev_priv->saveDSPAPOS);
	I915_WRITE(PIPEASRC, dev_priv->savePIPEASRC);
	I915_WRITE(DSPABASE, dev_priv->saveDSPABASE);
	if (IS_I965G(dev))
		I915_WRITE(DSPASURF, dev_priv->saveDSPASURF);
	I915_WRITE(PIPEACONF, dev_priv->savePIPEACONF);
	i915_driver_wait_next_vblank(dev, 0);
	I915_WRITE(DSPACNTR, dev_priv->saveDSPACNTR);
	I915_WRITE(DSPABASE, I915_READ(DSPABASE));
	i915_driver_wait_next_vblank(dev, 0);
   
	if(dev->mode_config.num_crtc == 2) {
		if (dev_priv->saveDPLL_B & DPLL_VCO_ENABLE) {
			I915_WRITE(DPLL_B, dev_priv->saveDPLL_B & ~DPLL_VCO_ENABLE);
			udelay(150);
		}
		I915_WRITE(FPB0, dev_priv->saveFPB0);
		I915_WRITE(FPB1, dev_priv->saveFPB1);
		I915_WRITE(DPLL_B, dev_priv->saveDPLL_B);
		udelay(150);
		if (IS_I965G(dev))
			I915_WRITE(DPLL_B_MD, dev_priv->saveDPLL_B_MD);
		else
			I915_WRITE(DPLL_B, dev_priv->saveDPLL_B);
		udelay(150);
   
		I915_WRITE(HTOTAL_B, dev_priv->saveHTOTAL_B);
		I915_WRITE(HBLANK_B, dev_priv->saveHBLANK_B);
		I915_WRITE(HSYNC_B, dev_priv->saveHSYNC_B);
		I915_WRITE(VTOTAL_B, dev_priv->saveVTOTAL_B);
		I915_WRITE(VBLANK_B, dev_priv->saveVBLANK_B);
		I915_WRITE(VSYNC_B, dev_priv->saveVSYNC_B);
		I915_WRITE(DSPBSTRIDE, dev_priv->saveDSPBSTRIDE);
		I915_WRITE(DSPBSIZE, dev_priv->saveDSPBSIZE);
		I915_WRITE(DSPBPOS, dev_priv->saveDSPBPOS);
		I915_WRITE(PIPEBSRC, dev_priv->savePIPEBSRC);
		I915_WRITE(DSPBBASE, dev_priv->saveDSPBBASE);
		if (IS_I965G(dev))
			I915_WRITE(DSPBSURF, dev_priv->saveDSPBSURF);
		I915_WRITE(PIPEBCONF, dev_priv->savePIPEBCONF);
		i915_driver_wait_next_vblank(dev, 0);
		I915_WRITE(DSPBCNTR, dev_priv->saveDSPBCNTR);
		I915_WRITE(DSPBBASE, I915_READ(DSPBBASE));
		i915_driver_wait_next_vblank(dev, 0);
	}

	/* Restore outputs */
	list_for_each_entry(output, &dev->mode_config.output_list, head)
		if (output->funcs->restore)
			output->funcs->restore(output);
    
	I915_WRITE(VGACNTRL, dev_priv->saveVGACNTRL);

	I915_WRITE(VCLK_DIVISOR_VGA0, dev_priv->saveVCLK_DIVISOR_VGA0);
	I915_WRITE(VCLK_DIVISOR_VGA1, dev_priv->saveVCLK_DIVISOR_VGA1);
	I915_WRITE(VCLK_POST_DIV, dev_priv->saveVCLK_POST_DIV);

	for(i = 0; i < 256; i++)
		I915_WRITE(PALETTE_A + (i << 2), dev_priv->savePaletteA[i]);
   
	if(dev->mode_config.num_crtc == 2)
		for(i= 0; i < 256; i++)
			I915_WRITE(PALETTE_B + (i << 2), dev_priv->savePaletteB[i]);

	for(i = 0; i < 7; i++) {
		I915_WRITE(SWF0 + (i << 2), dev_priv->saveSWF[i]);
		I915_WRITE(SWF00 + (i << 2), dev_priv->saveSWF[i+7]);
	}

	I915_WRITE(SWF30, dev_priv->saveSWF[14]);
	I915_WRITE(SWF31, dev_priv->saveSWF[15]);
	I915_WRITE(SWF32, dev_priv->saveSWF[16]);

#if 0 /* FIXME: restore VGA bits */
	vgaHWRestore(pScrn, vgaReg, VGA_SR_FONTS);
	vgaHWLock(hwp);
#endif

	return 0;
}

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
	.unload = i915_driver_unload,
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
	.fb_probe = intelfb_probe,
	.fb_remove = intelfb_remove,
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
		.suspend = i915_suspend,
		.resume = i915_resume,
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
