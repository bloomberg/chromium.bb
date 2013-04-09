// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/permissions/bluetooth_device_permission_data.h"

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/common/extensions/permissions/bluetooth_device_permission.h"

namespace {

const char* kDeviceAddressKey = "deviceAddress";

}  // namespace

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

scoped_ptr<base::Value> BluetoothDevicePermissionData::ToValue() const {
  base::DictionaryValue* result = new base::DictionaryValue();
  result->SetString(kDeviceAddressKey, device_address_);
  return scoped_ptr<base::Value>(result);
}

bool BluetoothDevicePermissionData::FromValue(const base::Value* value) {
  if (!value)
    return false;

  const base::DictionaryValue* dict_value;
  if (!value->GetAsDictionary(&dict_value))
    return false;

  if (!dict_value->GetString(kDeviceAddressKey, &device_address_)) {
    device_address_.clear();
    return false;
  }

  return true;
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
