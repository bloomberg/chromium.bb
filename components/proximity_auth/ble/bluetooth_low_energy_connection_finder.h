// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PROXIMITY_AUTH_BLE_BLUETOOTH_LOW_ENERGY_CONNECTION_FINDER_H
#define COMPONENTS_PROXIMITY_AUTH_BLE_BLUETOOTH_LOW_ENERGY_CONNECTION_FINDER_H

#include <set>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/proximity_auth/connection.h"
#include "components/proximity_auth/connection_finder.h"
#include "components/proximity_auth/connection_observer.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_discovery_session.h"
#include "device/bluetooth/bluetooth_gatt_connection.h"

namespace proximity_auth {

// This ConnectionFinder implementation is specialized in finding a Bluetooth
// Low Energy remote device.
class BluetoothLowEnergyConnectionFinder
    : public ConnectionFinder,
      public ConnectionObserver,
      public device::BluetoothAdapter::Observer {
 public:
  BluetoothLowEnergyConnectionFinder(
      const std::string& remote_service_uuid,
      const std::string& to_peripheral_char_uuid,
      const std::string& from_peripheral_char_uuid,
      int max_number_of_tries);
  ~BluetoothLowEnergyConnectionFinder() override;

  // Finds a connection to the remote device. Only the first one is functional.
  void Find(const ConnectionCallback& connection_callback) override;

  // Closes the connection and forgets the device.
  void CloseGattConnection(
      scoped_ptr<device::BluetoothGattConnection> gatt_connection);

  // proximity_auth::ConnectionObserver:
  void OnConnectionStatusChanged(Connection* connection,
                                 Connection::Status old_status,
                                 Connection::Status new_status) override;

  // device::BluetoothAdapter::Observer:
  void DeviceAdded(device::BluetoothAdapter* adapter,
                   device::BluetoothDevice* device) override;
  void DeviceChanged(device::BluetoothAdapter* adapter,
                     device::BluetoothDevice* device) override;
  void DeviceRemoved(device::BluetoothAdapter* adapter,
                     device::BluetoothDevice* device) override;

 protected:
  // Creates a proximity_auth::Connection based on |gatt_connection|. Exposed
  // for testing.
  virtual scoped_ptr<Connection> CreateConnection(
      scoped_ptr<device::BluetoothGattConnection> gatt_connection);

  // Sets |delay_after_gatt_connection_| for testing.
  void SetDelayForTesting(base::TimeDelta delay);

 private:
  // Callback to be called when the Bluetooth adapter is initialized.
  void OnAdapterInitialized(scoped_refptr<device::BluetoothAdapter> adapter);

  // Checks if |remote_device| contains |remote_service_uuid| and creates a
  // connection in that case.
  void HandleDeviceUpdated(device::BluetoothDevice* remote_device);

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
  void OnCreateGattConnectionError(
      std::string device_address,
      device::BluetoothDevice::ConnectErrorCode error_code);

  // Callback called when a GATT connection is created.
  void OnGattConnectionCreated(
      scoped_ptr<device::BluetoothGattConnection> gatt_connection);

  // Creates a GATT connection with |remote_device|, |connection_callback_| will
  // be called once the connection is established.
  void CreateGattConnection(device::BluetoothDevice* remote_device);

  // Creates a BluetoothLowEnergyconnection object and adds the necessary
  // observers.
  void CompleteConnection();

  // The uuid of the service it looks for to establish a GattConnection.
  device::BluetoothUUID remote_service_uuid_;

  // Characteristic used to send data to the remote device.
  device::BluetoothUUID to_peripheral_char_uuid_;

  // Characteristic used to receive data from the remote device.
  device::BluetoothUUID from_peripheral_char_uuid_;

  // The Bluetooth adapter over which the Bluetooth connection will be made.
  scoped_refptr<device::BluetoothAdapter> adapter_;

  // The discovery session associated to this object.
  scoped_ptr<device::BluetoothDiscoverySession> discovery_session_;

  // True if a connection was established to a remote device that has the
  // service |remote_service_uuid|.
  bool connected_;

  // The GATT connection with |remote_device|.
  scoped_ptr<device::BluetoothGattConnection> gatt_connection_;

  // The connection with |remote_device|.
  scoped_ptr<Connection> connection_;

  // Callback called when the connection is established.
  // device::BluetoothDevice::GattConnectionCallback connection_callback_;
  ConnectionCallback connection_callback_;

  // The set of devices this connection finder has tried to connect to.
  std::set<device::BluetoothDevice*> pending_connections_;

  // BluetoothLowEnergyConnection parameter.
  int max_number_of_tries_;

  // Necessary delay after a GATT connection is created and before any
  // read/write request is sent to the characteristics.
  base::TimeDelta delay_after_gatt_connection_;

  base::WeakPtrFactory<BluetoothLowEnergyConnectionFinder> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothLowEnergyConnectionFinder);
};

}  // namespace proximity_auth

#endif  // COMPONENTS_PROXIMITY_AUTH_BLE_BLUETOOTH_CONNECTION_FINDER_H
