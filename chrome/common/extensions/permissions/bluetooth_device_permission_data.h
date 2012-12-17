// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_PERMISSIONS_BLUETOOTH_DEVICE_PERMISSION_DATA_H_
#define CHROME_COMMON_EXTENSIONS_PERMISSIONS_BLUETOOTH_DEVICE_PERMISSION_DATA_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "chrome/common/extensions/permissions/api_permission.h"

namespace base {

class Value;

}  // namespace base

namespace extensions {

// A pattern that can be used to match a Bluetooth device permission.
// Must be of the format: "XX:XX:XX:XX:XX:XX", where XX are hexadecimal digits.
class BluetoothDevicePermissionData {
 public:
  BluetoothDevicePermissionData();
  explicit BluetoothDevicePermissionData(const std::string& device_address);

  // Check if |param| (which must be a BluetoothDevicePermission::CheckParam)
  // matches the address of this object..
  bool Check(const APIPermission::CheckParam* param) const;

  // Convert |this| into a base::Value.
  scoped_ptr<base::Value> ToValue() const;

  // Populate |this| from a base::Value.
  bool FromValue(const base::Value* value);

  bool operator<(const BluetoothDevicePermissionData& rhs) const;
  bool operator==(const BluetoothDevicePermissionData& rhs) const;

  // The address |this| matches against.
  const std::string& device_address() const { return device_address_; }

  // This accessor is provided for IPC_STRUCT_TRAITS_MEMBER.  Please
  // think twice before using it for anything else.
  std::string& device_address() { return device_address_; }

 private:
  std::string device_address_;
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_PERMISSIONS_BLUETOOTH_DEVICE_PERMISSION_DATA_H_
