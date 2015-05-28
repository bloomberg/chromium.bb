// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_USB_PUBLIC_CPP_DEVICE_MANAGER_DELEGATE_H_
#define DEVICE_USB_PUBLIC_CPP_DEVICE_MANAGER_DELEGATE_H_

#include "device/usb/public/interfaces/device.mojom.h"

namespace device {
namespace usb {

// Interface used by DeviceManager instances to delegate certain decisions and
// behaviors out to their embedder.
class DeviceManagerDelegate {
 public:
  virtual ~DeviceManagerDelegate() {}

  // Determines whether a given device should be accessible to clients of the
  // DeviceManager instance.
  virtual bool IsDeviceAllowed(const DeviceInfo& info) = 0;
};

}  // namespace usb
}  // namespace device

#endif  // DEVICE_USB_PUBLIC_CPP_DEVICE_MANAGER_DELEGATE_H_
