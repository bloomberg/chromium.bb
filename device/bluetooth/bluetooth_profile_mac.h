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

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace device {

class BluetoothProfileMac : public BluetoothProfile {
 public:
  // BluetoothProfile override.
  virtual void Unregister() OVERRIDE;
  virtual void SetConnectionCallback(
      const ConnectionCallback& callback) OVERRIDE;

  typedef base::Callback<void(const std::string&)> ErrorCallback;

  // Makes an outgoing connection to |device|, calling |callback| on succes or
  // |error_callback| on error. If successful, this method also calls
  // |connection_callback_| before calling |callback|.
  void Connect(const scoped_refptr<base::SequencedTaskRunner>& ui_task_runner,
               IOBluetoothDevice* device,
               const base::Closure& callback,
               const ErrorCallback& error_callback);

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
