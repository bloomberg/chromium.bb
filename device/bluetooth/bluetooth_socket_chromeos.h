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

namespace net {

class DrainableIOBuffer;
class GrowableIOBuffer;

}  // namespace net

namespace chromeos {

// This class is an implementation of BluetoothSocket class for Chrome OS
// platform.
class BluetoothSocketChromeOS : public device::BluetoothSocket {
 public:
  static scoped_refptr<device::BluetoothSocket> CreateBluetoothSocket(
      const device::BluetoothServiceRecord& service_record);

  // BluetoothSocket override
  virtual bool Receive(net::GrowableIOBuffer* buffer) OVERRIDE;
  virtual bool Send(net::DrainableIOBuffer* buffer) OVERRIDE;
  virtual std::string GetLastErrorMessage() const OVERRIDE;

 protected:
  virtual ~BluetoothSocketChromeOS();

 private:
  BluetoothSocketChromeOS(int fd);

  const int fd_;
  std::string error_message_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothSocketChromeOS);
};

}  // namespace chromeos

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_SOCKET_CHROMEOS_H_
