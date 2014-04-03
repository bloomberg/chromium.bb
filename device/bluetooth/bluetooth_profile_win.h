// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_PROFILE_WIN_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_PROFILE_WIN_H_

#include <string>

#include "device/bluetooth/bluetooth_profile.h"

namespace device {

class BluetoothDeviceWin;

class BluetoothProfileWin : public BluetoothProfile {
 public:
  // BluetoothProfile override.
  virtual void Unregister() OVERRIDE;
  virtual void SetConnectionCallback(
      const ConnectionCallback& callback) OVERRIDE;

  bool Connect(const BluetoothDeviceWin* device);

 private:
  friend BluetoothProfile;

  BluetoothProfileWin(const std::string& uuid, const std::string& name);
  virtual ~BluetoothProfileWin();

  const std::string uuid_;
  const std::string name_;
  ConnectionCallback connection_callback_;
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_PROFILE_WIN_H_
