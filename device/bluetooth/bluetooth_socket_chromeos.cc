// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_socket_chromeos.h"

#include <string>

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/thread_restrictions.h"
#include "dbus/file_descriptor.h"
#include "device/bluetooth/bluetooth_socket.h"
#include "device/bluetooth/bluetooth_socket_net.h"
#include "device/bluetooth/bluetooth_socket_thread.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"

namespace {

const char kSocketAlreadyConnected[] = "Socket is already connected.";

}  // namespace

namespace chromeos {

// static
scoped_refptr<BluetoothSocketChromeOS>
BluetoothSocketChromeOS::CreateBluetoothSocket(
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
    scoped_refptr<device::BluetoothSocketThread> socket_thread,
    net::NetLog* net_log,
    const net::NetLog::Source& source) {
  DCHECK(ui_task_runner->RunsTasksOnCurrentThread());

  return make_scoped_refptr(
      new BluetoothSocketChromeOS(
          ui_task_runner, socket_thread, net_log, source));
}

BluetoothSocketChromeOS::BluetoothSocketChromeOS(
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
    scoped_refptr<device::BluetoothSocketThread> socket_thread,
    net::NetLog* net_log,
    const net::NetLog::Source& source)
    : BluetoothSocketNet(ui_task_runner, socket_thread, net_log, source) {
}

BluetoothSocketChromeOS::~BluetoothSocketChromeOS() {
}

void BluetoothSocketChromeOS::Connect(
    scoped_ptr<dbus::FileDescriptor> fd,
    const base::Closure& success_callback,
    const ErrorCompletionCallback& error_callback) {
  DCHECK(ui_task_runner()->RunsTasksOnCurrentThread());

  socket_thread()->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(
          &BluetoothSocketChromeOS::DoConnect,
          this,
          base::Passed(&fd),
          base::Bind(&BluetoothSocketChromeOS::PostSuccess,
                     this,
                     success_callback),
          base::Bind(&BluetoothSocketChromeOS::PostErrorCompletion,
                     this,
                     error_callback)));
}

void BluetoothSocketChromeOS::DoConnect(
    scoped_ptr<dbus::FileDescriptor> fd,
    const base::Closure& success_callback,
    const ErrorCompletionCallback& error_callback) {
  DCHECK(socket_thread()->task_runner()->RunsTasksOnCurrentThread());
  base::ThreadRestrictions::AssertIOAllowed();
  DCHECK(fd->is_valid());

  if (tcp_socket()) {
    error_callback.Run(kSocketAlreadyConnected);
    return;
  }

  ResetTCPSocket();

  // Note: We don't have a meaningful |IPEndPoint|, but that is ok since the
  // TCPSocket implementation does not actually require one.
  int net_result = tcp_socket()->AdoptConnectedSocket(fd->value(),
                                                      net::IPEndPoint());
  if (net_result != net::OK) {
    error_callback.Run("Error connecting to socket: " +
                       std::string(net::ErrorToString(net_result)));
    return;
  }

  fd->TakeValue();
  success_callback.Run();
}

}  // namespace chromeos
