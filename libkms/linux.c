/**************************************************************************
 *
 * Copyright © 2009 VMware, Inc., Palo Alto, CA., USA
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS, AUTHORS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 **************************************************************************/
/*
 * Thanks to krh and jcristau for the tips on
 * going from fd to pci id via fstat and udev.
 */


#include <errno.h>
#include <stdio.h>
#include <xf86drm.h>
#include <sys/stat.h>

#include "internal.h"

#define LIBUDEV_I_KNOW_THE_API_IS_SUBJECT_TO_CHANGE
#include <libudev.h>

int linux_get_pciid_from_fd(int fd, unsigned *vendor_id, unsigned *chip_id)
{
	struct udev *udev;
	struct udev_device *device;
	struct udev_device *parent;
	const char *pci_id;
	struct stat buffer;
	int ret;

	ret = fstat(fd, &buffer);
	if (ret)
		return -EINVAL;

	if (!S_ISCHR(buffer.st_mode))
		return -EINVAL;

	udev = udev_new();
	if (!udev)
		return -ENOMEM;

	device = udev_device_new_from_devnum(udev, 'c', buffer.st_rdev);
	if (!device)
		goto err_free_udev;

	parent = udev_device_get_parent(device);
	if (!parent)
		goto err_free_device;

	pci_id = udev_device_get_property_value(parent, "PCI_ID");
	if (!pci_id)
		goto err_free_device;

	if (sscanf(pci_id, "%x:%x", vendor_id, chip_id) != 2)
		goto err_free_device;

	udev_device_unref(device);
	udev_unref(udev);

	return 0;

err_free_device:
	udev_device_unref(device);
err_free_udev:
	udev_unref(udev);
	return -EINVAL;
}
