// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_USB_FAKE_PERMISSION_PROVIDER_H_
#define DEVICE_USB_FAKE_PERMISSION_PROVIDER_H_

#include <stdint.h>

#include "device/usb/public/interfaces/permission_provider.mojom.h"
#include "mojo/public/cpp/bindings/array.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/weak_binding_set.h"

namespace device {
namespace usb {

class FakePermissionProvider : public PermissionProvider {
 public:
  FakePermissionProvider();
  ~FakePermissionProvider() override;

  void HasDevicePermission(
      mojo::Array<DeviceInfoPtr> requested_devices,
      const HasDevicePermissionCallback& callback) override;
  void HasConfigurationPermission(
      uint8_t requested_configuration,
      device::usb::DeviceInfoPtr device,
      const HasInterfacePermissionCallback& callback) override;
  void HasInterfacePermission(
      uint8_t requested_interface,
      uint8_t configuration_value,
      device::usb::DeviceInfoPtr device,
      const HasInterfacePermissionCallback& callback) override;
  void Bind(mojo::InterfaceRequest<PermissionProvider> request) override;

 private:
  mojo::WeakBindingSet<PermissionProvider> bindings_;
};

}  // namespace usb
}  // namespace device

#endif  // DEVICE_USB_FAKE_PERMISSION_PROVIDER_H_
