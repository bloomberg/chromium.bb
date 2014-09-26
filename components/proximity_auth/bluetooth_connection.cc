// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proximity_auth/bluetooth_connection.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/numerics/safe_conversions.h"
#include "components/proximity_auth/bluetooth_util.h"
#include "components/proximity_auth/remote_device.h"
#include "components/proximity_auth/wire_message.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_device.h"
#include "net/base/io_buffer.h"

namespace proximity_auth {
namespace {
const int kReceiveBufferSizeBytes = 1024;
}

BluetoothConnection::BluetoothConnection(const RemoteDevice& remote_device,
                                         const device::BluetoothUUID& uuid)
    : Connection(remote_device), uuid_(uuid), weak_ptr_factory_(this) {
}

BluetoothConnection::~BluetoothConnection() {
  Disconnect();
}

void BluetoothConnection::Connect() {
  if (status() != DISCONNECTED) {
    VLOG(1)
        << "[BC] Ignoring attempt to connect a non-disconnected connection.";
    return;
  }

  if (!device::BluetoothAdapterFactory::IsBluetoothAdapterAvailable()) {
    VLOG(1)
        << "[BC] Connection failed: Bluetooth is unsupported on this platform.";
    return;
  }

  SetStatus(IN_PROGRESS);
  device::BluetoothAdapterFactory::GetAdapter(
      base::Bind(&BluetoothConnection::OnAdapterInitialized,
                 weak_ptr_factory_.GetWeakPtr()));
}

void BluetoothConnection::Disconnect() {
  if (status() == DISCONNECTED) {
    VLOG(1)
        << "[BC] Ignoring attempt to disconnect a non-connected connection.";
    return;
  }

  // Set status as disconnected now, rather than after the socket closes, so
  // this connection is not reused.
  SetStatus(DISCONNECTED);
  if (socket_.get()) {
    socket_->Disconnect(base::Bind(&base::DoNothing));
    socket_ = NULL;
  }
  if (adapter_.get()) {
    adapter_->RemoveObserver(this);
    adapter_ = NULL;
  }
}

void BluetoothConnection::SendMessageImpl(scoped_ptr<WireMessage> message) {
  DCHECK_EQ(status(), CONNECTED);

  // Serialize the message.
  std::string serialized_message = message->Serialize();
  int message_length = base::checked_cast<int>(serialized_message.size());
  scoped_refptr<net::IOBuffer> buffer = new net::IOBuffer(message_length);
  memcpy(buffer->data(), serialized_message.c_str(), message_length);

  // Send it.
  pending_message_ = message.Pass();
  base::WeakPtr<BluetoothConnection> weak_this = weak_ptr_factory_.GetWeakPtr();
  socket_->Send(buffer,
                message_length,
                base::Bind(&BluetoothConnection::OnSend, weak_this),
                base::Bind(&BluetoothConnection::OnSendError, weak_this));
}

void BluetoothConnection::DeviceRemoved(device::BluetoothAdapter* adapter,
                                        device::BluetoothDevice* device) {
  DCHECK_EQ(adapter, adapter_.get());
  if (device->GetAddress() != remote_device().bluetooth_address)
    return;

  DCHECK_NE(status(), DISCONNECTED);
  VLOG(1) << "[BC] Device disconnected...";
  Disconnect();
}

void BluetoothConnection::ConnectToService(
    device::BluetoothDevice* device,
    const device::BluetoothUUID& uuid,
    const device::BluetoothDevice::ConnectToServiceCallback& callback,
    const device::BluetoothDevice::ConnectToServiceErrorCallback&
        error_callback) {
  bluetooth_util::ConnectToServiceInsecurely(
      device, uuid, callback, error_callback);
}

void BluetoothConnection::StartReceive() {
  base::WeakPtr<BluetoothConnection> weak_this = weak_ptr_factory_.GetWeakPtr();
  socket_->Receive(kReceiveBufferSizeBytes,
                   base::Bind(&BluetoothConnection::OnReceive, weak_this),
                   base::Bind(&BluetoothConnection::OnReceiveError, weak_this));
}

void BluetoothConnection::OnAdapterInitialized(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  const std::string address = remote_device().bluetooth_address;
  device::BluetoothDevice* bluetooth_device = adapter->GetDevice(address);
  if (!bluetooth_device) {
    VLOG(1) << "[BC] Device with address " << address
            << " is not known to the system Bluetooth daemon.";
    Disconnect();
    return;
  }

  adapter_ = adapter;
  adapter_->AddObserver(this);

  base::WeakPtr<BluetoothConnection> weak_this = weak_ptr_factory_.GetWeakPtr();
  ConnectToService(
      bluetooth_device,
      uuid_,
      base::Bind(&BluetoothConnection::OnConnected, weak_this),
      base::Bind(&BluetoothConnection::OnConnectionError, weak_this));
}

void BluetoothConnection::OnConnected(
    scoped_refptr<device::BluetoothSocket> socket) {
  if (status() != IN_PROGRESS) {
    // This case is reachable if the client of |this| connection called
    // |Disconnect()| while the backing Bluetooth connection was pending.
    DCHECK_EQ(status(), DISCONNECTED);
    VLOG(1) << "[BC] Ignoring successful backend Bluetooth connection to an "
            << "already disconnected logical connection.";
    return;
  }

  VLOG(1) << "[BC] Connection established with "
          << remote_device().bluetooth_address;
  socket_ = socket;
  SetStatus(CONNECTED);
  StartReceive();
}

void BluetoothConnection::OnConnectionError(const std::string& error_message) {
  VLOG(1) << "[BC] Connection failed: " << error_message;
  Disconnect();
}

void BluetoothConnection::OnSend(int bytes_sent) {
  VLOG(1) << "[BC] Successfully sent " << bytes_sent << " bytes.";
  OnDidSendMessage(*pending_message_, true);
  pending_message_.reset();
}

void BluetoothConnection::OnSendError(const std::string& error_message) {
  VLOG(1) << "[BC] Error when sending bytes: " << error_message;
  OnDidSendMessage(*pending_message_, false);
  pending_message_.reset();

  Disconnect();
}

void BluetoothConnection::OnReceive(int bytes_received,
                                    scoped_refptr<net::IOBuffer> buffer) {
  VLOG(1) << "[BC] Received " << bytes_received << " bytes.";
  OnBytesReceived(std::string(buffer->data(), bytes_received));

  // Post a task to delay the read until the socket is available, as
  // calling StartReceive at this point would error with ERR_IO_PENDING.
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&BluetoothConnection::StartReceive,
                 weak_ptr_factory_.GetWeakPtr()));
}

void BluetoothConnection::OnReceiveError(
    device::BluetoothSocket::ErrorReason error_reason,
    const std::string& error_message) {
  VLOG(1) << "[BC] Error receiving bytes: " << error_message;

  // Post a task to delay the read until the socket is available, as
  // calling StartReceive at this point would error with ERR_IO_PENDING.
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&BluetoothConnection::StartReceive,
                 weak_ptr_factory_.GetWeakPtr()));
}

}  // namespace proximity_auth
