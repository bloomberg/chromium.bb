// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PROXIMITY_AUTH_BLE_BLUETOOTH_LOW_ENERGY_CONNECTION_H_
#define COMPONENTS_PROXIMITY_AUTH_BLE_BLUETOOTH_LOW_ENERGY_CONNECTION_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <queue>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/cryptauth/ble/bluetooth_low_energy_characteristics_finder.h"
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

namespace proximity_auth {

// Represents a connection with a remote device over Bluetooth low energy. The
// connection is a persistent bidirectional channel for sending and receiving
// wire messages. The remote device is the peripheral mode and the service
// contains two characteristics: one to send data and another to receive it.
//
// The connection flow is described below.
//
//          Discover Reader and Writer Characteristics
//                              |
//                              |
//                              |
//                    Start Notify Session
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
class BluetoothLowEnergyConnection : public cryptauth::Connection,
                                     public device::BluetoothAdapter::Observer {
 public:
  // Signals sent to the remote device to indicate connection related events.
  enum class ControlSignal : uint32_t {
    kInviteToConnectSignal = 0,
    kInvitationResponseSignal = 1,
    kSendSignal = 2,
    kDisconnectSignal = 3,
  };

  // The sub-state of a proximity_auth::BluetoothLowEnergyConnection class
  // extends the IN_PROGRESS state of proximity_auth::Connection::Status.
  enum class SubStatus {
    DISCONNECTED,
    WAITING_GATT_CONNECTION,
    WAITING_CHARACTERISTICS,
    CHARACTERISTICS_FOUND,
    WAITING_NOTIFY_SESSION,
    NOTIFY_SESSION_READY,
    WAITING_RESPONSE_SIGNAL,
    CONNECTED,
  };

  // Constructs a Bluetooth low energy connection to the service with
  // |remote_service_| on the |remote_device|. The |adapter| must be already
  // initaalized and ready. The GATT connection may alreaady be established and
  // pass through |gatt_connection|. A subsequent call to Connect() must be
  // made.
  BluetoothLowEnergyConnection(
      const cryptauth::RemoteDevice& remote_device,
      scoped_refptr<device::BluetoothAdapter> adapter,
      const device::BluetoothUUID remote_service_uuid,
      cryptauth::BluetoothThrottler* bluetooth_throttler,
      int max_number_of_write_attempts);

  ~BluetoothLowEnergyConnection() override;

  // cryptauth::Connection:
  void Connect() override;
  void Disconnect() override;
  std::string GetDeviceAddress() override;

 protected:
  // Exposed for testing.
  void SetSubStatus(SubStatus status);
  SubStatus sub_status() { return sub_status_; }

  // Sets |task_runner_| for testing.
  void SetTaskRunnerForTesting(scoped_refptr<base::TaskRunner> task_runner);

  // Virtual for testing.
  virtual cryptauth::BluetoothLowEnergyCharacteristicsFinder*
  CreateCharacteristicsFinder(
      const cryptauth::BluetoothLowEnergyCharacteristicsFinder::SuccessCallback&
          success_callback,
      const cryptauth::BluetoothLowEnergyCharacteristicsFinder::ErrorCallback&
          error_callback);

  // cryptauth::Connection:
  void SendMessageImpl(
      std::unique_ptr<cryptauth::WireMessage> message) override;

  // device::BluetoothAdapter::Observer:
  void DeviceChanged(device::BluetoothAdapter* adapter,
                     device::BluetoothDevice* device) override;
  void DeviceRemoved(device::BluetoothAdapter* adapter,
                     device::BluetoothDevice* device) override;
  void GattCharacteristicValueChanged(
      device::BluetoothAdapter* adapter,
      device::BluetoothRemoteGattCharacteristic* characteristic,
      const std::vector<uint8_t>& value) override;

 private:
  // Represents a request to write |value| to a some characteristic.
  // |is_last_write_for_wire_messsage| indicates whether this request
  // corresponds to the last write request for some wire message.
  // A cryptauth::WireMessage corresponds to exactly two WriteRequest: the first
  // containing
  // a kSendSignal + the size of the cryptauth::WireMessage, and the second
  // containing a
  // SendStatusSignal + the serialized cryptauth::WireMessage.
  struct WriteRequest {
    WriteRequest(const std::vector<uint8_t>& val, bool flag);
    WriteRequest(const WriteRequest& other);
    ~WriteRequest();

    std::vector<uint8_t> value;
    bool is_last_write_for_wire_message;
    int number_of_failed_attempts;
  };

  // Creates the GATT connection with |remote_device|.
  void CreateGattConnection();

  // Called when a GATT connection is created.
  void OnGattConnectionCreated(
      std::unique_ptr<device::BluetoothGattConnection> gatt_connection);

  // Callback called when there is an error creating the GATT connection.
  void OnCreateGattConnectionError(
      device::BluetoothDevice::ConnectErrorCode error_code);

  // Callback called when |to_peripheral_char_| and |from_peripheral_char_| were
  // found.
  void OnCharacteristicsFound(
      const cryptauth::RemoteAttribute& service,
      const cryptauth::RemoteAttribute& to_peripheral_char,
      const cryptauth::RemoteAttribute& from_peripheral_char);

  // Callback called there was an error finding the characteristics.
  void OnCharacteristicsFinderError(
      const cryptauth::RemoteAttribute& to_peripheral_char,
      const cryptauth::RemoteAttribute& from_peripheral_char);

  // Starts a notify session for |from_peripheral_char_| when ready
  // (SubStatus::CHARACTERISTICS_FOUND).
  void StartNotifySession();

  // Called when a notification session is successfully started for
  // |from_peripheral_char_| characteristic.
  void OnNotifySessionStarted(
      std::unique_ptr<device::BluetoothGattNotifySession> notify_session);

  // Called when there is an error starting a notification session for
  // |from_peripheral_char_| characteristic.
  void OnNotifySessionError(device::BluetoothGattService::GattErrorCode);

  // Stops |notify_session_|.
  void StopNotifySession();

  // Sends an invite to connect signal to the peripheral if when ready
  // (SubStatus::NOTIFY_SESSION_READY).
  void SendInviteToConnectSignal();

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
  void WriteRemoteCharacteristic(WriteRequest request);

  // Processes the next request in |write_requests_queue_|.
  void ProcessNextWriteRequest();

  // Called when the
  // BluetoothRemoteGattCharacteristic::RemoteCharacteristicWrite() is
  // successfully complete.
  void OnRemoteCharacteristicWritten(bool run_did_send_message_callback);

  // Called when there is an error writing to the remote characteristic
  // |to_peripheral_char_|.
  void OnWriteRemoteCharacteristicError(
      bool run_did_send_message_callback,
      device::BluetoothRemoteGattService::GattErrorCode error);

  // Builds the value to be written on |to_peripheral_char_|. The value
  // corresponds to |signal| concatenated with |payload|.
  WriteRequest BuildWriteRequest(const std::vector<uint8_t>& signal,
                                 const std::vector<uint8_t>& bytes,
                                 bool is_last_message_for_wire_message);

  // Prints the time elapsed since |Connect()| was called.
  void PrintTimeElapsed();

  // Returns the device corresponding to |remote_device_address_|.
  device::BluetoothDevice* GetRemoteDevice();

  // Returns the service corresponding to |remote_service_| in the current
  // device.
  device::BluetoothRemoteGattService* GetRemoteService();

  // Returns the characteristic corresponding to |identifier| in the current
  // service.
  device::BluetoothRemoteGattCharacteristic* GetGattCharacteristic(
      const std::string& identifier);

  // Convert the first 4 bytes from a byte vector to a uint32_t.
  uint32_t ToUint32(const std::vector<uint8_t>& bytes);

  // Convert an uint32_t to a byte vector.
  const std::vector<uint8_t> ToByteVector(uint32_t value);

  // The Bluetooth adapter over which the Bluetooth connection will be made.
  scoped_refptr<device::BluetoothAdapter> adapter_;

  // Remote service the |gatt_connection_| was established with.
  cryptauth::RemoteAttribute remote_service_;

  // Characteristic used to send data to the remote device.
  cryptauth::RemoteAttribute to_peripheral_char_;

  // Characteristic used to receive data from the remote device.
  cryptauth::RemoteAttribute from_peripheral_char_;

  // Throttles repeated connection attempts to the same device. This is a
  // workaround for crbug.com/508919. Not owned, must outlive this instance.
  cryptauth::BluetoothThrottler* bluetooth_throttler_;

  scoped_refptr<base::TaskRunner> task_runner_;

  // The GATT connection with the remote device.
  std::unique_ptr<device::BluetoothGattConnection> gatt_connection_;

  // The characteristics finder for remote device.
  std::unique_ptr<cryptauth::BluetoothLowEnergyCharacteristicsFinder>
      characteristic_finder_;

  // The notify session for |from_peripheral_char|.
  std::unique_ptr<device::BluetoothGattNotifySession> notify_session_;

  // Internal connection status
  SubStatus sub_status_;

  // Indicates a receiving operation is in progress. This is set after a
  // ControlSignal::kSendSignal was received from the remote device.
  bool receiving_bytes_;

  // Total number of bytes expected for the current receive operation.
  std::size_t expected_number_of_incoming_bytes_;

  // Bytes already received for the current receive operation.
  std::string incoming_bytes_buffer_;

  // Indicates there is a
  // BluetoothRemoteGattCharacteristic::WriteRemoteCharacteristic
  // operation pending.
  bool write_remote_characteristic_pending_;

  std::queue<WriteRequest> write_requests_queue_;

  // Maximum number of tries to send any write request.
  int max_number_of_write_attempts_;

  // Maximum number of bytes that fit in a single chunk to be written in
  // |to_peripheral_char_|. Ideally, this should be the maximum value the
  // peripheral supports and it should be agreed when the GATT connection is
  // created. Currently, there is no API to find this value. The implementation
  // uses a hard-coded constant.
  int max_chunk_size_;

  // Stores when the instace was created.
  base::TimeTicks start_time_;

  base::WeakPtrFactory<BluetoothLowEnergyConnection> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothLowEnergyConnection);
};

}  // namespace proximity_auth

#endif  // COMPONENTS_PROXIMITY_AUTH_BLE_BLUETOOTH_LOW_ENERGY_CONNECTION_H_
