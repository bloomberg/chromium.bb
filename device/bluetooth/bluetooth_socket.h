// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_SOCKET_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_SOCKET_H_

#include <string>

#include "base/memory/ref_counted.h"

namespace net {

class DrainableIOBuffer;
class GrowableIOBuffer;

}  // namespace net

namespace device {

// BluetoothSocket represents a socket to a specific service on a
// BluetoothDevice.  BluetoothSocket objects are ref counted and may outlive
// both the BluetoothDevice and BluetoothAdapter that were involved in their
// creation.
class BluetoothSocket : public base::RefCounted<BluetoothSocket> {
 public:
  // Receives data from the socket and stores it in |buffer|. It returns whether
  // the reception has been successful. If it fails, the caller can get the
  // error message through |GetLastErrorMessage()|.
  virtual bool Receive(net::GrowableIOBuffer* buffer) = 0;

  // Sends |buffer| to the socket. It returns whether the sending has been
  // successful. If it fails, the caller can get the error message through
  // |GetLastErrorMessage()|.
  virtual bool Send(net::DrainableIOBuffer* buffer) = 0;

  virtual std::string GetLastErrorMessage() const = 0;

 protected:
  friend class base::RefCounted<BluetoothSocket>;
  virtual ~BluetoothSocket() {}
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_SOCKET_H_
