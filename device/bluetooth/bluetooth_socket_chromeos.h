// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_SOCKET_CHROMEOS_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_SOCKET_CHROMEOS_H_

#include "chromeos/chromeos_export.h"
#include "device/bluetooth/bluetooth_socket.h"
#include "device/bluetooth/bluetooth_socket_net.h"
#include "device/bluetooth/bluetooth_uuid.h"

namespace dbus {
class FileDescriptor;
}  // namespace dbus

namespace chromeos {

// The BluetoothSocketChromeOS class implements BluetoothSocket for the
// Chrome OS platform.
class CHROMEOS_EXPORT BluetoothSocketChromeOS
    : public device::BluetoothSocketNet {
 public:
  static scoped_refptr<BluetoothSocketChromeOS> CreateBluetoothSocket(
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
      scoped_refptr<device::BluetoothSocketThread> socket_thread,
      net::NetLog* net_log,
      const net::NetLog::Source& source);

  virtual void Connect(scoped_ptr<dbus::FileDescriptor> fd,
                       const base::Closure& success_callback,
                       const ErrorCompletionCallback& error_callback);

  // BluetoothSocket:
  virtual void Accept(const AcceptCompletionCallback& success_callback,
                      const ErrorCompletionCallback& error_callback) OVERRIDE;

 protected:
  virtual ~BluetoothSocketChromeOS();

 private:
  BluetoothSocketChromeOS(
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
      scoped_refptr<device::BluetoothSocketThread> socket_thread,
      net::NetLog* net_log,
      const net::NetLog::Source& source);

  void DoConnect(scoped_ptr<dbus::FileDescriptor> fd,
                 const base::Closure& success_callback,
                 const ErrorCompletionCallback& error_callback);

  DISALLOW_COPY_AND_ASSIGN(BluetoothSocketChromeOS);
};

}  // namespace chromeos

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_SOCKET_CHROMEOS_H_
