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


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <xf86drm.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#ifdef HAVE_SYS_MKDEV_H
#include <sys/mkdev.h>
#endif

#include "libdrm_macros.h"
#include "internal.h"

#define PATH_SIZE 512

static int
linux_name_from_sysfs(int fd, char **out)
{
	char path[PATH_SIZE+1] = ""; /* initialize to please valgrind */
	char link[PATH_SIZE+1] = "";
	struct stat buffer;
	unsigned maj, min;
	char* slash_name;
	int ret;

	/* 
	 * Inside the sysfs directory for the device there is a symlink
	 * to the directory representing the driver module, that path
	 * happens to hold the name of the driver.
	 *
	 * So lets get the symlink for the drm device. Then read the link
	 * and filter out the last directory which happens to be the name
	 * of the driver, which we can use to load the correct interface.
	 *
	 * Thanks to Ray Strode of Plymouth for the code.
	 */

	ret = fstat(fd, &buffer);
	if (ret)
		return -EINVAL;

	if (!S_ISCHR(buffer.st_mode))
		return -EINVAL;

	maj = major(buffer.st_rdev);
	min = minor(buffer.st_rdev);

	snprintf(path, PATH_SIZE, "/sys/dev/char/%d:%d/device/driver", maj, min);

	if (readlink(path, link, PATH_SIZE) < 0)
		return -EINVAL;

	/* link looks something like this: ../../../bus/pci/drivers/intel */
	slash_name = strrchr(link, '/');
	if (!slash_name)
		return -EINVAL;

	/* copy name and at the same time remove the slash */
	*out = strdup(slash_name + 1);
	return 0;
}

static int
linux_from_sysfs(int fd, struct kms_driver **out)
{
	char *name;
	int ret;

	ret = linux_name_from_sysfs(fd, &name);
	if (ret)
		return ret;

#ifdef HAVE_INTEL
	if (!strcmp(name, "intel"))
		ret = intel_create(fd, out);
	else
#endif
#ifdef HAVE_VMWGFX
	if (!strcmp(name, "vmwgfx"))
		ret = vmwgfx_create(fd, out);
	else
#endif
#ifdef HAVE_NOUVEAU
	if (!strcmp(name, "nouveau"))
		ret = nouveau_create(fd, out);
	else
#endif
#ifdef HAVE_RADEON
	if (!strcmp(name, "radeon"))
		ret = radeon_create(fd, out);
	else
#endif
#ifdef HAVE_EXYNOS
	if (!strcmp(name, "exynos"))
		ret = exynos_create(fd, out);
	else
#endif
		ret = -ENOSYS;

	free(name);
	return ret;
}

#if 0
#define LIBUDEV_I_KNOW_THE_API_IS_SUBJECT_TO_CHANGE
#include <libudev.h>

struct create_record
{
	unsigned vendor;
	unsigned chip;
	int (*func)(int fd, struct kms_driver **out);
};

static const struct create_record table[] = {
	{ 0x8086, 0x2a42, intel_create }, /* i965 */
#ifdef HAVE_VMWGFX
	{ 0x15ad, 0x0405, vmwgfx_create }, /* VMware vGPU */
#endif
	{ 0, 0, NULL },
};

static int
linux_get_pciid_from_fd(int fd, unsigned *vendor_id, unsigned *chip_id)
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

static int
linux_from_udev(int fd, struct kms_driver **out)
{
	unsigned vendor_id, chip_id;
	int ret, i;

	ret = linux_get_pciid_from_fd(fd, &vendor_id, &chip_id);
	if (ret)
		return ret;

	for (i = 0; table[i].func; i++)
		if (table[i].vendor == vendor_id && table[i].chip == chip_id)
			return table[i].func(fd, out);

	return -ENOSYS;
}
#else
static int
linux_from_udev(int fd, struct kms_driver **out)
{
	return -ENOSYS;
}
#endif

drm_private int
linux_create(int fd, struct kms_driver **out)
{
	if (!dumb_create(fd, out))
		return 0;

	if (!linux_from_udev(fd, out))
		return 0;

	return linux_from_sysfs(fd, out);
}
