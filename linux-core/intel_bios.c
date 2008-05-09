/*
 * Copyright © 2006 Intel Corporation
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
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Authors:
 *    Eric Anholt <eric@anholt.net>
 *
 */
#include "drmP.h"
#include "drm.h"
#include "i915_drm.h"
#include "i915_drv.h"
#include "intel_bios.h"

#define VBT_OFFSET 0x1a

/**
 * intel_find_vbt - find the VBT
 * @dev: DRM device
 *
 * Loads the Video BIOS and checks that the VBT exists.
 *
 * VBT existence is a sanity check that is relied on by other i830_bios.c code.
 * Note that it would be better to use a BIOS call to get the VBT, as BIOSes may
 * feed an updated VBT back through that, compared to what we'll fetch using
 * this method of groping around in the BIOS data.
 *
 * Returns 0 on success, nonzero on failure.
 */
bool
intel_find_bios(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct pci_dev *pdev = dev->pdev;
	void __iomem *bios;
	size_t size;

	bios = pci_map_rom(pdev, &size);
	if (!bios)
		return NULL;

	dev_priv->vbt = (struct vbt_header *)((u8 *)bios + VBT_OFFSET);
	dev_priv->bdb = (struct bdb_header *)((u8 *)bios + VBT_OFFSET + 
					      dev_priv->vbt->bdb_offset);
	
	if (memcmp(dev_priv->vbt->signature, "$VBT", 4) != 0) {
		DRM_ERROR("Bad VBT signature\n");
		pci_unmap_rom(pdev, bios);
		return NULL;
	}

	return bios;
}

#if 0
/**
 * Returns the BIOS's fixed panel mode.
 *
 * Note that many BIOSes will have the appropriate tables for a panel even when
 * a panel is not attached.  Additionally, many BIOSes adjust table sizes or
 * offsets, such that this parsing fails.  Thus, almost any other method for
 * detecting the panel mode is preferable.
 */
struct drm_display_mode *
i830_bios_get_panel_mode(struct drm_device *dev, bool *wants_dither)
{
    I830Ptr pI830 = I830PTR(pScrn);
    struct vbt_header *vbt;
    struct bdb_header *bdb;
    int vbt_off, bdb_off, bdb_block_off, block_size;
    int panel_type = -1;
    unsigned char *bios;

    bios = i830_bios_get (pScrn);

    if (bios == NULL)
	return NULL;

    vbt_off = INTEL_BIOS_16(0x1a);
    vbt = (struct vbt_header *)(bios + vbt_off);
    bdb_off = vbt_off + vbt->bdb_offset;
    bdb = (struct bdb_header *)(bios + bdb_off);

    if (memcmp(bdb->signature, "BIOS_DATA_BLOCK ", 16) != 0) {
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Bad BDB signature\n");
	xfree(bios);
	return NULL;
    }

    *wants_dither = FALSE;
    for (bdb_block_off = bdb->header_size; bdb_block_off < bdb->bdb_size;
	 bdb_block_off += block_size)
    {
	int start = bdb_off + bdb_block_off;
	int id;
	struct lvds_bdb_1 *lvds1;
	struct lvds_bdb_2 *lvds2;
	struct lvds_bdb_2_fp_params *fpparam;
	struct lvds_bdb_2_fp_edid_dtd *fptiming;
	DisplayModePtr fixed_mode;
	CARD8 *timing_ptr;

	id = INTEL_BIOS_8(start);
	block_size = INTEL_BIOS_16(start + 1) + 3;
	switch (id) {
	case 40:
	    lvds1 = (struct lvds_bdb_1 *)(bios + start);
	    panel_type = lvds1->panel_type;
	    if (lvds1->caps & LVDS_CAP_DITHER)
		*wants_dither = TRUE;
	    break;
	case 41:
	    if (panel_type == -1)
		break;

	    lvds2 = (struct lvds_bdb_2 *)(bios + start);
	    fpparam = (struct lvds_bdb_2_fp_params *)(bios +
		bdb_off + lvds2->panels[panel_type].fp_params_offset);
	    fptiming = (struct lvds_bdb_2_fp_edid_dtd *)(bios +
		bdb_off + lvds2->panels[panel_type].fp_edid_dtd_offset);
	    timing_ptr = bios + bdb_off +
	        lvds2->panels[panel_type].fp_edid_dtd_offset;

	    if (fpparam->terminator != 0xffff) {
		/* Apparently the offsets are wrong for some BIOSes, so we
		 * try the other offsets if we find a bad terminator.
		 */
		fpparam = (struct lvds_bdb_2_fp_params *)(bios +
		    bdb_off + lvds2->panels[panel_type].fp_params_offset + 8);
		fptiming = (struct lvds_bdb_2_fp_edid_dtd *)(bios +
		    bdb_off + lvds2->panels[panel_type].fp_edid_dtd_offset + 8);
		timing_ptr = bios + bdb_off +
	            lvds2->panels[panel_type].fp_edid_dtd_offset + 8;

		if (fpparam->terminator != 0xffff)
		    continue;
	    }

	    fixed_mode = xnfalloc(sizeof(DisplayModeRec));
	    memset(fixed_mode, 0, sizeof(*fixed_mode));

	    /* Since lvds_bdb_2_fp_edid_dtd is just an EDID detailed timing
	     * block, pull the contents out using EDID macros.
	     */
	    fixed_mode->HDisplay   = _H_ACTIVE(timing_ptr);
	    fixed_mode->VDisplay   = _V_ACTIVE(timing_ptr);
	    fixed_mode->HSyncStart = fixed_mode->HDisplay +
		_H_SYNC_OFF(timing_ptr);
	    fixed_mode->HSyncEnd   = fixed_mode->HSyncStart +
		_H_SYNC_WIDTH(timing_ptr);
	    fixed_mode->HTotal     = fixed_mode->HDisplay +
	        _H_BLANK(timing_ptr);
	    fixed_mode->VSyncStart = fixed_mode->VDisplay +
		_V_SYNC_OFF(timing_ptr);
	    fixed_mode->VSyncEnd   = fixed_mode->VSyncStart +
		_V_SYNC_WIDTH(timing_ptr);
	    fixed_mode->VTotal     = fixed_mode->VDisplay +
	        _V_BLANK(timing_ptr);
	    fixed_mode->Clock      = _PIXEL_CLOCK(timing_ptr) / 1000;
	    fixed_mode->type       = M_T_PREFERRED;

	    xf86SetModeDefaultName(fixed_mode);

	    if (pI830->debug_modes) {
		xf86DrvMsg(pScrn->scrnIndex, X_INFO,
			   "Found panel mode in BIOS VBT tables:\n");
		xf86PrintModeline(pScrn->scrnIndex, fixed_mode);
	    }

	    xfree(bios);
	    return fixed_mode;
	}
    }

    xfree(bios);
    return NULL;
}

unsigned char *
i830_bios_get_aim_data_block (ScrnInfoPtr pScrn, int aim, int data_block)
{
    unsigned char   *bios;
    int		    bdb_off;
    int		    vbt_off;
    int		    aim_off;
    struct vbt_header *vbt;
    struct aimdb_header *aimdb;
    struct aimdb_block *aimdb_block;

    bios = i830_bios_get (pScrn);
    if (!bios)
	return NULL;

    vbt_off = INTEL_BIOS_16(0x1a);
    vbt = (struct vbt_header *)(bios + vbt_off);

    aim_off = vbt->aim_offset[aim];
    if (!aim_off)
    {
	free (bios);
	return NULL;
    }
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "aim_off %d\n", aim_off);
    aimdb = (struct aimdb_header *) (bios + vbt_off + aim_off);
    bdb_off = aimdb->aimdb_header_size;
    while (bdb_off < aimdb->aimdb_size)
    {
	aimdb_block = (struct aimdb_block *) (bios + vbt_off + aim_off + bdb_off);
	if (aimdb_block->aimdb_id == data_block)
	{
	    unsigned char   *aim = malloc (aimdb_block->aimdb_size + sizeof (struct aimdb_block));
	    if (!aim)
	    {
		free (bios);
		return NULL;
	    }
	    memcpy (aim, aimdb_block, aimdb_block->aimdb_size + sizeof (struct aimdb_block));
	    free (bios);
	    return aim;
	}
	bdb_off += aimdb_block->aimdb_size + sizeof (struct aimdb_block);
    }
    free (bios);
    return NULL;
}
#endif
