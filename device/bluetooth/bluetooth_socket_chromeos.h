// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_SOCKET_CHROMEOS_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_SOCKET_CHROMEOS_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "device/bluetooth/bluetooth_socket.h"

namespace device {

class BluetoothServiceRecord;

}  // namespace device

namespace chromeos {

// This class is an implementation of BluetoothSocket class for Chrome OS
// platform.
class BluetoothSocketChromeOs : public device::BluetoothSocket {
 public:
  static scoped_refptr<device::BluetoothSocket> CreateBluetoothSocket(
      const device::BluetoothServiceRecord& service_record);

  // BluetoothSocket override
  virtual int fd() const OVERRIDE;

 protected:
  virtual ~BluetoothSocketChromeOs();

 private:
  BluetoothSocketChromeOs(const std::string& address, int fd);

  const std::string address_;
  const int fd_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothSocketChromeOs);
};

}  // namespace chromeos

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_SOCKET_CHROMEOS_H_
