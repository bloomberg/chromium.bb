/*
 * (C) Copyright IBM Corporation 2007
 * Copyright (C) Paul Mackerras 2005.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * on the rights to use, copy, modify, merge, publish, distribute, sub
 * license, and/or sell copies of the Software, and to permit persons to whom
 * the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS AND/OR THEIR SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *    Ian Romanick <idr@us.ibm.com>
 */

#include <linux/compat.h>

#include "drmP.h"
#include "drm.h"

#include "xgi_drm.h"

/* This is copied from drm_ioc32.c.
 */
struct drm_map32 {
	u32 offset;		/**< Requested physical address (0 for SAREA)*/
	u32 size;		/**< Requested physical size (bytes) */
	enum drm_map_type type;	/**< Type of memory to map */
	enum drm_map_flags flags;	/**< Flags */
	u32 handle;		/**< User-space: "Handle" to pass to mmap() */
	int mtrr;		/**< MTRR slot used */
};

struct drm32_xgi_bootstrap {
	struct drm_map32 gart;
};


extern int xgi_bootstrap(struct drm_device *, void *, struct drm_file *);

static int compat_xgi_bootstrap(struct file *filp, unsigned int cmd,
				unsigned long arg)
{
	struct drm32_xgi_bootstrap __user *const argp = (void __user *)arg;
	struct drm32_xgi_bootstrap bs32;
	struct xgi_bootstrap __user *bs;
	int err;
	void *handle;


	if (copy_from_user(&bs32, argp, sizeof(bs32))) {
		return -EFAULT;
	}

	bs = compat_alloc_user_space(sizeof(*bs));
	if (!access_ok(VERIFY_WRITE, bs, sizeof(*bs))) {
		return -EFAULT;
	}

	if (__put_user(bs32.gart.offset, &bs->gart.offset)
	    || __put_user(bs32.gart.size, &bs->gart.size)
	    || __put_user(bs32.gart.type, &bs->gart.type)
	    || __put_user(bs32.gart.flags, &bs->gart.flags)) {
		return -EFAULT;
	}

	err = drm_ioctl(filp->f_dentry->d_inode, filp, XGI_IOCTL_BOOTSTRAP,
			(unsigned long)bs);
	if (err) {
		return err;
	}

	if (__get_user(bs32.gart.offset, &bs->gart.offset)
	    || __get_user(bs32.gart.mtrr, &bs->gart.mtrr)
	    || __get_user(handle, &bs->gart.handle)) {
		return -EFAULT;
	}

	bs32.gart.handle = (unsigned long)handle;
	if (bs32.gart.handle != (unsigned long)handle && printk_ratelimit()) {
		printk(KERN_ERR "%s truncated handle %p for type %d "
		       "offset %x\n",
		       __func__, handle, bs32.gart.type, bs32.gart.offset);
	}

	if (copy_to_user(argp, &bs32, sizeof(bs32))) {
		return -EFAULT;
	}

	return 0;
}


drm_ioctl_compat_t *xgi_compat_ioctls[] = {
	[DRM_XGI_BOOTSTRAP] = compat_xgi_bootstrap,
};

/**
 * Called whenever a 32-bit process running under a 64-bit kernel
 * performs an ioctl on /dev/dri/card<n>.
 *
 * \param filp file pointer.
 * \param cmd command.
 * \param arg user argument.
 * \return zero on success or negative number on failure.
 */
long xgi_compat_ioctl(struct file *filp, unsigned int cmd,
		      unsigned long arg)
{
	const unsigned int nr = DRM_IOCTL_NR(cmd);
	drm_ioctl_compat_t *fn = NULL;
	int ret;

	if (nr < DRM_COMMAND_BASE)
		return drm_compat_ioctl(filp, cmd, arg);

	if (nr < DRM_COMMAND_BASE + DRM_ARRAY_SIZE(xgi_compat_ioctls))
		fn = xgi_compat_ioctls[nr - DRM_COMMAND_BASE];

	lock_kernel();
	ret = (fn != NULL)
		? (*fn)(filp, cmd, arg)
		: drm_ioctl(filp->f_dentry->d_inode, filp, cmd, arg);
	unlock_kernel();

	return ret;
}
