/* gamma_drv.h -- Private header for 3dlabs GMX 2000 driver -*- c -*-
 * Created: Mon Jan  4 10:05:05 1999 by faith@precisioninsight.com
 *
 * Copyright 1999 Precision Insight, Inc., Cedar Park, Texas.
 * Copyright 2000 VA Linux Systems, Inc., Sunnyvale, California.
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
 * PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 * 
 */

#ifndef _GAMMA_DRV_H_
#define _GAMMA_DRV_H_

				/* gamma_drv.c */
extern d_open_t gamma_open;
extern d_close_t gamma_close;
extern d_ioctl_t gamma_ioctl;
extern d_ioctl_t gamma_version;
extern d_ioctl_t gamma_dma;
extern d_ioctl_t gamma_lock;
extern d_ioctl_t gamma_unlock;
extern d_ioctl_t gamma_control;

				/* gamma_dma.c */
extern int  gamma_dma_schedule(drm_device_t *dev, int locked);
extern int  gamma_irq_install(drm_device_t *dev, int irq);
extern int  gamma_irq_uninstall(drm_device_t *dev);
extern int  gamma_found(void);

#endif
