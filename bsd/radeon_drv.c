/* radeon_drv.c -- ATI Radeon driver -*- linux-c -*-
 * Created: Wed Feb 14 17:10:04 2001 by gareth@valinux.com
 *
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
 *    Gareth Hughes <gareth@valinux.com>
 */

#include <sys/types.h>

#include "radeon.h"
#include "drmP.h"
#include "drm.h"
#include "radeon_drm.h"
#include "radeon_drv.h"
#if __REALLY_HAVE_SG
#include "ati_pcigart.h"
#endif

/* List acquired from http://www.yourvote.com/pci/pcihdr.h and xc/xc/programs/Xserver/hw/xfree86/common/xf86PciInfo.h
 * Please report to eta@lclark.edu inaccuracies or if a chip you have works that is marked unsupported here.
 */
drm_chipinfo_t DRM(devicelist)[] = {
	{0x1002, 0x4C57, 1, "ATI Radeon LW Mobility 7 (AGP)"},
	{0x1002, 0x4C59, 1, "ATI Radeon LY Mobility 6 (AGP)"},
	{0x1002, 0x4C5A, 1, "ATI Radeon LZ Mobility 6 (AGP)"},
	{0x1002, 0x5144, 1, "ATI Radeon QD (AGP)"},
	{0x1002, 0x5145, 1, "ATI Radeon QE (AGP)"},
	{0x1002, 0x5146, 1, "ATI Radeon QF (AGP)"},
	{0x1002, 0x5147, 1, "ATI Radeon QG (AGP)"},
	{0x1002, 0x5157, 1, "ATI Radeon QW 7500 (AGP)"},
	{0x1002, 0x5159, 1, "ATI Radeon QY VE (AGP)"},
	{0x1002, 0x515A, 1, "ATI Radeon QZ VE (AGP)"},
	{0, 0, 0, NULL}
};

#include "drm_agpsupport.h"
#include "drm_auth.h"
#include "drm_bufs.h"
#include "drm_context.h"
#include "drm_dma.h"
#include "drm_drawable.h"
#include "drm_drv.h"
#include "drm_fops.h"
#include "drm_init.h"
#include "drm_ioctl.h"
#include "drm_lock.h"
#include "drm_memory.h"
#include "drm_vm.h"
#include "drm_sysctl.h"
#if __HAVE_SG
#include "drm_scatter.h"
#endif

DRIVER_MODULE(DRIVER_NAME, pci, DRM(driver), DRM(devclass), 0, 0);
