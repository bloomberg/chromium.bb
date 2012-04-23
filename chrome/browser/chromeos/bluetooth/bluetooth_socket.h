// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_BLUETOOTH_BLUETOOTH_SOCKET_H_
#define CHROME_BROWSER_CHROMEOS_BLUETOOTH_BLUETOOTH_SOCKET_H_
#pragma once

#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/bluetooth/bluetooth_service_record.h"

namespace chromeos {

class BluetoothDevice;

// The BluetoothSocket class represents a socket to a specific service on
// a BluetoothDevice.  BluetoothSocket objects are ref counted and may outlive
// both the BluetoothDevice and BluetoothAdapter that were involved in their
// creation.
class BluetoothSocket : public base::RefCounted<BluetoothSocket> {
 public:
  static scoped_refptr<BluetoothSocket> CreateBluetoothSocket(
      const BluetoothServiceRecord& service_record);

  int fd() const { return fd_; }

 private:
  friend class base::RefCounted<BluetoothSocket>;

  BluetoothSocket(const std::string& address, int fd);
  virtual ~BluetoothSocket();

  const std::string address_;
  const int fd_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothSocket);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_BLUETOOTH_BLUETOOTH_SOCKET_H_
