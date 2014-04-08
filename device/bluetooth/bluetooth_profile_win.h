// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_PROFILE_WIN_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_PROFILE_WIN_H_

#include <string>

#include "device/bluetooth/bluetooth_profile.h"
#include "device/bluetooth/bluetooth_uuid.h"
#include "net/base/net_log.h"

namespace device {

class BluetoothDeviceWin;
class BluetoothSocketThreadWin;

class BluetoothProfileWin : public BluetoothProfile {
 public:
  // BluetoothProfile override.
  virtual void Unregister() OVERRIDE;
  virtual void SetConnectionCallback(
      const ConnectionCallback& callback) OVERRIDE;

  typedef base::Callback<void(const std::string&)> ErrorCallback;

  void Connect(const BluetoothDeviceWin* device,
               scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
               scoped_refptr<BluetoothSocketThreadWin> socket_thread,
               net::NetLog* net_log,
               const net::NetLog::Source& source,
               const base::Closure& callback,
               const ErrorCallback& error_callback);

 private:
  friend BluetoothProfile;

  BluetoothProfileWin(const BluetoothUUID& uuid, const std::string& name);
  virtual ~BluetoothProfileWin();

  const BluetoothUUID uuid_;
  const std::string name_;
  ConnectionCallback connection_callback_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothProfileWin);
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_PROFILE_WIN_H_
