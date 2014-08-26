// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <libudev.h>

#include "base/files/file_util.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/extensions/api/image_writer_private/removable_storage_provider.h"
#include "content/public/browser/browser_thread.h"

namespace extensions {
// TODO(haven): Udev code may be duplicated in the Chrome codebase.
// https://code.google.com/p/chromium/issues/detail?id=284898

// Returns the integer contained in |attr|.  Returns 0 on error.
static uint64 get_int_attr(const char* attr){
  uint64 result = 0;
  // In error cases, StringToInt will set result to 0
  base::StringToUint64(attr, &result);
  return result;
}

static int get_device_blk_size(const std::string& path) {
  base::FilePath file_path(path);
  std::string device = file_path.BaseName().value();

  base::FilePath info_file_path = base::FilePath("/sys/block")
                                  .Append(device)
                                  .Append("queue/logical_block_size");

  std::string file_contents;
  int blk_size;

  if (!base::ReadFileToString(info_file_path, &file_contents)) {
    return 0;
  }
  // In error cases, StringToInt will set blk_size to 0
  base::StringToInt(file_contents, &blk_size);

  return blk_size;
}

bool RemovableStorageProvider::PopulateDeviceList(
    scoped_refptr<StorageDeviceList> device_list) {
  struct udev* udev;
  struct udev_enumerate* enumerate;
  struct udev_list_entry* devices, *dev_list_entry;
  struct udev_device* dev, *parent;

  udev = udev_new();
  if (!udev) {
    DLOG(ERROR) << "Can't create udev";
    return false;
  }

  /* Create a list of the devices in the 'block' subsystem. */
  enumerate = udev_enumerate_new(udev);

  udev_enumerate_add_match_subsystem(enumerate, "block");
  udev_enumerate_scan_devices(enumerate);
  devices = udev_enumerate_get_list_entry(enumerate);

  udev_list_entry_foreach(dev_list_entry, devices) {
    const char* path = udev_list_entry_get_name(dev_list_entry);
    dev = udev_device_new_from_syspath(udev, path);

    const char* partition = udev_device_get_sysattr_value(dev, "partition");
    if (partition && get_int_attr(partition)){
      // This is a partition of a device, not the device itself
      continue;
    }

    const char* removable = udev_device_get_sysattr_value(dev, "removable");
    if (!removable || !get_int_attr(removable)) {
      // This is not a removable storage device.
      continue;
    }

    /* Get the parent SCSI device that contains the model
       and manufacturer.  You can look at the hierarchy with
       udevadm info -a -n /dev/<device> */
    parent = udev_device_get_parent_with_subsystem_devtype(
           dev,
           "scsi",
           NULL);
    if (!parent) {
      // this is not a usb device
      continue;
    }

    linked_ptr<api::image_writer_private::RemovableStorageDevice> device(
      new api::image_writer_private::RemovableStorageDevice());
    device->vendor = udev_device_get_sysattr_value(parent, "vendor");
    device->model = udev_device_get_sysattr_value(parent, "model");
    // TODO (smaskell): Don't expose raw device path
    device->storage_unit_id = udev_device_get_devnode(dev);
    device->capacity = get_int_attr(udev_device_get_sysattr_value(dev, "size"))
      * get_device_blk_size(device->storage_unit_id);
    device->removable = removable;

    device_list->data.push_back(device);

    udev_device_unref(dev);
  }
  /* Free the enumerator object */
  udev_enumerate_unref(enumerate);
  udev_unref(udev);

  return true;
}

} // namespace extensions
