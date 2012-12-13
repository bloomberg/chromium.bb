// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_PERMISSIONS_BLUETOOTH_DEVICE_PERMISSION_H_
#define CHROME_COMMON_EXTENSIONS_PERMISSIONS_BLUETOOTH_DEVICE_PERMISSION_H_

#include <string>

#include "chrome/common/extensions/permissions/api_permission.h"
#include "chrome/common/extensions/permissions/bluetooth_device_permission_data.h"
#include "chrome/common/extensions/permissions/set_disjunction_permission.h"

namespace extensions {

// BluetoothDevicePermission represents the permission to access a specific
// Bluetooth Device.
class BluetoothDevicePermission
  : public SetDisjunctionPermission<BluetoothDevicePermissionData,
                                    BluetoothDevicePermission> {
 public:
  // A Bluetooth device address that should be check for permission to access.
  struct CheckParam : APIPermission::CheckParam {
    explicit CheckParam(std::string device_address)
      : device_address(device_address) {}
    const std::string device_address;
  };

  explicit BluetoothDevicePermission(const APIPermissionInfo* info);
  virtual ~BluetoothDevicePermission();

  // APIPermission overrides
  virtual bool ManifestEntryForbidden() const OVERRIDE;
  virtual PermissionMessages GetMessages() const OVERRIDE;
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_PERMISSIONS_BLUETOOTH_DEVICE_PERMISSION_H_
