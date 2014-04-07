// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_profile_win.h"

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_device_win.h"
#include "device/bluetooth/bluetooth_service_record.h"
#include "device/bluetooth/bluetooth_socket_thread_win.h"
#include "device/bluetooth/bluetooth_socket_win.h"

namespace {

using device::BluetoothAdapter;
using device::BluetoothDevice;
using device::BluetoothProfileWin;
using device::BluetoothSocket;
using device::BluetoothSocketWin;

const char kNoConnectionCallback[] = "Connection callback not set";
const char kProfileNotFound[] = "Profile not found";

void OnConnectSuccessUIWithAdapter(
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
    const base::Closure& callback,
    const BluetoothProfileWin::ConnectionCallback& connection_callback,
    const std::string& device_address,
    scoped_refptr<BluetoothSocketWin> socket,
    scoped_refptr<BluetoothAdapter> adapter) {
  DCHECK(ui_task_runner->RunsTasksOnCurrentThread());
  const BluetoothDevice* device = adapter->GetDevice(device_address);
  if (device) {
    connection_callback.Run(device, socket);
    callback.Run();
  }
}

void OnConnectSuccessUI(
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
    const base::Closure& callback,
    const BluetoothProfileWin::ConnectionCallback& connection_callback,
    const std::string& device_address,
    scoped_refptr<BluetoothSocketWin> socket) {
  DCHECK(ui_task_runner->RunsTasksOnCurrentThread());
  device::BluetoothAdapterFactory::GetAdapter(
      base::Bind(&OnConnectSuccessUIWithAdapter,
                 ui_task_runner,
                 callback,
                 connection_callback,
                 device_address,
                 socket));
}

void OnConnectErrorUI(scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
                      const BluetoothProfileWin::ErrorCallback& error_callback,
                      const std::string& error) {
  DCHECK(ui_task_runner->RunsTasksOnCurrentThread());
  error_callback.Run(error);
}

}  // namespace

namespace device {

BluetoothProfileWin::BluetoothProfileWin(const BluetoothUUID& uuid,
                                         const std::string& name)
    : BluetoothProfile(), uuid_(uuid), name_(name) {
}

BluetoothProfileWin::~BluetoothProfileWin() {
}

void BluetoothProfileWin::Unregister() {
  delete this;
}

void BluetoothProfileWin::SetConnectionCallback(
    const ConnectionCallback& callback) {
  connection_callback_ = callback;
}

void BluetoothProfileWin::Connect(
    const BluetoothDeviceWin* device,
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
    scoped_refptr<BluetoothSocketThreadWin> socket_thread,
    net::NetLog* net_log,
    const net::NetLog::Source& source,
    const base::Closure& success_callback,
    const ErrorCallback& error_callback) {
  DCHECK(ui_task_runner->RunsTasksOnCurrentThread());
  if (connection_callback_.is_null()) {
    error_callback.Run(kNoConnectionCallback);
    return;
  }

  const BluetoothServiceRecord* record = device->GetServiceRecord(uuid_);
  if (!record) {
    error_callback.Run(kProfileNotFound);
    return;
  }

  scoped_refptr<BluetoothSocketWin> socket(
      BluetoothSocketWin::CreateBluetoothSocket(
          *record, ui_task_runner, socket_thread, net_log, source));

  socket->Connect(base::Bind(&OnConnectSuccessUI,
                             ui_task_runner,
                             success_callback,
                             connection_callback_,
                             device->GetAddress(),
                             socket),
                  error_callback);
}

}  // namespace device
