// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_PROFILE_MAC_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_PROFILE_MAC_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "device/bluetooth/bluetooth_profile.h"
#include "device/bluetooth/bluetooth_uuid.h"

#ifdef __OBJC__
@class IOBluetoothDevice;
#else
class IOBluetoothDevice;
#endif

namespace device {

class BluetoothProfileMac : public BluetoothProfile {
 public:
  // BluetoothProfile override.
  virtual void Unregister() OVERRIDE;
  virtual void SetConnectionCallback(
      const ConnectionCallback& callback) OVERRIDE;

  // Makes an outgoing connection to |device|.
  // This method runs |socket_callback_| with the socket and returns true if the
  // connection is made successfully.
  bool Connect(IOBluetoothDevice* device);

 private:
  friend BluetoothProfile;

  BluetoothProfileMac(const BluetoothUUID& uuid, const std::string& name);
  virtual ~BluetoothProfileMac();

  const BluetoothUUID uuid_;
  const std::string name_;
  ConnectionCallback connection_callback_;
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_PROFILE_MAC_H_
