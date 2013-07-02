// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_PERMISSIONS_BLUETOOTH_PERMISSION_H_
#define CHROME_COMMON_EXTENSIONS_PERMISSIONS_BLUETOOTH_PERMISSION_H_

#include <string>

#include "chrome/common/extensions/permissions/api_permission.h"
#include "chrome/common/extensions/permissions/bluetooth_permission_data.h"
#include "chrome/common/extensions/permissions/set_disjunction_permission.h"

namespace extensions {

// BluetoothPermission represents the permission to implement a specific
// Bluetooth Profile.
class BluetoothPermission
  : public SetDisjunctionPermission<BluetoothPermissionData,
                                    BluetoothPermission> {
 public:
  // A Bluetooth profile uuid that should be checked for permission to access.
  struct CheckParam : APIPermission::CheckParam {
    explicit CheckParam(std::string uuid)
      : uuid(uuid) {}
    const std::string uuid;
  };

  explicit BluetoothPermission(const APIPermissionInfo* info);
  virtual ~BluetoothPermission();

  // SetDisjunctionPermission overrides.
  // BluetoothPermission permits an empty list for gaining permission to the
  // Bluetooth APIs without implementing a profile.
  virtual bool FromValue(const base::Value* value) OVERRIDE;

  // APIPermission overrides
  virtual PermissionMessages GetMessages() const OVERRIDE;
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_PERMISSIONS_BLUETOOTH_PERMISSION_H_
