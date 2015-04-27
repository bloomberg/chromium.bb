// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PROXIMITY_AUTH_BLE_BLUETOOTH_LOW_ENERGY_CONNECTION_FINDER_H
#define COMPONENTS_PROXIMITY_AUTH_BLE_BLUETOOTH_LOW_ENERGY_CONNECTION_FINDER_H

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/proximity_auth/connection_finder.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_discovery_session.h"
#include "device/bluetooth/bluetooth_gatt_connection.h"

namespace proximity_auth {

// This ConnectionFinder implementation is specialized in finding a Bluetooth
// Low Energy remote device.
class BluetoothLowEnergyConnectionFinder
    : public ConnectionFinder,
      public device::BluetoothAdapter::Observer {
 public:
  BluetoothLowEnergyConnectionFinder(const std::string& remote_service_uuid);
  ~BluetoothLowEnergyConnectionFinder() override;

  // Finds a connection the remote device, only the first one is functional.
  void Find(const device::BluetoothDevice::GattConnectionCallback&
                connection_callback);
  void Find(const ConnectionCallback& connection_callback) override;

 protected:
  // device::BluetoothAdapter::Observer:
  void DeviceAdded(device::BluetoothAdapter* adapter,
                   device::BluetoothDevice* device) override;

 private:
  // Callback to be called when the Bluetooth adapter is initialized.
  void OnAdapterInitialized(scoped_refptr<device::BluetoothAdapter> adapter);

  // Checks if |remote_device| contains |remote_service_uuid| and creates a
  // connection in that case.
  void HandleDeviceAdded(device::BluetoothDevice* remote_device);

  // Callback called when a new discovery session is started.
  void OnDiscoverySessionStarted(
      scoped_ptr<device::BluetoothDiscoverySession> discovery_session);

  // Callback called when there is an error starting a new discovery session.
  void OnStartDiscoverySessionError();

  // Starts a discovery session for |adapter_|.
  void StartDiscoverySession();

  // Callback called when |discovery_session_| is stopped.
  void OnDiscoverySessionStopped();

  // Callback called when there is an error stopping |discovery_session_|.
  void OnStopDiscoverySessionError();

  // Stops the discovery session given by |discovery_session_|.
  void StopDiscoverySession();

  // Checks if a service with |service_uuid| is offered by |remote_device|.
  bool HasService(device::BluetoothDevice* remote_device);

  // Callback called when there is an error creating the connection.
  void OnCreateConnectionError(
      device::BluetoothDevice::ConnectErrorCode error_code);

  // Callback called when the connection is created.
  void OnConnectionCreated(
      scoped_ptr<device::BluetoothGattConnection> connection);

  // Creates a GATT connection with |remote_device|, |connection_callback_| will
  // be called once the connection is established.
  void CreateConnection(device::BluetoothDevice* remote_device);

  // The uuid of the service it looks for to establish a GattConnection.
  device::BluetoothUUID remote_service_uuid_;

  // The Bluetooth adapter over which the Bluetooth connection will be made.
  scoped_refptr<device::BluetoothAdapter> adapter_;

  // The discovery session associated to this object.
  scoped_ptr<device::BluetoothDiscoverySession> discovery_session_;

  // True if there is a connection.
  bool connected_;

  // Callback called when the connection is established.
  device::BluetoothDevice::GattConnectionCallback connection_callback_;

  base::WeakPtrFactory<BluetoothLowEnergyConnectionFinder> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothLowEnergyConnectionFinder);
};

}  // namespace proximity_auth

#endif  // COMPONENTS_PROXIMITY_AUTH_BLE_BLUETOOTH_CONNECTION_FINDER_H
