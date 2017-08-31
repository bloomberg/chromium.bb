// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_CRYPTAUTH_BLE_BLUETOOTH_LOW_ENERGY_WEAVE_CLIENT_CONNECTION_H_
#define COMPONENTS_CRYPTAUTH_BLE_BLUETOOTH_LOW_ENERGY_WEAVE_CLIENT_CONNECTION_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <queue>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/cryptauth/ble/bluetooth_low_energy_characteristics_finder.h"
#include "components/cryptauth/ble/bluetooth_low_energy_weave_packet_generator.h"
#include "components/cryptauth/ble/bluetooth_low_energy_weave_packet_receiver.h"
#include "components/cryptauth/ble/fake_wire_message.h"
#include "components/cryptauth/ble/remote_attribute.h"
#include "components/cryptauth/connection.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_gatt_notify_session.h"
#include "device/bluetooth/bluetooth_remote_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_uuid.h"

namespace base {
class TaskRunner;
}

namespace cryptauth {

namespace weave {

// Creates GATT connection on top of the BLE connection and act as a Client.
// uWeave communication follows the flow:
//
// Client                           | Server
// ---------------------------------|--------------------------------
// send connection request          |
//                                  | receive connection request
//                                  | send connection response
// receive connection response      |
// opt: send data                   | opt: send data
// receive data                     | receive data
// opt: close connection            | opt: close connection
class BluetoothLowEnergyWeaveClientConnection
    : public Connection,
      public device::BluetoothAdapter::Observer {
 public:
  class Factory {
   public:
    static std::unique_ptr<Connection> NewInstance(
        const RemoteDevice& remote_device,
        const std::string& device_address,
        scoped_refptr<device::BluetoothAdapter> adapter,
        const device::BluetoothUUID remote_service_uuid);
    static void SetInstanceForTesting(Factory* factory);

   protected:
    virtual std::unique_ptr<Connection> BuildInstance(
        const RemoteDevice& remote_device,
        const std::string& device_address,
        scoped_refptr<device::BluetoothAdapter> adapter,
        const device::BluetoothUUID remote_service_uuid);

   private:
    static Factory* factory_instance_;
  };

  // TODO(thakis): It looks like this could be stack-allocated and passed by
  // pointer instead of by unique_ptr, removing the need for a virtual dtor.
  class TimerFactory {
   public:
    virtual ~TimerFactory();

    virtual std::unique_ptr<base::Timer> CreateTimer();
  };

  enum SubStatus {
    DISCONNECTED,
    WAITING_CONNECTION_LATENCY,
    WAITING_GATT_CONNECTION,
    WAITING_CHARACTERISTICS,
    CHARACTERISTICS_FOUND,
    WAITING_NOTIFY_SESSION,
    NOTIFY_SESSION_READY,
    WAITING_CONNECTION_RESPONSE,
    CONNECTED,
  };

  // Constructs the Connection object; a subsequent call to Connect() is
  // necessary to initiate the BLE connection.
  BluetoothLowEnergyWeaveClientConnection(
      const RemoteDevice& remote_device,
      const std::string& device_address,
      scoped_refptr<device::BluetoothAdapter> adapter,
      const device::BluetoothUUID remote_service_uuid);

  ~BluetoothLowEnergyWeaveClientConnection() override;

  // Connection:
  void Connect() override;
  void Disconnect() override;
  std::string GetDeviceAddress() override;

 protected:
  // Destroys the connection immediately; if there was an active connection, it
  // will be disconnected after this call. Note that this function may notify
  // observers of a connection status change.
  void DestroyConnection();

  SubStatus sub_status() { return sub_status_; }

  void SetupTestDoubles(
      scoped_refptr<base::TaskRunner> test_task_runner,
      std::unique_ptr<TimerFactory> test_timer_factory,
      std::unique_ptr<BluetoothLowEnergyWeavePacketGenerator> test_generator,
      std::unique_ptr<BluetoothLowEnergyWeavePacketReceiver> test_receiver);

  virtual BluetoothLowEnergyCharacteristicsFinder* CreateCharacteristicsFinder(
      const BluetoothLowEnergyCharacteristicsFinder::SuccessCallback&
          success_callback,
      const BluetoothLowEnergyCharacteristicsFinder::ErrorCallback&
          error_callback);

  // Connection:
  void SendMessageImpl(std::unique_ptr<WireMessage> message) override;
  void OnDidSendMessage(const WireMessage& message, bool success) override;

  // device::BluetoothAdapter::Observer:
  void DeviceChanged(device::BluetoothAdapter* adapter,
                     device::BluetoothDevice* device) override;
  void DeviceRemoved(device::BluetoothAdapter* adapter,
                     device::BluetoothDevice* device) override;
  void GattCharacteristicValueChanged(
      device::BluetoothAdapter* adapter,
      device::BluetoothRemoteGattCharacteristic* characteristic,
      const Packet& value) override;

 private:
  enum WriteRequestType {
    REGULAR,
    MESSAGE_COMPLETE,
    CONNECTION_REQUEST,
    CONNECTION_CLOSE
  };

  // Represents a request to write |value| to a some characteristic.
  // |is_last_write_for_wire_messsage| indicates whether this request
  // corresponds to the last write request for some wire message.
  struct WriteRequest {
    WriteRequest(const Packet& val,
                 WriteRequestType request_type,
                 WireMessage* associated_wire_message);
    WriteRequest(const Packet& val, WriteRequestType request_type);
    WriteRequest(const WireMessage& other);
    virtual ~WriteRequest();

    Packet value;
    WriteRequestType request_type;
    WireMessage* associated_wire_message;
    int number_of_failed_attempts = 0;
  };

  // Sets the current status; if |status| corresponds to one of Connection's
  // Status types, observers will be notified of the change.
  void SetSubStatus(SubStatus status);

  // These functions are used to set up the connection so that it is ready to
  // send/receive data.
  void SetConnectionLatency();
  void CreateGattConnection();
  void OnGattConnectionCreated(
      std::unique_ptr<device::BluetoothGattConnection> gatt_connection);
  void OnSetConnectionLatencyError();
  void OnCreateGattConnectionError(
      device::BluetoothDevice::ConnectErrorCode error_code);
  void OnCharacteristicsFound(const RemoteAttribute& service,
                              const RemoteAttribute& tx_characteristic,
                              const RemoteAttribute& rx_characteristic);
  void OnCharacteristicsFinderError(const RemoteAttribute& tx_characteristic,
                                    const RemoteAttribute& rx_characteristic);
  void StartNotifySession();
  void OnNotifySessionStarted(
      std::unique_ptr<device::BluetoothGattNotifySession> notify_session);
  void OnNotifySessionError(device::BluetoothGattService::GattErrorCode);

  // Sends the connection request message (the first message in the uWeave
  // handshake).
  void SendConnectionRequest();
  void OnConnectionResponseTimeout();

  // Completes and updates the status accordingly.
  void CompleteConnection();

  // If no write is in progress and there are queued packets, sends the next
  // packet; if there is already a write in progress or there are no queued
  // packets, this function is a no-op.
  void ProcessNextWriteRequest();

  void SendPendingWriteRequest();
  void OnRemoteCharacteristicWritten();
  void OnWriteRemoteCharacteristicError(
      device::BluetoothRemoteGattService::GattErrorCode error);
  void ClearQueueAndSendConnectionClose();

  // Private getters for the Bluetooth classes corresponding to this connection.
  device::BluetoothRemoteGattService* GetRemoteService();
  device::BluetoothRemoteGattCharacteristic* GetGattCharacteristic(
      const std::string& identifier);
  device::BluetoothDevice* GetBluetoothDevice();

  // Get the reason that the other side of the connection decided to close the
  // connection.
  std::string GetReasonForClose();

  // The device to which to connect. This is the starting value, but the device
  // address may change during the connection because BLE addresses are
  // ephemeral. Use GetDeviceAddress() to get the most up-to-date address.
  const std::string device_address_;

  scoped_refptr<device::BluetoothAdapter> adapter_;
  RemoteAttribute remote_service_;
  std::unique_ptr<BluetoothLowEnergyWeavePacketGenerator> packet_generator_;
  std::unique_ptr<BluetoothLowEnergyWeavePacketReceiver> packet_receiver_;
  RemoteAttribute tx_characteristic_;
  RemoteAttribute rx_characteristic_;
  std::unique_ptr<TimerFactory> timer_factory_;
  scoped_refptr<base::TaskRunner> task_runner_;
  std::unique_ptr<base::Timer> connection_response_timer_;

  // These pointers start out null and are created during the connection
  // process.
  std::unique_ptr<device::BluetoothGattConnection> gatt_connection_;
  std::unique_ptr<BluetoothLowEnergyCharacteristicsFinder>
      characteristic_finder_;
  std::unique_ptr<device::BluetoothGattNotifySession> notify_session_;

  SubStatus sub_status_;

  // The WriteRequest that is currently being sent as well as those queued to be
  // sent. Each WriteRequest corresponds to one uWeave packet to be sent.
  std::unique_ptr<WriteRequest> pending_write_request_;
  std::queue<std::unique_ptr<WriteRequest>> queued_write_requests_;

  // WireMessages queued to be sent. Each WireMessage correponds to one or more
  // WriteRequests. WireMessages remain in this queue until the last
  // corresponding WriteRequest has been sent.
  std::queue<std::unique_ptr<WireMessage>> queued_wire_messages_;

  base::WeakPtrFactory<BluetoothLowEnergyWeaveClientConnection>
      weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothLowEnergyWeaveClientConnection);
};

}  // namespace weave

}  // namespace cryptauth

#endif  // COMPONENTS_CRYPTAUTH_BLE_BLUETOOTH_LOW_ENERGY_WEAVE_CLIENT_CONNECTION_H_
