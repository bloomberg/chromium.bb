// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_USB_FAKE_PERMISSION_PROVIDER_H_
#define DEVICE_USB_FAKE_PERMISSION_PROVIDER_H_

#include <stdint.h>

#include "base/memory/weak_ptr.h"
#include "device/usb/mojo/permission_provider.h"

namespace device {
namespace usb {

class FakePermissionProvider : public PermissionProvider {
 public:
  FakePermissionProvider();
  ~FakePermissionProvider() override;

  base::WeakPtr<PermissionProvider> GetWeakPtr();
  bool HasDevicePermission(const DeviceInfo& device_info) const override;
  bool HasConfigurationPermission(uint8_t requested_configuration,
                                  const DeviceInfo& device_info) const override;
  bool HasInterfacePermission(uint8_t requested_interface,
                              uint8_t configuration_value,
                              const DeviceInfo& device_info) const override;

 private:
  base::WeakPtrFactory<PermissionProvider> weak_factory_;
};

}  // namespace usb
}  // namespace device

#endif  // DEVICE_USB_FAKE_PERMISSION_PROVIDER_H_
