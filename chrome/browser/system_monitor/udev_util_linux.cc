// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/system_monitor/udev_util_linux.h"

namespace chrome {

std::string GetUdevDevicePropertyValue(struct udev_device* udev_device,
                                       const char* key) {
  const char* value = udev_device_get_property_value(udev_device, key);
  return value ? value : std::string();
}

}  // namespace chrome
