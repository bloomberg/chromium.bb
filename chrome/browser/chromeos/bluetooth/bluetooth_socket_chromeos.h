// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_BLUETOOTH_BLUETOOTH_SOCKET_CHROMEOS_H_
#define CHROME_BROWSER_CHROMEOS_BLUETOOTH_BLUETOOTH_SOCKET_CHROMEOS_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "chrome/browser/chromeos/bluetooth/bluetooth_service_record.h"
#include "chrome/browser/chromeos/bluetooth/bluetooth_socket.h"

namespace chromeos {

// This class is an implementation of BluetoothSocket class for Chrome OS
// platform.
class BluetoothSocketChromeOs : public BluetoothSocket {
 public:
  static scoped_refptr<BluetoothSocket> CreateBluetoothSocket(
      const BluetoothServiceRecord& service_record);

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

#endif  // CHROME_BROWSER_CHROMEOS_BLUETOOTH_BLUETOOTH_SOCKET_CHROMEOS_H_
