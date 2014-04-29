// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_profile_win.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/stringprintf.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_adapter_win.h"
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

std::string IPEndPointToBluetoothAddress(const net::IPEndPoint& end_point) {
  if (end_point.address().size() != net::kBluetoothAddressSize)
    return std::string();
  // The address is copied from BTH_ADDR field of SOCKADDR_BTH, which is a
  // 64-bit ULONGLONG that stores Bluetooth address in little-endian. Print in
  // reverse order to preserve the correct ordering.
  return base::StringPrintf("%02X:%02X:%02X:%02X:%02X:%02X",
      end_point.address()[5],
      end_point.address()[4],
      end_point.address()[3],
      end_point.address()[2],
      end_point.address()[1],
      end_point.address()[0]);
}

}  // namespace

namespace device {

BluetoothProfileWin::BluetoothProfileWin()
    : BluetoothProfile(), rfcomm_channel_(0), weak_ptr_factory_(this) {
}

BluetoothProfileWin::~BluetoothProfileWin() {
}

void BluetoothProfileWin::Unregister() {
  if (profile_socket_)
    profile_socket_->Close();

  delete this;
}

void BluetoothProfileWin::SetConnectionCallback(
    const ConnectionCallback& callback) {
  connection_callback_ = callback;
}

void BluetoothProfileWin::Init(const BluetoothUUID& uuid,
                               const BluetoothProfile::Options& options,
                               const ProfileCallback& callback) {
  uuid_ = uuid;
  name_ = options.name;
  rfcomm_channel_ = options.channel;

  BluetoothAdapterFactory::GetAdapter(
    base::Bind(&BluetoothProfileWin::OnGetAdapter,
               weak_ptr_factory_.GetWeakPtr(),
               callback));
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
          ui_task_runner, socket_thread, net_log, source));

  socket->Connect(*record,
                  base::Bind(&OnConnectSuccessUI,
                             ui_task_runner,
                             success_callback,
                             connection_callback_,
                             device->GetAddress(),
                             socket),
                  error_callback);
}

void BluetoothProfileWin::OnGetAdapter(
    const ProfileCallback& callback,
    scoped_refptr<BluetoothAdapter> in_adapter) {
  DCHECK(!adapter_);
  DCHECK(!profile_socket_);

  adapter_ = in_adapter;
  profile_socket_ = BluetoothSocketWin::CreateBluetoothSocket(
      adapter()->ui_task_runner(),
      adapter()->socket_thread(),
      NULL,
      net::NetLog::Source());
  profile_socket_->StartService(
      uuid_,
      name_,
      rfcomm_channel_,
      base::Bind(&BluetoothProfileWin::OnRegisterProfileSuccess,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback),
      base::Bind(&BluetoothProfileWin::OnRegisterProfileError,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback),
      base::Bind(&BluetoothProfileWin::OnNewConnection,
                 weak_ptr_factory_.GetWeakPtr()));
}

void BluetoothProfileWin::OnRegisterProfileSuccess(
    const ProfileCallback& callback) {
  callback.Run(this);
}

void BluetoothProfileWin::OnRegisterProfileError(
    const ProfileCallback& callback,
    const std::string& error_message) {
  callback.Run(NULL);
  delete this;
}

void BluetoothProfileWin::OnNewConnection(
    scoped_refptr<BluetoothSocketWin> connected,
    const net::IPEndPoint& peer_address) {
  DCHECK(adapter()->ui_task_runner()->RunsTasksOnCurrentThread());
  if (connection_callback_.is_null())
    return;

  std::string device_address = IPEndPointToBluetoothAddress(peer_address);
  if (device_address.empty()) {
    LOG(WARNING) << "Failed to accept connection for profile "
                 << "uuid=" << uuid_.value()
                 << ", unexpected peer device address.";
    return;
  }

  BluetoothDevice* device = adapter_->GetDevice(device_address);
  if (!device) {
    LOG(WARNING) << "Failed to accept connection for profile"
                 << ",uuid=" << uuid_.value()
                 << ", unknown device=" << device_address;
    return;
  }

  connection_callback_.Run(device, connected);
}

BluetoothAdapterWin* BluetoothProfileWin::adapter() const {
  DCHECK(adapter_);
  return static_cast<BluetoothAdapterWin*>(adapter_.get());
}

}  // namespace device
