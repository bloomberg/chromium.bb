// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/storage_monitor/udev_util_linux.h"

#include "base/file_path.h"

namespace chrome {

std::string GetUdevDevicePropertyValue(struct udev_device* udev_device,
                                       const char* key) {
  const char* value = udev_device_get_property_value(udev_device, key);
  return value ? value : std::string();
}

bool GetUdevDevicePropertyValueByPath(const base::FilePath& device_path,
                                      const char* key,
                                      std::string* result) {
  ScopedUdevObject udev(udev_new());
  if (!udev.get())
    return false;
  ScopedUdevDeviceObject device(udev_device_new_from_syspath(
      udev.get(), device_path.value().c_str()));
  if (!device.get())
    return false;
  *result = GetUdevDevicePropertyValue(device.get(), key);
  return true;
}

}  // namespace chrome
