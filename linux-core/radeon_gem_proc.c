/*
 * Copyright © 2008 Intel Corporation
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
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 * Authors:
 *    Eric Anholt <eric@anholt.net>
 *    Keith Packard <keithp@keithp.com>
 *
 */

#include "drmP.h"
#include "drm.h"
#include "radeon_drm.h"
#include "radeon_drv.h"


static int radeon_ring_info(char *buf, char **start, off_t offset,
			       int request, int *eof, void *data)
{
	struct drm_minor *minor = (struct drm_minor *) data;
	struct drm_device *dev = minor->dev;
	drm_radeon_private_t *dev_priv = dev->dev_private;
	int len = 0;

	if (offset > DRM_PROC_LIMIT) {
		*eof = 1;
		return 0;
	}

	*start = &buf[offset];
	*eof = 0;
	DRM_PROC_PRINT("RADEON_CP_RB_WPTR %08x\n",
		       RADEON_READ(RADEON_CP_RB_WPTR));

	DRM_PROC_PRINT("RADEON_CP_RB_RPTR %08x\n",
		       RADEON_READ(RADEON_CP_RB_RPTR));

	
	if (len > request + offset)
		return request;
	*eof = 1;
	return len - offset;
}

static int radeon_interrupt_info(char *buf, char **start, off_t offset,
			       int request, int *eof, void *data)
{
	struct drm_minor *minor = (struct drm_minor *) data;
	struct drm_device *dev = minor->dev;
	drm_radeon_private_t *dev_priv = dev->dev_private;
	int len = 0;

	if (offset > DRM_PROC_LIMIT) {
		*eof = 1;
		return 0;
	}

	*start = &buf[offset];
	*eof = 0;
	DRM_PROC_PRINT("Interrupt enable:    %08x\n",
		       RADEON_READ(RADEON_GEN_INT_CNTL));

	if (dev_priv->chip_family >= CHIP_RS690) {
	  DRM_PROC_PRINT("DxMODE_INT_MASK:         %08x\n",
			 RADEON_READ(R500_DxMODE_INT_MASK));
	}
	DRM_PROC_PRINT("Interrupts received: %d\n",
		       atomic_read(&dev_priv->irq_received));
	DRM_PROC_PRINT("Current sequence:    %d\n",
		       READ_BREADCRUMB(dev_priv));
	DRM_PROC_PRINT("Counter sequence:     %d\n",
		       dev_priv->counter);

	
	if (len > request + offset)
		return request;
	*eof = 1;
	return len - offset;
}

static struct drm_proc_list {
	/** file name */
	const char *name;
	/** proc callback*/
	int (*f) (char *, char **, off_t, int, int *, void *);
} radeon_gem_proc_list[] = {
	{"radeon_gem_interrupt", radeon_interrupt_info},
	{"radeon_gem_ring", radeon_ring_info},
};


#define RADEON_GEM_PROC_ENTRIES ARRAY_SIZE(radeon_gem_proc_list)

int radeon_gem_proc_init(struct drm_minor *minor)
{
	struct proc_dir_entry *ent;
	int i, j;

	for (i = 0; i < RADEON_GEM_PROC_ENTRIES; i++) {
		ent = create_proc_entry(radeon_gem_proc_list[i].name,
					S_IFREG | S_IRUGO, minor->dev_root);
		if (!ent) {
			DRM_ERROR("Cannot create /proc/dri/.../%s\n",
				  radeon_gem_proc_list[i].name);
			for (j = 0; j < i; j++)
				remove_proc_entry(radeon_gem_proc_list[i].name,
						  minor->dev_root);
			return -1;
		}
		ent->read_proc = radeon_gem_proc_list[i].f;
		ent->data = minor;
	}
	return 0;
}

void radeon_gem_proc_cleanup(struct drm_minor *minor)
{
	int i;

	if (!minor->dev_root)
		return;

	for (i = 0; i < RADEON_GEM_PROC_ENTRIES; i++)
		remove_proc_entry(radeon_gem_proc_list[i].name, minor->dev_root);
}
