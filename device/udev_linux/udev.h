// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_UDEV_LINUX_UDEV_H_
#define DEVICE_UDEV_LINUX_UDEV_H_

#include <libudev.h>

#include "base/memory/scoped_ptr.h"

#if !defined(USE_UDEV)
#error "USE_UDEV not defined"
#endif

namespace device {

struct UdevDeleter {
  void operator()(udev* dev) const;
};
struct UdevEnumerateDeleter {
  void operator()(udev_enumerate* enumerate) const;
};
struct UdevDeviceDeleter {
  void operator()(udev_device* device) const;
};
struct UdevMonitorDeleter {
  void operator()(udev_monitor* monitor) const;
};

typedef scoped_ptr<udev, UdevDeleter> ScopedUdevPtr;
typedef scoped_ptr<udev_enumerate, UdevEnumerateDeleter> ScopedUdevEnumeratePtr;
typedef scoped_ptr<udev_device, UdevDeviceDeleter> ScopedUdevDevicePtr;
typedef scoped_ptr<udev_monitor, UdevMonitorDeleter> ScopedUdevMonitorPtr;

}  // namespace device

#endif  // DEVICE_UDEV_LINUX_UDEV_H_
