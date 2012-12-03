// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_PERMISSIONS_BLUETOOTH_DEVICE_PERMISSION_H_
#define CHROME_COMMON_EXTENSIONS_PERMISSIONS_BLUETOOTH_DEVICE_PERMISSION_H_

#include <set>
#include <string>

#include "chrome/common/extensions/permissions/api_permission.h"

namespace base {
class Value;
}

namespace IPC {
class Message;
}

namespace extensions {

// There's room to share code with related classes, see http://crbug.com/147531
class BluetoothDevicePermission : public APIPermission {
 public:
  struct CheckParam : public APIPermission::CheckParam {
    explicit CheckParam(const std::string& device_address)
      : device_address(device_address) {}
    const std::string device_address;
  };

  explicit BluetoothDevicePermission(const APIPermissionInfo* info);

  virtual ~BluetoothDevicePermission();

  // Adds BluetoothDevices from |devices| to the set of allowed devices.
  // |devices| should be a string of Bluetooth device addresses separated by |.
  void AddDevicesFromString(const std::string &devices_string);

  // APIPermission overrides
  virtual std::string ToString() const OVERRIDE;
  virtual bool ManifestEntryForbidden() const OVERRIDE;
  virtual bool HasMessages() const OVERRIDE;
  virtual PermissionMessages GetMessages() const OVERRIDE;
  virtual bool Check(
      const APIPermission::CheckParam* param) const OVERRIDE;
  virtual bool Contains(const APIPermission* rhs) const OVERRIDE;
  virtual bool Equal(const APIPermission* rhs) const OVERRIDE;
  virtual bool FromValue(const base::Value* value) OVERRIDE;
  virtual void ToValue(base::Value** value) const OVERRIDE;
  virtual APIPermission* Clone() const OVERRIDE;
  virtual APIPermission* Diff(const APIPermission* rhs) const OVERRIDE;
  virtual APIPermission* Union(const APIPermission* rhs) const OVERRIDE;
  virtual APIPermission* Intersect(const APIPermission* rhs) const OVERRIDE;
  virtual void Write(IPC::Message* m) const OVERRIDE;
  virtual bool Read(const IPC::Message* m, PickleIterator* iter) OVERRIDE;
  virtual void Log(std::string* log) const OVERRIDE;

 private:
  std::set<std::string> devices_;
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_PERMISSIONS_BLUETOOTH_DEVICE_PERMISSION_H_
