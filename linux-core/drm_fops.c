/**
 * \file drm_fops.c
 * File operations for DRM
 *
 * \author Rickard E. (Rik) Faith <faith@valinux.com>
 * \author Daryll Strauss <daryll@valinux.com>
 * \author Gareth Hughes <gareth@valinux.com>
 */

/*
 * Created: Mon Jan  4 08:58:31 1999 by faith@valinux.com
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
 */

#include "drmP.h"
#include "drm_sarea.h"
#include <linux/poll.h>

static int drm_open_helper(struct inode *inode, struct file *filp,
			   struct drm_device * dev);

static int drm_setup(struct drm_device * dev)
{
	int i;
	int ret;

	if (dev->driver->firstopen) {
		ret = dev->driver->firstopen(dev);
		if (ret != 0)
			return ret;
	}

	atomic_set(&dev->ioctl_count, 0);
	atomic_set(&dev->vma_count, 0);
	dev->buf_use = 0;
	atomic_set(&dev->buf_alloc, 0);

	if (drm_core_check_feature(dev, DRIVER_HAVE_DMA)) {
		i = drm_dma_setup(dev);
		if (i < 0)
			return i;
	}

	for (i = 0; i < ARRAY_SIZE(dev->counts); i++)
		atomic_set(&dev->counts[i], 0);

	dev->sigdata.lock = NULL;

	dev->queue_count = 0;
	dev->queue_reserved = 0;
	dev->queue_slots = 0;
	dev->queuelist = NULL;
	dev->context_flag = 0;
	dev->interrupt_flag = 0;
	dev->dma_flag = 0;
	dev->last_context = 0;
	dev->last_switch = 0;
	dev->last_checked = 0;
	init_waitqueue_head(&dev->context_wait);
	dev->if_version = 0;

	dev->ctx_start = 0;
	dev->lck_start = 0;

	dev->buf_async = NULL;
	init_waitqueue_head(&dev->buf_readers);
	init_waitqueue_head(&dev->buf_writers);

	DRM_DEBUG("\n");

	/*
	 * The kernel's context could be created here, but is now created
	 * in drm_dma_enqueue.  This is more resource-efficient for
	 * hardware that does not do DMA, but may mean that
	 * drm_select_queue fails between the time the interrupt is
	 * initialized and the time the queues are initialized.
	 */

	return 0;
}

/**
 * Open file.
 *
 * \param inode device inode
 * \param filp file pointer.
 * \return zero on success or a negative number on failure.
 *
 * Searches the DRM device with the same minor number, calls open_helper(), and
 * increments the device open count. If the open count was previous at zero,
 * i.e., it's the first that the device is open, then calls setup().
 */
int drm_open(struct inode *inode, struct file *filp)
{
	struct drm_device *dev = NULL;
	int minor_id = iminor(inode);
	struct drm_minor *minor;
	int retcode = 0;

	minor = idr_find(&drm_minors_idr, minor_id);
	if (!minor)
		return -ENODEV;

	if (!(dev = minor->dev))
		return -ENODEV;

	retcode = drm_open_helper(inode, filp, dev);
	if (!retcode) {
		atomic_inc(&dev->counts[_DRM_STAT_OPENS]);
		spin_lock(&dev->count_lock);
		if (!dev->open_count++) {
			spin_unlock(&dev->count_lock);
			retcode = drm_setup(dev);
			goto out;
		}
		spin_unlock(&dev->count_lock);
	}

out:
	mutex_lock(&dev->struct_mutex);
	BUG_ON((dev->dev_mapping != NULL) &&
	       (dev->dev_mapping != inode->i_mapping));
	if (dev->dev_mapping == NULL)
		dev->dev_mapping = inode->i_mapping;
	mutex_unlock(&dev->struct_mutex);

	return retcode;
}
EXPORT_SYMBOL(drm_open);

/**
 * File \c open operation.
 *
 * \param inode device inode.
 * \param filp file pointer.
 *
 * Puts the dev->fops corresponding to the device minor number into
 * \p filp, call the \c open method, and restore the file operations.
 */
int drm_stub_open(struct inode *inode, struct file *filp)
{
	struct drm_device *dev = NULL;
	struct drm_minor *minor;
	int minor_id = iminor(inode);
	int err = -ENODEV;
	const struct file_operations *old_fops;

	DRM_DEBUG("\n");

	minor = idr_find(&drm_minors_idr, minor_id);
	if (!minor)
		return -ENODEV;
	
	if (!(dev = minor->dev))
		return -ENODEV;

	old_fops = filp->f_op;
	filp->f_op = fops_get(&dev->driver->fops);
	if (filp->f_op->open && (err = filp->f_op->open(inode, filp))) {
		fops_put(filp->f_op);
		filp->f_op = fops_get(old_fops);
	}
	fops_put(old_fops);

	return err;
}

/**
 * Check whether DRI will run on this CPU.
 *
 * \return non-zero if the DRI will run on this CPU, or zero otherwise.
 */
static int drm_cpu_valid(void)
{
#if defined(__i386__)
	if (boot_cpu_data.x86 == 3)
		return 0;	/* No cmpxchg on a 386 */
#endif
#if defined(__sparc__) && !defined(__sparc_v9__)
	return 0;		/* No cmpxchg before v9 sparc. */
#endif
	return 1;
}

/**
 * Called whenever a process opens /dev/drm.
 *
 * \param inode device inode.
 * \param filp file pointer.
 * \param dev device.
 * \return zero on success or a negative number on failure.
 *
 * Creates and initializes a drm_file structure for the file private data in \p
 * filp and add it into the double linked list in \p dev.
 */
static int drm_open_helper(struct inode *inode, struct file *filp,
			   struct drm_device * dev)
{
	int minor_id = iminor(inode);
	struct drm_file *priv;
	int ret;
	int i, j;

	if (filp->f_flags & O_EXCL)
		return -EBUSY;	/* No exclusive opens */
	if (!drm_cpu_valid())
		return -EINVAL;

	DRM_DEBUG("pid = %d, minor = %d\n", current->pid, minor_id);

	priv = drm_alloc(sizeof(*priv), DRM_MEM_FILES);
	if (!priv)
		return -ENOMEM;

	memset(priv, 0, sizeof(*priv));
	filp->private_data = priv;
	priv->filp = filp;
	priv->uid = current->euid;
	priv->pid = current->pid;
	priv->minor = idr_find(&drm_minors_idr, minor_id);
	priv->ioctl_count = 0;
	/* for compatibility root is always authenticated */
	priv->authenticated = capable(CAP_SYS_ADMIN);
	priv->lock_count = 0;

	INIT_LIST_HEAD(&priv->lhead);
	INIT_LIST_HEAD(&priv->refd_objects);
	INIT_LIST_HEAD(&priv->fbs);

	for (i = 0; i < _DRM_NO_REF_TYPES; ++i) {
		ret = drm_ht_create(&priv->refd_object_hash[i],
				    DRM_FILE_HASH_ORDER);
		if (ret)
			break;
	}

	if (ret) {
		for (j = 0; j < i; ++j)
			drm_ht_remove(&priv->refd_object_hash[j]);
		goto out_free;
	}

	if (dev->driver->open) {
		ret = dev->driver->open(dev, priv);
		if (ret < 0)
			goto out_free;
	}


	/* if there is no current master make this fd it */
	mutex_lock(&dev->struct_mutex);
	if (!priv->minor->master) {
		priv->minor->master = drm_get_master(priv->minor);
		if (!priv->minor->master) {
			ret = -ENOMEM;
			goto out_free;
		}

		priv->is_master = 1;
		priv->master = priv->minor->master;

		mutex_unlock(&dev->struct_mutex);
		if (dev->driver->master_create) {
			ret = dev->driver->master_create(dev, priv->master);
			if (ret) {
				drm_put_master(priv->minor->master);
				priv->minor->master = priv->master = NULL;
				mutex_unlock(&dev->struct_mutex);
				goto out_free;
			}
		}
	} else {
		priv->master = priv->minor->master;
		mutex_unlock(&dev->struct_mutex);
	}

	mutex_lock(&dev->struct_mutex);
	list_add(&priv->lhead, &dev->filelist);
	mutex_unlock(&dev->struct_mutex);

#ifdef __alpha__
	/*
	 * Default the hose
	 */
	if (!dev->hose) {
		struct pci_dev *pci_dev;
		pci_dev = pci_get_class(PCI_CLASS_DISPLAY_VGA << 8, NULL);
		if (pci_dev) {
			dev->hose = pci_dev->sysdata;
			pci_dev_put(pci_dev);
		}
		if (!dev->hose) {
			struct pci_bus *b = pci_bus_b(pci_root_buses.next);
			if (b)
				dev->hose = b->sysdata;
		}
	}
#endif

	return 0;
      out_free:
	drm_free(priv, sizeof(*priv), DRM_MEM_FILES);
	filp->private_data = NULL;
	return ret;
}

/** No-op. */
int drm_fasync(int fd, struct file *filp, int on)
{
	struct drm_file *priv = filp->private_data;
	struct drm_device *dev = priv->minor->dev;
	int retcode;

	DRM_DEBUG("fd = %d, device = 0x%lx\n", fd,
		  (long)old_encode_dev(priv->minor->device));
	retcode = fasync_helper(fd, filp, on, &dev->buf_async);
	if (retcode < 0)
		return retcode;
	return 0;
}
EXPORT_SYMBOL(drm_fasync);

static void drm_object_release(struct file *filp)
{
	struct drm_file *priv = filp->private_data;
	struct list_head *head;
	struct drm_ref_object *ref_object;
	int i;

	/*
	 * Free leftover ref objects created by me. Note that we cannot use
	 * list_for_each() here, as the struct_mutex may be temporarily
	 * released by the remove_() functions, and thus the lists may be
	 * altered.
	 * Also, a drm_remove_ref_object() will not remove it
	 * from the list unless its refcount is 1.
	 */

	head = &priv->refd_objects;
	while (head->next != head) {
		ref_object = list_entry(head->next, struct drm_ref_object, list);
		drm_remove_ref_object(priv, ref_object);
		head = &priv->refd_objects;
	}

	for (i = 0; i < _DRM_NO_REF_TYPES; ++i)
		drm_ht_remove(&priv->refd_object_hash[i]);
}

/**
 * Release file.
 *
 * \param inode device inode
 * \param file_priv DRM file private.
 * \return zero on success or a negative number on failure.
 *
 * If the hardware lock is held then free it, and take it again for the kernel
 * context since it's necessary to reclaim buffers. Unlink the file private
 * data from its list and free it. Decreases the open count and if it reaches
 * zero calls drm_lastclose().
 */
int drm_release(struct inode *inode, struct file *filp)
{
	struct drm_file *file_priv = filp->private_data;
	struct drm_device *dev = file_priv->minor->dev;
	int retcode = 0;

	lock_kernel();

	DRM_DEBUG("open_count = %d\n", dev->open_count);

	if (dev->driver->preclose)
		dev->driver->preclose(dev, file_priv);

	/* ========================================================
	 * Begin inline drm_release
	 */

	DRM_DEBUG("pid = %d, device = 0x%lx, open_count = %d\n",
		  current->pid, (long)old_encode_dev(file_priv->minor->device),
		  dev->open_count);

	/* if the master has gone away we can't do anything with the lock */
	if (file_priv->minor->master) {
		if (dev->driver->reclaim_buffers_locked && file_priv->master->lock.hw_lock) {
			if (drm_i_have_hw_lock(dev, file_priv)) {
				dev->driver->reclaim_buffers_locked(dev, file_priv);
			} else {
				unsigned long _end=jiffies + 3*DRM_HZ;
				int locked = 0;
				
				drm_idlelock_take(&file_priv->master->lock);
				
				/*
				 * Wait for a while.
				 */
				
				do{
					spin_lock(&file_priv->master->lock.spinlock);
					locked = file_priv->master->lock.idle_has_lock;
					spin_unlock(&file_priv->master->lock.spinlock);
					if (locked)
						break;
					schedule();
				} while (!time_after_eq(jiffies, _end));
				
				if (!locked) {
					DRM_ERROR("reclaim_buffers_locked() deadlock. Please rework this\n"
						  "\tdriver to use reclaim_buffers_idlelocked() instead.\n"
						  "\tI will go on reclaiming the buffers anyway.\n");
				}
				
				dev->driver->reclaim_buffers_locked(dev, file_priv);
				drm_idlelock_release(&file_priv->master->lock);
			}
		}

		if (dev->driver->reclaim_buffers_idlelocked && file_priv->master->lock.hw_lock) {
			
			drm_idlelock_take(&file_priv->master->lock);
			dev->driver->reclaim_buffers_idlelocked(dev, file_priv);
			drm_idlelock_release(&file_priv->master->lock);
			
		}


		if (drm_i_have_hw_lock(dev, file_priv)) {
			DRM_DEBUG("File %p released, freeing lock for context %d\n",
				  filp, _DRM_LOCKING_CONTEXT(file_priv->master->lock.hw_lock->lock));
			
			drm_lock_free(&file_priv->master->lock,
				      _DRM_LOCKING_CONTEXT(file_priv->master->lock.hw_lock->lock));
		}
		

		if (drm_core_check_feature(dev, DRIVER_HAVE_DMA) &&
		    !dev->driver->reclaim_buffers_locked) {
			dev->driver->reclaim_buffers(dev, file_priv);
		}
	}

	drm_fasync(-1, filp, 0);

	mutex_lock(&dev->ctxlist_mutex);

	if (!list_empty(&dev->ctxlist)) {
		struct drm_ctx_list *pos, *n;

		list_for_each_entry_safe(pos, n, &dev->ctxlist, head) {
			if (pos->tag == file_priv &&
			    pos->handle != DRM_KERNEL_CONTEXT) {
				if (dev->driver->context_dtor)
					dev->driver->context_dtor(dev,
								  pos->handle);

				drm_ctxbitmap_free(dev, pos->handle);

				list_del(&pos->head);
				drm_free(pos, sizeof(*pos), DRM_MEM_CTXLIST);
				--dev->ctx_count;
			}
		}
	}
	mutex_unlock(&dev->ctxlist_mutex);

	if (drm_core_check_feature(dev, DRIVER_MODESET))
		drm_fb_release(filp);

	if (file_priv->is_master) {
		struct drm_file *temp;
		list_for_each_entry(temp, &dev->filelist, lhead) {
			if ((temp->master == file_priv->master) &&
			    (temp != file_priv))
				temp->authenticated = 0;
		}

		if (file_priv->minor->master == file_priv->master)
			file_priv->minor->master = NULL;
		drm_put_master(file_priv->master);
	}

	file_priv->master = NULL;
	file_priv->is_master = 0;

	mutex_lock(&dev->struct_mutex);
	drm_object_release(filp);

	list_del(&file_priv->lhead);

	mutex_unlock(&dev->struct_mutex);

	if (dev->driver->postclose)
		dev->driver->postclose(dev, file_priv);
	drm_free(file_priv, sizeof(*file_priv), DRM_MEM_FILES);

	/* ========================================================
	 * End inline drm_release
	 */

	atomic_inc(&dev->counts[_DRM_STAT_CLOSES]);
	spin_lock(&dev->count_lock);
	if (!--dev->open_count) {
		if (atomic_read(&dev->ioctl_count)) {
			DRM_ERROR("Device busy: %d\n",
				  atomic_read(&dev->ioctl_count));
			spin_unlock(&dev->count_lock);
			unlock_kernel();
			return -EBUSY;
		}
		spin_unlock(&dev->count_lock);
		unlock_kernel();
		return drm_lastclose(dev);
	}
	spin_unlock(&dev->count_lock);

	unlock_kernel();

	return retcode;
}
EXPORT_SYMBOL(drm_release);

/** No-op. */
/* This is to deal with older X servers that believe 0 means data is
 * available which is not the correct return for a poll function.
 * This cannot be fixed until the Xserver is fixed. Xserver will need
 * to set a newer interface version to avoid breaking older Xservers.
 * Without fixing the Xserver you get: "WaitForSomething(): select: errno=22"
 * http://freedesktop.org/bugzilla/show_bug.cgi?id=1505 if you try
 * to return the correct response.
 */
unsigned int drm_poll(struct file *filp, struct poll_table_struct *wait)
{
	/* return (POLLIN | POLLOUT | POLLRDNORM | POLLWRNORM); */
	return 0;
}
EXPORT_SYMBOL(drm_poll);
