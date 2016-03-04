// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/usb/mojo/fake_permission_provider.h"

#include <stddef.h>
#include <utility>

namespace device {
namespace usb {

FakePermissionProvider::FakePermissionProvider() : weak_factory_(this) {}

FakePermissionProvider::~FakePermissionProvider() {}

base::WeakPtr<PermissionProvider> FakePermissionProvider::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

bool FakePermissionProvider::HasDevicePermission(
    const device::usb::DeviceInfo& device_info) const {
  return true;
}

bool FakePermissionProvider::HasConfigurationPermission(
    uint8_t requested_configuration,
    const device::usb::DeviceInfo& device_info) const {
  return true;
}

bool FakePermissionProvider::HasInterfacePermission(
    uint8_t requested_interface,
    uint8_t configuration_value,
    const device::usb::DeviceInfo& device_info) const {
  return true;
}

}  // namespace usb
}  // namespace device
