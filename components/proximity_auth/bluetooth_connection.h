// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PROXIMITY_AUTH_BLUETOOTH_CONNECTION_H
#define COMPONENTS_PROXIMITY_AUTH_BLUETOOTH_CONNECTION_H

#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "components/proximity_auth/connection.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_socket.h"
#include "device/bluetooth/bluetooth_uuid.h"

namespace net {
class IOBuffer;
}

namespace proximity_auth {

struct RemoteDevice;

// Represents a Bluetooth connection with a remote device. The connection is a
// persistent bidirectional channel for sending and receiving wire messages.
class BluetoothConnection : public Connection,
                            public device::BluetoothAdapter::Observer {
 public:
  // Constructs a Bluetooth connection to the service with |uuid| on the
  // |remote_device|. The |remote_device| must already be known to the system
  // Bluetooth daemon.
  BluetoothConnection(const RemoteDevice& remote_device,
                      const device::BluetoothUUID& uuid);
  ~BluetoothConnection() override;

  // Connection:
  void Connect() override;
  void Disconnect() override;

 protected:
  // Connection:
  void SendMessageImpl(std::unique_ptr<WireMessage> message) override;

  // BluetoothAdapter::Observer:
  void DeviceChanged(device::BluetoothAdapter* adapter,
                     device::BluetoothDevice* device) override;
  void DeviceRemoved(device::BluetoothAdapter* adapter,
                     device::BluetoothDevice* device) override;

 private:
  // Registers receive callbacks with the backing |socket_|.
  void StartReceive();

  // Callbacks for asynchronous Bluetooth operations.
  void OnAdapterInitialized(scoped_refptr<device::BluetoothAdapter> adapter);
  void OnConnected(scoped_refptr<device::BluetoothSocket> socket);
  void OnConnectionError(const std::string& error_message);
  void OnSend(int bytes_sent);
  void OnSendError(const std::string& error_message);
  void OnReceive(int bytes_received, scoped_refptr<net::IOBuffer> buffer);
  void OnReceiveError(device::BluetoothSocket::ErrorReason error_reason,
                      const std::string& error_message);

  // The UUID (universally unique identifier) of the Bluetooth service on the
  // remote device that |this| connection should connect to.
  const device::BluetoothUUID uuid_;

  // The Bluetooth adapter over which this connection is made. Non-null iff
  // |this| connection is registered as an observer of the |adapter_|.
  scoped_refptr<device::BluetoothAdapter> adapter_;

  // The Bluetooth socket that backs this connection. NULL iff the connection is
  // not in a connected state.
  scoped_refptr<device::BluetoothSocket> socket_;

  // The message that was sent over the backing |socket_|. NULL iff there is no
  // send operation in progress.
  std::unique_ptr<WireMessage> pending_message_;

  base::WeakPtrFactory<BluetoothConnection> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothConnection);
};

}  // namespace proximity_auth

#endif  // COMPONENTS_PROXIMITY_AUTH_BLUETOOTH_CONNECTION_H
