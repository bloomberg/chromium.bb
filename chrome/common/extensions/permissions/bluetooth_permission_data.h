// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_PERMISSIONS_BLUETOOTH_PERMISSION_DATA_H_
#define CHROME_COMMON_EXTENSIONS_PERMISSIONS_BLUETOOTH_PERMISSION_DATA_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "extensions/common/permissions/api_permission.h"

namespace base {

class Value;

}  // namespace base

namespace extensions {

// A pattern that can be used to match a Bluetooth profile permission, must be
// of a format that can be passed to device::bluetooth_utils::CanonoicalUuid().
class BluetoothPermissionData {
 public:
  BluetoothPermissionData();
  explicit BluetoothPermissionData(const std::string& uuid);

  // Check if |param| (which must be a BluetoothPermission::CheckParam)
  // matches the uuid of this object.
  bool Check(const APIPermission::CheckParam* param) const;

  // Convert |this| into a base::Value.
  scoped_ptr<base::Value> ToValue() const;

  // Populate |this| from a base::Value.
  bool FromValue(const base::Value* value);

  bool operator<(const BluetoothPermissionData& rhs) const;
  bool operator==(const BluetoothPermissionData& rhs) const;

  // The uuid |this| matches against.
  const std::string& uuid() const { return uuid_; }

  // This accessor is provided for IPC_STRUCT_TRAITS_MEMBER.  Please
  // think twice before using it for anything else.
  std::string& uuid() { return uuid_; }

 private:
  std::string uuid_;
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_PERMISSIONS_BLUETOOTH_PERMISSION_DATA_H_
