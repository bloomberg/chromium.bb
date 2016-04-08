// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_USB_MOJO_PERMISSION_PROVIDER_H_
#define DEVICE_USB_MOJO_PERMISSION_PROVIDER_H_

#include <stdint.h>

namespace device {
namespace usb {

class DeviceInfo;

// An implementation of this interface must be provided to a DeviceManager in
// order to implement device permission checks.
class PermissionProvider {
 public:
  PermissionProvider();
  virtual ~PermissionProvider();

  virtual bool HasDevicePermission(const DeviceInfo& device_info) const = 0;
  virtual bool HasConfigurationPermission(
      uint8_t requested_configuration,
      const DeviceInfo& device_info) const = 0;
  virtual bool HasFunctionPermission(uint8_t requested_function,
                                     uint8_t configuration_value,
                                     const DeviceInfo& device_info) const = 0;
  virtual void IncrementConnectionCount() = 0;
  virtual void DecrementConnectionCount() = 0;
};

}  // namespace usb
}  // namespace device

#endif  // DEVICE_USB_MOJO_PERMISSION_PROVIDER_H_
