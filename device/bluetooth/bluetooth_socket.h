// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_SOCKET_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_SOCKET_H_

#include "base/memory/ref_counted.h"

namespace device {

// BluetoothSocket represents a socket to a specific service on a
// BluetoothDevice.  BluetoothSocket objects are ref counted and may outlive
// both the BluetoothDevice and BluetoothAdapter that were involved in their
// creation.
class BluetoothSocket : public base::RefCounted<BluetoothSocket> {
 public:
  // TODO(youngki): Replace this with an opaque id when read/write calls are
  // added. This interface is platform-independent and file descriptor is
  // linux-specific hence this method has to be renamed.
  virtual int fd() const = 0;

 protected:
  friend class base::RefCounted<BluetoothSocket>;
  virtual ~BluetoothSocket() {}
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_SOCKET_H_
