/* sis_drv.h -- Private header for sis driver -*- linux-c -*-
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

#ifndef _SIS_DRV_H_
#define _SIS_DRV_H_

				/* sis_drv.c */
extern int  sis_version(struct inode *inode, struct file *filp,
			  unsigned int cmd, unsigned long arg);
extern int  sis_open(struct inode *inode, struct file *filp);
extern int  sis_release(struct inode *inode, struct file *filp);
extern int  sis_ioctl(struct inode *inode, struct file *filp,
			unsigned int cmd, unsigned long arg);
extern int  sis_irq_install(drm_device_t *dev, int irq);
extern int  sis_irq_uninstall(drm_device_t *dev);
extern int  sis_control(struct inode *inode, struct file *filp,
			  unsigned int cmd, unsigned long arg);
extern int  sis_lock(struct inode *inode, struct file *filp,
		       unsigned int cmd, unsigned long arg);
extern int  sis_unlock(struct inode *inode, struct file *filp,
			 unsigned int cmd, unsigned long arg);

				/* sis_context.c */

extern int  sis_resctx(struct inode *inode, struct file *filp,
			unsigned int cmd, unsigned long arg);
extern int  sis_addctx(struct inode *inode, struct file *filp,
		        unsigned int cmd, unsigned long arg);
extern int  sis_modctx(struct inode *inode, struct file *filp,
		        unsigned int cmd, unsigned long arg);
extern int  sis_getctx(struct inode *inode, struct file *filp,
		        unsigned int cmd, unsigned long arg);
extern int  sis_switchctx(struct inode *inode, struct file *filp,
			   unsigned int cmd, unsigned long arg);
extern int  sis_newctx(struct inode *inode, struct file *filp,
			unsigned int cmd, unsigned long arg);
extern int  sis_rmctx(struct inode *inode, struct file *filp,
		       unsigned int cmd, unsigned long arg);

extern int  sis_context_switch(drm_device_t *dev, int old, int new);
extern int  sis_context_switch_complete(drm_device_t *dev, int new);

int sis_fb_alloc(struct inode *inode, struct file *filp, unsigned int cmd,
		  unsigned long arg);
int sis_fb_free(struct inode *inode, struct file *filp, unsigned int cmd,
		  unsigned long arg);

int sis_agp_init(struct inode *inode, struct file *filp, unsigned int cmd,
		  unsigned long arg);
int sis_agp_alloc(struct inode *inode, struct file *filp, unsigned int cmd,
		  unsigned long arg);
int sis_agp_free(struct inode *inode, struct file *filp, unsigned int cmd,
		  unsigned long arg);

int sis_flip(struct inode *inode, struct file *filp, unsigned int cmd,
		  unsigned long arg);
int sis_flip_init(struct inode *inode, struct file *filp, unsigned int cmd,
		  unsigned long arg);
int sis_flip_final(struct inode *inode, struct file *filp, unsigned int cmd,
		  unsigned long arg);
void flip_final(void);
		 		  
int sis_init_context(int contexy);
int sis_final_context(int context);

#endif
