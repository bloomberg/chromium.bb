// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_STORAGE_MONITOR_UDEV_UTIL_LINUX_H_
#define CHROME_BROWSER_STORAGE_MONITOR_UDEV_UTIL_LINUX_H_

#include <libudev.h>

#include <string>

#include "base/memory/scoped_ptr.h"

namespace base {
class FilePath;
}

// Deleter for ScopedUdevObject.
struct UdevDeleter {
  void operator()(struct udev* udev);
};
typedef scoped_ptr<struct udev, UdevDeleter> ScopedUdevObject;

// Deleter for ScopedUdevDeviceObject().
struct UdevDeviceDeleter {
  void operator()(struct udev_device* device);
};
typedef scoped_ptr<struct udev_device, UdevDeviceDeleter>
    ScopedUdevDeviceObject;

// Wrapper function for udev_device_get_property_value() that also checks for
// valid but empty values.
std::string GetUdevDevicePropertyValue(struct udev_device* udev_device,
                                       const char* key);

// Helper for udev_device_new_from_syspath()/udev_device_get_property_value()
// pair. |device_path| is the absolute path to the device, including /sys.
bool GetUdevDevicePropertyValueByPath(const base::FilePath& device_path,
                                      const char* key,
                                      std::string* result);

#endif  // CHROME_BROWSER_STORAGE_MONITOR_UDEV_UTIL_LINUX_H_
