// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/permissions/bluetooth_device_permission_data.h"

#include <string>

#include "chrome/common/extensions/permissions/bluetooth_device_permission.h"

namespace extensions {

BluetoothDevicePermissionData::BluetoothDevicePermissionData()
  : device_address_("") {
}

BluetoothDevicePermissionData::BluetoothDevicePermissionData(
    const std::string& device_address) : device_address_(device_address) {
}

bool BluetoothDevicePermissionData::Check(
    const APIPermission::CheckParam* param) const {
  if (!param)
    return false;
  const BluetoothDevicePermission::CheckParam& specific_param =
      *static_cast<const BluetoothDevicePermission::CheckParam*>(param);
  return device_address_ == specific_param.device_address;
}

bool BluetoothDevicePermissionData::Parse(const std::string& spec) {
  device_address_ = spec;
  return true;
}

const std::string &BluetoothDevicePermissionData::GetAsString() const {
  return device_address_;
}

bool BluetoothDevicePermissionData::operator<(
    const BluetoothDevicePermissionData& rhs) const {
  return device_address_ < rhs.device_address_;
}

bool BluetoothDevicePermissionData::operator==(
    const BluetoothDevicePermissionData& rhs) const {
  return device_address_ == rhs.device_address_;
}

}  // namespace extensions
