// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_SOCKET_CHROMEOS_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_SOCKET_CHROMEOS_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "chromeos/chromeos_export.h"
#include "device/bluetooth/bluetooth_socket.h"

namespace dbus {

class FileDescriptor;

}  // namespace dbus

namespace net {

class DrainableIOBuffer;
class GrowableIOBuffer;

}  // namespace net

namespace chromeos {

// The BluetoothSocketChromeOS class implements BluetoothSocket for the
// Chrome OS platform.
class CHROMEOS_EXPORT BluetoothSocketChromeOS
    : public device::BluetoothSocket {
 public:
  // BluetoothSocket override.
  virtual bool Receive(net::GrowableIOBuffer* buffer) OVERRIDE;
  virtual bool Send(net::DrainableIOBuffer* buffer) OVERRIDE;
  virtual std::string GetLastErrorMessage() const OVERRIDE;

  // Create an instance of a BluetoothSocket from the passed file descriptor
  // received over D-Bus in |fd|, the descriptor will be taken from that object
  // and ownership passed to the returned object.
  static scoped_refptr<device::BluetoothSocket> Create(
      dbus::FileDescriptor* fd);

 protected:
  virtual ~BluetoothSocketChromeOS();

 private:
  BluetoothSocketChromeOS(int fd);

  // The different socket types have different reading patterns; l2cap sockets
  // have to be read with boundaries between datagrams preserved while rfcomm
  // sockets do not.
  enum SocketType {
    L2CAP,
    RFCOMM
  };

  // File descriptor and socket type of the socket.
  const int fd_;
  SocketType socket_type_;

  // Last error message, set during Receive() and Send() and retrieved using
  // GetLastErrorMessage().
  std::string error_message_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothSocketChromeOS);
};

}  // namespace chromeos

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_SOCKET_CHROMEOS_H_
