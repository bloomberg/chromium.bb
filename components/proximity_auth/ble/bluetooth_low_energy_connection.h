// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PROXIMITY_AUTH_BLUETOOTH_LOW_ENERGY_CONNECTION_H
#define COMPONENTS_PROXIMITY_AUTH_BLUETOOTH_LOW_ENERGY_CONNECTION_H

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/proximity_auth/ble/fake_wire_message.h"
#include "components/proximity_auth/connection.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_gatt_notify_session.h"
#include "device/bluetooth/bluetooth_uuid.h"

namespace proximity_auth {

// Represents a connection with a remote device over Bluetooth low energy. The
// connection is a persistent bidirectional channel for sending and receiving
// wire messages. The remote device is the peripheral mode and the service
// contains two characteristics: one to send data and another to receive it.
class BluetoothLowEnergyConnection : public Connection,
                                     public device::BluetoothAdapter::Observer {
 public:
  enum class ControlSignal : uint32 {
    kInviteToConnectSignal = 0,
    kInvitationResponseSignal = 1,
    kSendSignal = 2,
    kDisconnectSignal = 3,
  };

  // Constructs a Bluetooth low energy connection to the service with
  // |remote_service_| on the |remote_device|. The GATT connection with
  // |remote_device| must be already established and |adapter| already
  // initialized.
  //
  // The connection flow is described below.
  //
  // Discover Reader Characteristic         Discover Writer Characteristic
  //           |                                         |
  //   Start Notify Session                              |
  //           |                                         |
  //            ---------------- && ---------------------
  //                              |
  //                              |
  //                              |
  //     Write kInviteToConnectSignal to Writer Characteristic
  //                              |
  //                              |
  //                              |
  //    Read kInvitationResponseSignal from Reader Characteristic
  //                              |
  //                              |
  //                              |
  //           Proximity Auth Connection Established
  BluetoothLowEnergyConnection(
      const RemoteDevice& remote_device,
      scoped_refptr<device::BluetoothAdapter> adapter,
      const device::BluetoothUUID remote_service_uuid,
      const device::BluetoothUUID to_peripheral_char_uuid,
      const device::BluetoothUUID from_peripheral_char_uuid,
      scoped_ptr<device::BluetoothGattConnection> gatt_connection);

  ~BluetoothLowEnergyConnection() override;

  // proximity_auth::Connection
  void Connect() override;
  void Disconnect() override;

 protected:
  // proximity_auth::Connection
  void SendMessageImpl(scoped_ptr<WireMessage> message) override;
  scoped_ptr<WireMessage> DeserializeWireMessage(
      bool* is_incomplete_message) override;

  // device::BluetoothAdapter::Observer
  void DeviceRemoved(device::BluetoothAdapter* adapter,
                     device::BluetoothDevice* device) override;
  void GattDiscoveryCompleteForService(
      device::BluetoothAdapter* adapter,
      device::BluetoothGattService* service) override;
  void GattCharacteristicAdded(
      device::BluetoothAdapter* adapter,
      device::BluetoothGattCharacteristic* characteristic) override;
  void GattCharacteristicValueChanged(
      device::BluetoothAdapter* adapter,
      device::BluetoothGattCharacteristic* characteristic,
      const std::vector<uint8>& value) override;

 private:
  // Represents a remote characteristic or service.
  struct RemoteBleObject {
    device::BluetoothUUID uuid;
    std::string id;
  };

  // Sends an invite to connect signal to the peripheral if the connection
  // is ready. That is, satisfying the conditions:
  // (i) |to_peripheral_char_| and |from_peripheral_char_| were found;
  // (ii) |notify_session_| was set for |from_peripheral_char_|.
  //
  // The asynchronous events that can casue these conditions to be
  // satisfied are:
  // (i) a new characteristic is discovered (HandleCharacteristicUpdate);
  // (ii) a new notify session is started (OnNotifySessionStarted).
  void SendInviteToConnectSignal();

  // Called when there is an error writing to the remote characteristic
  // |to_peripheral_char_|.
  void OnWriteRemoteCharacteristicError(
      device::BluetoothGattService::GattErrorCode error);

  // Handles the discovery of a new characteristic.
  void HandleCharacteristicUpdate(
      device::BluetoothGattCharacteristic* characteristic);

  // Scans the remote chracteristics of |service| calling
  // HandleCharacteristicUpdate() for each of them.
  void ScanRemoteCharacteristics(device::BluetoothGattService* service);

  // This function verifies if the connection is complete and updates
  // the status accordingly. The only asynchronous event
  // that can cause the connection to be completed is receiving a
  // kInvitationResponseSignal (GattCharateristcValueChanged).
  void CompleteConnection();

  // Starts a notify session for |from_peripheral_char_|.
  void StartNotifySession();

  // Called when there is an error starting a notification session for
  // |from_peripheral_char_| characteristic.
  void OnNotifySessionError(device::BluetoothGattService::GattErrorCode);

  // Called when a notification session is successfully started for
  // |from_peripheral_char_| characteristic.
  void OnNotifySessionStarted(
      scoped_ptr<device::BluetoothGattNotifySession> notify_session);

  // Stops |notify_session_|.
  void StopNotifySession();

  // Updates the value of |to_peripheral_char_| and
  // |from_peripheral_char_|
  // when |characteristic| was found.
  void UpdateCharacteristicsStatus(
      device::BluetoothGattCharacteristic* characteristic);

  // Returns the Bluetooth address of the remote device.
  const std::string& GetRemoteDeviceAddress();

  // Returns the device corresponding to |remote_device_address_|.
  device::BluetoothDevice* GetRemoteDevice();

  // Returns the service corresponding to |remote_service_| in the current
  // device.
  device::BluetoothGattService* GetRemoteService();

  // Returns the characteristic corresponding to |identifier| in the current
  // service.
  device::BluetoothGattCharacteristic* GetGattCharacteristic(
      const std::string& identifier);

  // Convert the first 4 bytes from a byte vector to a uint32.
  uint32 ToUint32(const std::vector<uint8>& bytes);

  // Convert an uint32 to a byte array.
  const std::string ToString(const uint32 value);

  // The Bluetooth adapter over which the Bluetooth connection will be made.
  scoped_refptr<device::BluetoothAdapter> adapter_;

  // Remote service the |connection_| was established with.
  RemoteBleObject remote_service_;

  // Characteristic used to send data to the remote device.
  RemoteBleObject to_peripheral_char_;

  // Characteristic used to receive data from the remote device.
  RemoteBleObject from_peripheral_char_;

  // The GATT connection with the remote device.
  scoped_ptr<device::BluetoothGattConnection> connection_;

  // Indicates if there is a pending notification session. Used to ensure there
  // is only one pending notify session.
  bool notify_session_pending_;

  // The notify session for |from_peripheral_char|.
  scoped_ptr<device::BluetoothGattNotifySession> notify_session_;

  // Indicates if there pending response to a invite to connect signal.
  bool connect_signal_response_pending_;

  // Stores when the instace was created.
  base::TimeTicks start_time_;

  base::WeakPtrFactory<BluetoothLowEnergyConnection> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothLowEnergyConnection);
};

}  // namespace proximity_auth

#endif  // COMPONENTS_PROXIMITY_AUTH_BLUETOOTH_LOW_ENERGY_CONNECTION_H
