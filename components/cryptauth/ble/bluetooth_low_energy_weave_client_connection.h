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
#include "components/cryptauth/bluetooth_throttler.h"
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
        const device::BluetoothUUID remote_service_uuid,
        BluetoothThrottler* bluetooth_throttler);

    // Exposed for testing.
    static void SetInstanceForTesting(Factory* factory);

   protected:
    // Exposed for testing.
    virtual std::unique_ptr<Connection> BuildInstance(
        const RemoteDevice& remote_device,
        const std::string& device_address,
        scoped_refptr<device::BluetoothAdapter> adapter,
        const device::BluetoothUUID remote_service_uuid,
        BluetoothThrottler* bluetooth_throttler);

   private:
    static Factory* factory_instance_;
  };

  class TimerFactory {
   public:
    virtual std::unique_ptr<base::Timer> CreateTimer();
  };

  // The sub-state of a BluetoothLowEnergyWeaveClientConnection
  // extends the IN_PROGRESS state of Connection::Status.
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

  // Constructs a Bluetooth low energy connection to the service with
  // |remote_service_| on the |remote_device|. The |adapter| must be already
  // initialized and ready. The GATT connection may already be established and
  // pass through |gatt_connection|. A subsequent call to Connect() must be
  // made.
  BluetoothLowEnergyWeaveClientConnection(
      const RemoteDevice& remote_device,
      const std::string& device_address,
      scoped_refptr<device::BluetoothAdapter> adapter,
      const device::BluetoothUUID remote_service_uuid,
      BluetoothThrottler* bluetooth_throttler,
      std::unique_ptr<TimerFactory> timer_factory);

  ~BluetoothLowEnergyWeaveClientConnection() override;

  // namespace Connection:
  void Connect() override;
  void Disconnect() override;
  std::string GetDeviceAddress() override;

 protected:
  // Exposed for testing.
  // NOTE: This method may indirectly cause this object's destructor to be
  // called. Do not perform any operations that touch the internals of this
  // class after calling this method.
  void DestroyConnection();

  // Exposed for testing.
  SubStatus sub_status() { return sub_status_; }

  // Sets |task_runner_| for testing.
  void SetTaskRunnerForTesting(scoped_refptr<base::TaskRunner> task_runner);

  // Virtual for testing.
  virtual BluetoothLowEnergyCharacteristicsFinder* CreateCharacteristicsFinder(
      const BluetoothLowEnergyCharacteristicsFinder::SuccessCallback&
          success_callback,
      const BluetoothLowEnergyCharacteristicsFinder::ErrorCallback&
          error_callback);

  // namespace Connection:
  void SendMessageImpl(std::unique_ptr<WireMessage> message) override;

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
                 std::shared_ptr<WireMessage> message);
    WriteRequest(const Packet& val, WriteRequestType request_type);
    WriteRequest(const WriteRequest& other);
    ~WriteRequest();

    Packet value;
    WriteRequestType request_type;
    std::shared_ptr<WireMessage> message;
    int number_of_failed_attempts;
  };

  void SetSubStatus(SubStatus status);

  // Sets the connection interval before connecting.
  void SetConnectionLatency();

  // Creates the GATT connection with |remote_device|.
  void CreateGattConnection();

  // Called when a GATT connection is created.
  void OnGattConnectionCreated(
      std::unique_ptr<device::BluetoothGattConnection> gatt_connection);

  // Callback when there is an error setting the connection interval.
  void OnSetConnectionLatencyError();

  // Callback called when there is an error creating the GATT connection.
  void OnCreateGattConnectionError(
      device::BluetoothDevice::ConnectErrorCode error_code);

  // Callback called when |tx_characteristic_| and |rx_characteristic_| were
  // found.
  void OnCharacteristicsFound(const RemoteAttribute& service,
                              const RemoteAttribute& tx_characteristic,
                              const RemoteAttribute& rx_characteristic);

  // Callback called there was an error finding the characteristics.
  void OnCharacteristicsFinderError(const RemoteAttribute& tx_characteristic,
                                    const RemoteAttribute& rx_characteristic);

  // Starts a notify session for |rx_characteristic_| when ready
  // (SubStatus::CHARACTERISTICS_FOUND).
  void StartNotifySession();

  // Called when a notification session is successfully started for
  // |rx_characteristic_| characteristic.
  void OnNotifySessionStarted(
      std::unique_ptr<device::BluetoothGattNotifySession> notify_session);

  // Called when there is an error starting a notification session for
  // |rx_characteristic_| characteristic.
  void OnNotifySessionError(device::BluetoothGattService::GattErrorCode);

  // Stops |notify_session_|.
  void StopNotifySession();

  // Sends a connection request to server if ready
  // (SubStatus::NOTIFY_SESSION_READY).
  void SendConnectionRequest();

  // Completes and updates the status accordingly.
  void CompleteConnection();

  // This is the only entry point for WriteRequests, which are processed
  // accordingly the following flow:
  // 1) |request| is enqueued;
  // 2) |request| will be processed by ProcessNextWriteRequest() when there is
  // no pending write request;
  // 3) |request| will be dequeued when it's successfully processed
  // (OnRemoteCharacteristicWritten());
  // 4) |request| is not dequeued if it fails
  // (OnWriteRemoteCharacteristicError()), it remains on the queue and will be
  // retried. |request| will remain on the queue until it succeeds or it
  // triggers a Disconnect() call (after |max_number_of_tries_|).
  void WriteRemoteCharacteristic(const WriteRequest& request);

  // Processes the next request in |write_requests_queue_|.
  void ProcessNextWriteRequest();

  // Called when the
  // BluetoothRemoteGattCharacteristic::RemoteCharacteristicWrite() is
  // successfully complete.
  void OnRemoteCharacteristicWritten();

  // Called when there is an error writing to the remote characteristic
  // |tx_characteristic_|.
  void OnWriteRemoteCharacteristicError(
      device::BluetoothRemoteGattService::GattErrorCode error);

  void OnPacketReceiverError();

  // Called when waiting for connection response from server times out.
  void OnConnectionResponseTimeout();

  // Returns the service corresponding to |remote_service_| in the current
  // device.
  device::BluetoothRemoteGattService* GetRemoteService();

  // Returns the characteristic corresponding to |identifier| in the current
  // service.
  device::BluetoothRemoteGattCharacteristic* GetGattCharacteristic(
      const std::string& identifier);

  // Get the reason that the other side of the connection decided to close the
  // connection.
  // Will crash if the receiver is not in CONNECTION_CLOSED state.
  std::string GetReasonForClose();

  // Returns the device corresponding to |device_address_|.
  device::BluetoothDevice* GetBluetoothDevice();

  // The device to which to connect.
  const std::string device_address_;

  // The Bluetooth adapter over which the Bluetooth connection will be made.
  scoped_refptr<device::BluetoothAdapter> adapter_;

  // Remote service the |gatt_connection_| was established with.
  RemoteAttribute remote_service_;

  // uWeave packet generator.
  std::unique_ptr<BluetoothLowEnergyWeavePacketGenerator> packet_generator_;

  // uWeave packet receiver.
  std::unique_ptr<BluetoothLowEnergyWeavePacketReceiver> packet_receiver_;

  // Characteristic used to send data to the remote device.
  RemoteAttribute tx_characteristic_;

  // Characteristic used to receive data from the remote device.
  RemoteAttribute rx_characteristic_;

  // Throttles repeated connection attempts to the same device. This is a
  // workaround for crbug.com/508919. Not owned, must outlive this instance.
  BluetoothThrottler* bluetooth_throttler_;

  // Used for timing out when waiting for connection response from the server.
  std::unique_ptr<TimerFactory> timer_factory_;

  scoped_refptr<base::TaskRunner> task_runner_;

  // The GATT connection with the remote device.
  std::unique_ptr<device::BluetoothGattConnection> gatt_connection_;

  // The characteristics finder for remote device.
  std::unique_ptr<BluetoothLowEnergyCharacteristicsFinder>
      characteristic_finder_;

  // The notify session for |rx_characteristic|.
  std::unique_ptr<device::BluetoothGattNotifySession> notify_session_;

  // Internal connection status
  SubStatus sub_status_;

  // Bytes already received for the current receive operation.
  std::string incoming_bytes_buffer_;

  // Indicates there is a
  // BluetoothRemoteGattCharacteristic::WriteRemoteCharacteristic
  // operation pending.
  bool write_remote_characteristic_pending_;

  std::queue<WriteRequest> write_requests_queue_;

  // Used for timing out when waiting for connection response from the server.
  std::unique_ptr<base::Timer> connection_response_timer_;

  base::WeakPtrFactory<BluetoothLowEnergyWeaveClientConnection>
      weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothLowEnergyWeaveClientConnection);
};

}  // namespace weave

}  // namespace cryptauth

#endif  // COMPONENTS_CRYPTAUTH_BLE_BLUETOOTH_LOW_ENERGY_WEAVE_CLIENT_CONNECTION_H_
