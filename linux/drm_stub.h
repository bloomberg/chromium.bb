/**
 * \file drm_stub.h
 * Stub support
 *
 * \author Rickard E. (Rik) Faith <faith@valinux.com>
 */

/*
 * Created: Fri Jan 19 10:48:35 2001 by faith@acm.org
 *
 * Copyright 2001 VA Linux Systems, Inc., Sunnyvale, California.
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
 * PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include "drmP.h"

static unsigned int cards_limit = 16;	/* Enough for one machine */
static unsigned int debug = 0;		/* 1 to enable debug output */

MODULE_AUTHOR( DRIVER_AUTHOR );
MODULE_DESCRIPTION( DRIVER_DESC );
MODULE_LICENSE("GPL and additional rights");
module_param(cards_limit, int, 0444);
MODULE_PARM_DESC(cards_limit, "Maximum number of graphics cards");
module_param(debug, int, 0644);
MODULE_PARM_DESC(debug, "Enable debug output");

drm_global_t *DRM(global);

/**
 * File \c open operation.
 *
 * \param inode device inode.
 * \param filp file pointer.
 *
 * Puts the drm_stub_list::fops corresponding to the device minor number into
 * \p filp, call the \c open method, and restore the file operations.
 */
static int stub_open(struct inode *inode, struct file *filp)
{
	drm_device_t *dev = NULL;
	int minor = iminor(inode);
	int err = -ENODEV;
	struct file_operations *old_fops;
	
	DRM_DEBUG("\n");

	if (!((minor >= 0) && (minor < DRM(global)->cards_limit)))
		return -ENODEV;

	dev = DRM(global)->minors[minor].dev;
	if (!dev)
		return -ENODEV;

	old_fops = filp->f_op;
	filp->f_op = fops_get(dev->fops);
	if (filp->f_op->open && (err = filp->f_op->open(inode, filp))) {
		fops_put(filp->f_op);
		filp->f_op = fops_get(old_fops);
	}
	fops_put(old_fops);

	return err;
}

/** File operations structure */
static struct file_operations DRM(stub_fops) = {
	.owner = THIS_MODULE,
	.open  = stub_open
};

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)
static int drm_hotplug (struct class_device *cdev, char **envp, int num_envp,
				char *buffer, int buffer_size)
{
	drm_device_t *dev;
	struct pci_dev *pdev;
	char *scratch;
	int i = 0;
	int length = 0;

	DRM_DEBUG("\n");
	if (!cdev)
		return -ENODEV;

	pdev = to_pci_dev(cdev->dev);
	if (!pdev)
		return -ENODEV;

	scratch = buffer;

	/* stuff we want to pass to /sbin/hotplug */
	envp[i++] = scratch;
	length += snprintf (scratch, buffer_size - length, "PCI_CLASS=%04X",
							pdev->class);
	if ((buffer_size - length <= 0) || (i >= num_envp))
		return -ENOMEM;
	++length;
	scratch += length;

	envp[i++] = scratch;
	length += snprintf (scratch, buffer_size - length, "PCI_ID=%04X:%04X",
							pdev->vendor, pdev->device);
	if ((buffer_size - length <= 0) || (i >= num_envp))
		return -ENOMEM;
	++length;
	scratch += length;
	
	envp[i++] = scratch;
	length += snprintf (scratch, buffer_size - length,
							"PCI_SUBSYS_ID=%04X:%04X", pdev->subsystem_vendor,
							pdev->subsystem_device);
	if ((buffer_size - length <= 0) || (i >= num_envp))
		return -ENOMEM;
	++length;
	scratch += length;

	envp[i++] = scratch;
	length += snprintf (scratch, buffer_size - length, "PCI_SLOT_NAME=%s",
							pci_name(pdev));
	if ((buffer_size - length <= 0) || (i >= num_envp))
		return -ENOMEM;
	++length;
	scratch += length;

	dev = pci_get_drvdata(pdev);
	if (dev) {
		envp[i++] = scratch;
		length += snprintf (scratch, buffer_size - length, 
							"RESET=%s", (dev->need_reset ? "true" : "false"));
		if ((buffer_size - length <= 0) || (i >= num_envp))
			return -ENOMEM;
	}
	envp[i] = 0;

	return 0;
}
#endif

/**
 * Get a device minor number.
 *
 * \param name driver name.
 * \param fops file operations.
 * \param dev DRM device.
 * \return minor number on success, or a negative number on failure.
 *
 * Allocate and initialize ::stub_list if one doesn't exist already.  Search an
 * empty entry and initialize it to the given parameters, and create the proc
 * init entry via proc_init().
 */
static int get_minor(struct pci_dev *pdev, const struct pci_device_id *ent)
{
	struct class_device *dev_class;
	drm_device_t *dev;
	int ret;
	int minor;
	drm_minor_t *minors = &DRM(global)->minors[0];

	DRM_DEBUG("\n");

	for (minor = 0; minor < DRM(global)->cards_limit; minor++, minors++) {
		if (minors->class == DRM_MINOR_FREE) {

			DRM_DEBUG("assigning minor %d\n", minor);
			dev = DRM(calloc)(1, sizeof(*dev), DRM_MEM_STUB);
			if(!dev) 
				return -ENOMEM;

			*minors = (drm_minor_t){.dev = NULL, .class = DRM_MINOR_PRIMARY};
			dev->minor = minor;
			if ((ret = DRM(fill_in_dev)(dev, pdev, ent))) {
				printk (KERN_ERR "DRM: Fill_in_dev failed.\n");
				goto err_g1;
			}
			if ((ret = DRM(proc_init)(dev, minor, DRM(global)->proc_root, &minors->dev_root))) {
				printk (KERN_ERR "DRM: Failed to initialize /proc/dri.\n");
				goto err_g1;
			}
			if (!DRM(fb_loaded)) {	/* set this before device_add hotplug uses it */
				pci_set_drvdata(pdev, dev);
				pci_request_regions(pdev, DRIVER_NAME);
				pci_enable_device(pdev);
			}
			dev_class = class_simple_device_add(DRM(global)->drm_class, 
					MKDEV(DRM_MAJOR, minor), &pdev->dev, "card%d", minor);
			if (IS_ERR(dev_class)) {
				printk (KERN_ERR "DRM: Error class_simple_device_add.\n");
				ret = PTR_ERR(dev_class);
				goto err_g2;
			}

			DRM_DEBUG("new primary minor assigned %d\n", minor);
			return 0;
		}
	}
	DRM_ERROR("out of minors\n");
	return -ENOMEM;
err_g2:
	if (!DRM(fb_loaded)) {
		pci_set_drvdata(pdev, NULL);
		pci_release_regions(pdev);
		pci_disable_device(pdev);
	}
	DRM(proc_cleanup)(minor, DRM(global)->proc_root, minors->dev_root);
err_g1:
	*minors = (drm_minor_t){.dev = NULL, .class = DRM_MINOR_FREE};
	DRM(free)(dev, sizeof(*dev), DRM_MEM_STUB);
	return ret;
}

int DRM(get_secondary_minor)(drm_device_t *dev, drm_minor_t **sec_minor)
{
	drm_minor_t *minors = &DRM(global)->minors[0];
	struct class_device *dev_class;
	int ret;
	int minor;

	DRM_DEBUG("\n");

	for (minor = 0; minor < DRM(global)->cards_limit; minor++, minors++) {
		if (minors->class == DRM_MINOR_FREE) {

			*minors = (drm_minor_t){.dev = dev, .class = DRM_MINOR_SECONDARY};
			if ((ret = DRM(proc_init)(dev, minor, DRM(global)->proc_root, &minors->dev_root))) {
				printk (KERN_ERR "DRM: Failed to initialize /proc/dri.\n");
				goto err_g1;
			}

			dev_class = class_simple_device_add(DRM(global)->drm_class, 
					MKDEV(DRM_MAJOR, minor), &dev->pdev->dev, "card%d", minor);
			if (IS_ERR(dev_class)) {
				printk (KERN_ERR "DRM: Error class_simple_device_add.\n");
				ret = PTR_ERR(dev_class);
				goto err_g2;
			}
			*sec_minor = minors;

			DRM_DEBUG("new secondary minor assigned %d\n", minor);
			return 0;
		}
	}
	DRM_ERROR("out of minors\n");
	return -ENOMEM;
err_g2:
	DRM(proc_cleanup)(minor, DRM(global)->proc_root, minors->dev_root);
err_g1:
	*minors = (drm_minor_t){.dev = NULL, .class = DRM_MINOR_FREE};
	DRM(free)(dev, sizeof(*dev), DRM_MEM_STUB);
	return ret;
}

/**
 * Put a device minor number.
 *
 * \param minor minor number.
 * \return always zero.
 *
 * Cleans up the proc resources. If a minor is zero then release the foreign
 * "drm" data, otherwise unregisters the "drm" data, frees the stub list and
 * unregisters the character device. 
 */
int DRM(put_minor)(drm_device_t *dev)
{
	drm_minor_t *minors = &DRM(global)->minors[dev->minor];
	int i;
	
	DRM_DEBUG("release primary minor %d\n", dev->minor);

	DRM(proc_cleanup)(dev->minor, DRM(global)->proc_root, minors->dev_root);
	class_simple_device_remove(MKDEV(DRM_MAJOR, dev->minor));

	*minors = (drm_minor_t){.dev = NULL, .class = DRM_MINOR_FREE};
	DRM(free)(dev, sizeof(*dev), DRM_MEM_STUB);

	/* if any device pointers are non-NULL we are not the last module */
	for (i = 0; i < DRM(global)->cards_limit; i++) {
		if (DRM(global)->minors[i].class != DRM_MINOR_FREE) {
			DRM_DEBUG("inter_module_put called\n");
			inter_module_put("drm");
			return 0;
		}
	}
	DRM_DEBUG("unregistering inter_module \n");
	inter_module_unregister("drm");
	remove_proc_entry("dri", NULL);
	class_simple_destroy(DRM(global)->drm_class);
	cdev_del(&DRM(global)->drm_cdev);
	unregister_chrdev_region(MKDEV(DRM_MAJOR, 0), DRM_MAX_MINOR);

	DRM(free)(DRM(global)->minors, sizeof(*DRM(global)->minors) *
				DRM(global)->cards_limit, DRM_MEM_STUB);
	DRM(free)(DRM(global), sizeof(*DRM(global)), DRM_MEM_STUB);
	DRM(global) = NULL;

	return 0;
}

int DRM(put_secondary_minor)(drm_minor_t *sec_minor)
{
	int minor = sec_minor - &DRM(global)->minors[0];

	DRM_DEBUG("release secondary minor %d\n", minor);

	DRM(proc_cleanup)(minor, DRM(global)->proc_root, sec_minor->dev_root);
	class_simple_device_remove(MKDEV(DRM_MAJOR, minor));

	*sec_minor = (drm_minor_t){.dev = NULL, .class = DRM_MINOR_FREE};

	return 0;
}

/**
 * Register.
 *
 * \param name driver name.
 * \param fops file operations
 * \param dev DRM device.
 * \return zero on success or a negative number on failure.
 *
 * Attempt to gets inter module "drm" information. If we are first
 * then register the character device and inter module information.
 * Try and register, if we fail to register, backout previous work.
 *
 * Finally calls stub_info::info_register.
 */
int DRM(probe)(struct pci_dev *pdev, const struct pci_device_id *ent)
{
	dev_t dev = MKDEV(DRM_MAJOR, 0);
	drm_global_t *global;
	int ret = -ENOMEM;

	DRM_DEBUG("\n");

	/* use the inter_module_get to check - as if the same module
		registers chrdev twice it succeeds */
	global = (drm_global_t *)inter_module_get("drm");
	if (global) {
		DRM(global) = global;
		global = NULL;
	} else {
		DRM_DEBUG("first probe\n");

		global = DRM(calloc)(1, sizeof(*global), DRM_MEM_STUB);
		if(!global) 
			return -ENOMEM;

		global->cards_limit = (cards_limit < DRM_MAX_MINOR + 1 ? cards_limit : DRM_MAX_MINOR + 1);
		global->minors = DRM(calloc)(global->cards_limit, 
					sizeof(*global->minors), DRM_MEM_STUB);
		if(!global->minors) 
			goto err_p1;

		if (register_chrdev_region(dev, DRM_MAX_MINOR, "drm"))
			goto err_p1;
	
		strncpy(global->drm_cdev.kobj.name, "drm", KOBJ_NAME_LEN);
		global->drm_cdev.owner = THIS_MODULE;
		cdev_init(&global->drm_cdev, &DRM(stub_fops));
		if (cdev_add(&global->drm_cdev, dev, DRM_MAX_MINOR)) {
			kobject_put(&global->drm_cdev.kobj);
			printk (KERN_ERR "DRM: Error registering drm major number.\n");
			goto err_p2;
		}
			
		global->drm_class = class_simple_create(THIS_MODULE, "drm");
		if (IS_ERR(global->drm_class)) {
			printk (KERN_ERR "DRM: Error creating drm class.\n");
			ret = PTR_ERR(global->drm_class);
			goto err_p3;
		}
		class_simple_set_hotplug(global->drm_class, drm_hotplug);

		global->proc_root = create_proc_entry("dri", S_IFDIR, NULL);
		if (!global->proc_root) {
			DRM_ERROR("Cannot create /proc/dri\n");
			ret = -1;
			goto err_p4;
		}
		DRM_DEBUG("calling inter_module_register\n");
		inter_module_register("drm", THIS_MODULE, global);
		
		DRM(global) = global;
	}
	if ((ret = get_minor(pdev, ent))) {
		if (global)
			goto err_p4;
		return ret;
	}
	return 0;
err_p4:
	class_simple_destroy(global->drm_class);
err_p3:
	cdev_del(&global->drm_cdev);
	unregister_chrdev_region(dev, DRM_MAX_MINOR);
err_p2:
	DRM(free)(global->minors, sizeof(*global->minors) * global->cards_limit, DRM_MEM_STUB);
err_p1:	
	DRM(free)(global, sizeof(*global), DRM_MEM_STUB);
	DRM(global) = NULL;
	return ret;
}
