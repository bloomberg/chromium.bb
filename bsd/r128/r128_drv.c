/* r128_drv.c -- ATI Rage 128 driver -*- linux-c -*-
 * Created: Mon Dec 13 09:47:27 1999 by faith@precisioninsight.com
 *
 * Copyright 1999 Precision Insight, Inc., Cedar Park, Texas.
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
 *    Rickard E. (Rik) Faith <faith@valinux.com>
 *    Gareth Hughes <gareth@valinux.com>
 */


#include <sys/types.h>
#include <sys/bus.h>
#include <pci/pcivar.h>
#include <opt_drm_linux.h>

#include "r128.h"
#include "drmP.h"
#include "r128_drv.h"
#if __REALLY_HAVE_SG
#include "ati_pcigart.h"
#endif

#define DRIVER_AUTHOR		"Gareth Hughes, VA Linux Systems Inc."

#define DRIVER_NAME		"r128"
#define DRIVER_DESC		"ATI Rage 128"
#define DRIVER_DATE		"20010405"

#define DRIVER_MAJOR		2
#define DRIVER_MINOR		2
#define DRIVER_PATCHLEVEL	0

/* List acquired from http://www.yourvote.com/pci/pcihdr.h and xc/xc/programs/Xserver/hw/xfree86/common/xf86PciInfo.h
 * Please report to anholt@teleport.com inaccuracies or if a chip you have works that is marked unsupported here.
 */
drm_chipinfo_t DRM(devicelist)[] = {
	{0x1002, 0x4c45, 1, "ATI Rage 128 Mobility LE"},
	{0x1002, 0x4c46, 1, "ATI Rage 128 Mobility LF"},
	{0x1002, 0x4d46, 1, "ATI Rage 128 Mobility MF (AGP 4x)"},
	{0x1002, 0x4d4c, 1, "ATI Rage 128 Mobility ML"},
	{0x1002, 0x5041, 0, "ATI Rage 128 Pro PA (PCI)"},
	{0x1002, 0x5042, 1, "ATI Rage 128 Pro PB (AGP 2x)"},
	{0x1002, 0x5043, 1, "ATI Rage 128 Pro PC (AGP 4x)"},
	{0x1002, 0x5044, 0, "ATI Rage 128 Pro PD (PCI)"},
	{0x1002, 0x5045, 1, "ATI Rage 128 Pro PE (AGP 2x)"},
	{0x1002, 0x5046, 1, "ATI Rage 128 Pro PF (AGP 4x)"},
	{0x1002, 0x5047, 0, "ATI Rage 128 Pro PG (PCI)"},
	{0x1002, 0x5048, 1, "ATI Rage 128 Pro PH (AGP)"},
	{0x1002, 0x5049, 1, "ATI Rage 128 Pro PI (AGP)"},
	{0x1002, 0x504a, 0, "ATI Rage 128 Pro PJ (PCI)"},
	{0x1002, 0x504b, 1, "ATI Rage 128 Pro PK (AGP)"},
	{0x1002, 0x504c, 1, "ATI Rage 128 Pro PL (AGP)"},
	{0x1002, 0x504d, 0, "ATI Rage 128 Pro PM (PCI)"},
	{0x1002, 0x504e, 1, "ATI Rage 128 Pro PN (AGP)"},
	{0x1002, 0x505f, 1, "ATI Rage 128 Pro PO (AGP)"},
	{0x1002, 0x5050, 0, "ATI Rage 128 Pro PP (PCI)"},
	{0x1002, 0x5051, 1, "ATI Rage 128 Pro PQ (AGP)"},
	{0x1002, 0x5052, 1, "ATI Rage 128 Pro PR (AGP)"},
	{0x1002, 0x5053, 0, "ATI Rage 128 Pro PS (PCI)"},
	{0x1002, 0x5054, 1, "ATI Rage 128 Pro PT (AGP)"},
	{0x1002, 0x5055, 1, "ATI Rage 128 Pro PU (AGP)"},
	{0x1002, 0x5056, 0, "ATI Rage 128 Pro PV (PCI)"},
	{0x1002, 0x5057, 1, "ATI Rage 128 Pro PW (AGP)"},
	{0x1002, 0x5058, 1, "ATI Rage 128 Pro PX (AGP)"},
	{0x1002, 0x5245, 0, "ATI Rage 128 GL (PCI)"},
	{0x1002, 0x5246, 1, "ATI Rage 128 GL (AGP 2x)"},
	{0x1002, 0x524b, 0, "ATI Rage 128 VR (PCI)"},
	{0x1002, 0x524c, 1, "ATI Rage 128 VR (AGP 2x)"},
	{0x1002, 0x5345, 0, "ATI Rage 128 SE (PCI)"},
	{0x1002, 0x5346, 1, "ATI Rage 128 SF (AGP 2x)"},
	{0x1002, 0x5347, 1, "ATI Rage 128 SG (AGP 4x)"},
	{0x1002, 0x5348, 0, "ATI Rage 128 SH (unknown)"},
	{0x1002, 0x534b, 0, "ATI Rage 128 SK (PCI)"},
	{0x1002, 0x534c, 1, "ATI Rage 128 SL (AGP 2x)"},
	{0x1002, 0x534d, 1, "ATI Rage 128 SM (AGP 4x)"},
	{0x1002, 0x534e, 1, "ATI Rage 128 (AGP 4x?)"},
	{0, 0, 0, NULL}
};

#define DRIVER_IOCTLS							    \
   [DRM_IOCTL_NR(DRM_IOCTL_DMA)]             = { r128_cce_buffers,  1, 0 }, \
   [DRM_IOCTL_NR(DRM_IOCTL_R128_INIT)]       = { r128_cce_init,     1, 1 }, \
   [DRM_IOCTL_NR(DRM_IOCTL_R128_CCE_START)]  = { r128_cce_start,    1, 1 }, \
   [DRM_IOCTL_NR(DRM_IOCTL_R128_CCE_STOP)]   = { r128_cce_stop,     1, 1 }, \
   [DRM_IOCTL_NR(DRM_IOCTL_R128_CCE_RESET)]  = { r128_cce_reset,    1, 1 }, \
   [DRM_IOCTL_NR(DRM_IOCTL_R128_CCE_IDLE)]   = { r128_cce_idle,     1, 0 }, \
   [DRM_IOCTL_NR(DRM_IOCTL_R128_RESET)]      = { r128_engine_reset, 1, 0 }, \
   [DRM_IOCTL_NR(DRM_IOCTL_R128_FULLSCREEN)] = { r128_fullscreen,   1, 0 }, \
   [DRM_IOCTL_NR(DRM_IOCTL_R128_SWAP)]       = { r128_cce_swap,     1, 0 }, \
   [DRM_IOCTL_NR(DRM_IOCTL_R128_CLEAR)]      = { r128_cce_clear,    1, 0 }, \
   [DRM_IOCTL_NR(DRM_IOCTL_R128_VERTEX)]     = { r128_cce_vertex,   1, 0 }, \
   [DRM_IOCTL_NR(DRM_IOCTL_R128_INDICES)]    = { r128_cce_indices,  1, 0 }, \
   [DRM_IOCTL_NR(DRM_IOCTL_R128_BLIT)]       = { r128_cce_blit,     1, 0 }, \
   [DRM_IOCTL_NR(DRM_IOCTL_R128_DEPTH)]      = { r128_cce_depth,    1, 0 }, \
   [DRM_IOCTL_NR(DRM_IOCTL_R128_STIPPLE)]    = { r128_cce_stipple,  1, 0 }, \
   [DRM_IOCTL_NR(DRM_IOCTL_R128_INDIRECT)]   = { r128_cce_indirect, 1, 1 },


#if 0
/* GH: Count data sent to card via ring or vertex/indirect buffers.
 */
#define __HAVE_COUNTERS         3
#define __HAVE_COUNTER6         _DRM_STAT_IRQ
#define __HAVE_COUNTER7         _DRM_STAT_PRIMARY
#define __HAVE_COUNTER8         _DRM_STAT_SECONDARY
#endif


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
#include "drm_sysctl.h"
#include "drm_vm.h"
#if __REALLY_HAVE_SG
#include "drm_scatter.h"
#endif

DRIVER_MODULE(r128, pci, r128_driver, r128_devclass, 0, 0);
