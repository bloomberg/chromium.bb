/* i810_drv.h -- Private header for the Matrox g200/g400 driver -*- linux-c -*-
 * Created: Mon Dec 13 01:50:01 1999 by jhartmann@precisioninsight.com
 *
 * Copyright 1999 Precision Insight, Inc., Cedar Park, Texas.
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
 * Authors: Rickard E. (Rik) Faith <faith@precisioninsight.com>
 * 	    Jeff Hartmann <jhartmann@precisioninsight.com>
 *
 * $XFree86: xc/programs/Xserver/hw/xfree86/os-support/linux/drm/kernel/i810_drv.h,v 1.1 2000/02/11 17:26:05 dawes Exp $
 */

#ifndef _I810_DRV_H_
#define _I810_DRV_H_

				/* i810_drv.c */
extern int  i810_init(void);
extern void i810_cleanup(void);
extern int  i810_version(struct inode *inode, struct file *filp,
			  unsigned int cmd, unsigned long arg);
extern int  i810_open(struct inode *inode, struct file *filp);
extern int  i810_release(struct inode *inode, struct file *filp);
extern int  i810_ioctl(struct inode *inode, struct file *filp,
			unsigned int cmd, unsigned long arg);
extern int  i810_unlock(struct inode *inode, struct file *filp,
			 unsigned int cmd, unsigned long arg);

				/* i810_dma.c */
extern int  i810_dma_schedule(drm_device_t *dev, int locked);
extern int  i810_dma(struct inode *inode, struct file *filp,
		      unsigned int cmd, unsigned long arg);
extern int  i810_irq_install(drm_device_t *dev, int irq);
extern int  i810_irq_uninstall(drm_device_t *dev);
extern int  i810_control(struct inode *inode, struct file *filp,
			  unsigned int cmd, unsigned long arg);
extern int  i810_lock(struct inode *inode, struct file *filp,
		       unsigned int cmd, unsigned long arg);
extern void i810_dma_init(drm_device_t *dev);
extern void i810_dma_cleanup(drm_device_t *dev);


				/* i810_bufs.c */
extern int  i810_addbufs(struct inode *inode, struct file *filp, 
			unsigned int cmd, unsigned long arg);
extern int  i810_infobufs(struct inode *inode, struct file *filp, 
			 unsigned int cmd, unsigned long arg);
extern int  i810_markbufs(struct inode *inode, struct file *filp,
			 unsigned int cmd, unsigned long arg);
extern int  i810_freebufs(struct inode *inode, struct file *filp,
			 unsigned int cmd, unsigned long arg);
extern int  i810_mapbufs(struct inode *inode, struct file *filp,
			unsigned int cmd, unsigned long arg);
extern int  i810_addmap(struct inode *inode, struct file *filp,
		       unsigned int cmd, unsigned long arg);


#endif
