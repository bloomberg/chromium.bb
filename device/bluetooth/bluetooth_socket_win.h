// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_SOCKET_WIN_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_SOCKET_WIN_H_

#include "device/bluetooth/bluetooth_socket.h"

namespace device {

class BluetoothSocketWin : public BluetoothSocket {
 public:
  // BluetoothSocket override
  virtual int fd() const OVERRIDE;

 private:
  BluetoothSocketWin();
  virtual ~BluetoothSocketWin();
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_SOCKET_WIN_H_
