// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_HID_UDEV_COMMON_H_
#define DEVICE_HID_UDEV_COMMON_H_

#include <libudev.h>

#include "base/memory/scoped_ptr.h"

namespace device {

template <typename T, void func(T*)>
void DiscardReturnType(T* arg) {
  func(arg);
}

template <typename T, T* func(T*)>
void DiscardReturnType(T* arg) {
  func(arg);
}

template <typename T, void func(T*)>
struct Deleter {
  void operator()(T* enumerate) const {
    if (enumerate != NULL)
      func(enumerate);
  }
};

typedef Deleter<udev, DiscardReturnType<udev, udev_unref> > UdevDeleter;
typedef Deleter<udev_enumerate,
                DiscardReturnType<udev_enumerate, udev_enumerate_unref> >
    UdevEnumerateDeleter;
typedef Deleter<udev_device, DiscardReturnType<udev_device, udev_device_unref> >
    UdevDeviceDeleter;
typedef Deleter<udev_monitor,
                DiscardReturnType<udev_monitor, udev_monitor_unref> >
    UdevMonitorDeleter;

typedef scoped_ptr<udev, UdevDeleter> ScopedUdevPtr;
typedef scoped_ptr<udev_enumerate, UdevEnumerateDeleter> ScopedUdevEnumeratePtr;
typedef scoped_ptr<udev_device, UdevDeviceDeleter> ScopedUdevDevicePtr;
typedef scoped_ptr<udev_monitor, UdevMonitorDeleter> ScopedUdevMonitorPtr;

}  // namespace device

#endif  // DEVICE_HID_UDEV_COMMON_H_