// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proximity_auth/ble/bluetooth_low_energy_connection.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/cryptauth/ble/bluetooth_low_energy_characteristics_finder.h"
#include "components/cryptauth/ble/fake_wire_message.h"
#include "components/cryptauth/bluetooth_throttler.h"
#include "components/cryptauth/connection.h"
#include "components/cryptauth/connection_finder.h"
#include "components/cryptauth/wire_message.h"
#include "components/proximity_auth/logging/logging.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_gatt_connection.h"
#include "device/bluetooth/bluetooth_gatt_notify_session.h"
#include "device/bluetooth/bluetooth_remote_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_uuid.h"

using device::BluetoothAdapter;
using device::BluetoothDevice;
using device::BluetoothGattConnection;
using device::BluetoothRemoteGattService;
using device::BluetoothRemoteGattCharacteristic;
using device::BluetoothGattNotifySession;
using device::BluetoothUUID;

namespace proximity_auth {
namespace {

// The UUID of the characteristic used to send data to the peripheral.
const char kToPeripheralCharUUID[] = "977c6674-1239-4e72-993b-502369b8bb5a";

// The UUID of the characteristic used to receive data from the peripheral.
const char kFromPeripheralCharUUID[] = "f4b904a2-a030-43b3-98a8-221c536c03cb";

// Deprecated signal send as the first byte in send byte operations.
const int kFirstByteZero = 0;

// The maximum number of bytes written in a remote characteristic with a single
// write request. This is not the connection MTU, we are assuming that the
// remote device allows for writes larger than MTU.
const int kMaxChunkSize = 500;
}  // namespace

BluetoothLowEnergyConnection::BluetoothLowEnergyConnection(
    const cryptauth::RemoteDevice& device,
    scoped_refptr<device::BluetoothAdapter> adapter,
    const BluetoothUUID remote_service_uuid,
    cryptauth::BluetoothThrottler* bluetooth_throttler,
    int max_number_of_write_attempts)
    : cryptauth::Connection(device),
      adapter_(adapter),
      remote_service_({remote_service_uuid, ""}),
      to_peripheral_char_({BluetoothUUID(kToPeripheralCharUUID), ""}),
      from_peripheral_char_({BluetoothUUID(kFromPeripheralCharUUID), ""}),
      bluetooth_throttler_(bluetooth_throttler),
      task_runner_(base::ThreadTaskRunnerHandle::Get()),
      sub_status_(SubStatus::DISCONNECTED),
      receiving_bytes_(false),
      write_remote_characteristic_pending_(false),
      max_number_of_write_attempts_(max_number_of_write_attempts),
      max_chunk_size_(kMaxChunkSize),
      weak_ptr_factory_(this) {
  DCHECK(adapter_);
  DCHECK(adapter_->IsInitialized());

  adapter_->AddObserver(this);
}

BluetoothLowEnergyConnection::~BluetoothLowEnergyConnection() {
  Disconnect();
  if (adapter_) {
    adapter_->RemoveObserver(this);
    adapter_ = NULL;
  }
}

void BluetoothLowEnergyConnection::Connect() {
  DCHECK(sub_status() == SubStatus::DISCONNECTED);

  SetSubStatus(SubStatus::WAITING_GATT_CONNECTION);
  base::TimeDelta throttler_delay = bluetooth_throttler_->GetDelay();
  PA_LOG(INFO) << "Connecting in  " << throttler_delay;

  start_time_ = base::TimeTicks::Now();

  // If necessary, wait to create a new GATT connection.
  //
  // Avoid creating a new GATT connection immediately after a given device was
  // disconnected. This is a workaround for crbug.com/508919.
  if (!throttler_delay.is_zero()) {
    task_runner_->PostDelayedTask(
        FROM_HERE,
        base::Bind(&BluetoothLowEnergyConnection::CreateGattConnection,
                   weak_ptr_factory_.GetWeakPtr()),
        throttler_delay);
    return;
  }

  CreateGattConnection();
}

void BluetoothLowEnergyConnection::CreateGattConnection() {
  DCHECK(sub_status() == SubStatus::WAITING_GATT_CONNECTION);

  BluetoothDevice* remote_device = GetRemoteDevice();
  if (remote_device) {
    PA_LOG(INFO) << "Creating GATT connection with "
                 << remote_device->GetAddress();

    remote_device->CreateGattConnection(
        base::Bind(&BluetoothLowEnergyConnection::OnGattConnectionCreated,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&BluetoothLowEnergyConnection::OnCreateGattConnectionError,
                   weak_ptr_factory_.GetWeakPtr()));
  }
}

void BluetoothLowEnergyConnection::Disconnect() {
  if (sub_status() != SubStatus::DISCONNECTED) {
    weak_ptr_factory_.InvalidateWeakPtrs();
    StopNotifySession();
    characteristic_finder_.reset();
    if (gatt_connection_) {
      PA_LOG(INFO) << "Disconnect from device "
                   << gatt_connection_->GetDeviceAddress();

      // Destroying BluetoothGattConnection also disconnects it.
      gatt_connection_.reset();
    }

    // Only transition to the DISCONNECTED state after perfoming all necessary
    // operations. Otherwise, it'll trigger observers that can pontentially
    // destroy the current instance (causing a crash).
    SetSubStatus(SubStatus::DISCONNECTED);
  }
}

void BluetoothLowEnergyConnection::SetSubStatus(SubStatus new_sub_status) {
  sub_status_ = new_sub_status;

  // Sets the status of parent class cryptauth::Connection accordingly.
  if (new_sub_status == SubStatus::CONNECTED) {
    SetStatus(CONNECTED);
  } else if (new_sub_status == SubStatus::DISCONNECTED) {
    SetStatus(DISCONNECTED);
  } else {
    SetStatus(IN_PROGRESS);
  }
}

void BluetoothLowEnergyConnection::SetTaskRunnerForTesting(
    scoped_refptr<base::TaskRunner> task_runner) {
  task_runner_ = task_runner;
}

void BluetoothLowEnergyConnection::SendMessageImpl(
    std::unique_ptr<cryptauth::WireMessage> message) {
  PA_LOG(INFO) << "Sending message " << message->Serialize();
  std::string serialized_msg = message->Serialize();

  // [First write]: Build a header with the [send signal] + [size of the
  // message].
  WriteRequest write_request = BuildWriteRequest(
      ToByteVector(static_cast<uint32_t>(ControlSignal::kSendSignal)),
      ToByteVector(static_cast<uint32_t>(serialized_msg.size())), false);

  // [First write]: Fill the it with a prefix of |serialized_msg| up to
  // |max_chunk_size_|.
  size_t first_chunk_size = std::min(
      max_chunk_size_ - write_request.value.size(), serialized_msg.size());
  std::vector<uint8_t> bytes(serialized_msg.begin(),
                             serialized_msg.begin() + first_chunk_size);
  write_request.value.insert(write_request.value.end(), bytes.begin(),
                             bytes.end());

  bool is_last_write_request = first_chunk_size == serialized_msg.size();
  write_request.is_last_write_for_wire_message = is_last_write_request;
  WriteRemoteCharacteristic(write_request);
  if (is_last_write_request)
    return;

  // [Other write requests]: Each chunk has to include a deprecated signal:
  // |kFirstByteZero| as the first byte.
  int chunk_size = max_chunk_size_ - 1;
  std::vector<uint8_t> kFirstByteZeroVector;
  kFirstByteZeroVector.push_back(static_cast<uint8_t>(kFirstByteZero));

  int message_size = static_cast<int>(serialized_msg.size());
  int start_index = first_chunk_size;
  while (start_index < message_size) {
    int end_index = (start_index + chunk_size) <= message_size
                        ? (start_index + chunk_size)
                        : message_size;
    bool is_last_write_request = (end_index == message_size);
    write_request = BuildWriteRequest(
        kFirstByteZeroVector,
        std::vector<uint8_t>(serialized_msg.begin() + start_index,
                             serialized_msg.begin() + end_index),
        is_last_write_request);
    WriteRemoteCharacteristic(write_request);
    start_index = end_index;
  }
}

// Changes in the GATT connection with the remote device should be observed
// here. If the GATT connection is dropped, we should call Disconnect() anyway,
// so the object can notify its observers.
void BluetoothLowEnergyConnection::DeviceChanged(BluetoothAdapter* adapter,
                                                 BluetoothDevice* device) {
  DCHECK(device);
  if (sub_status() == SubStatus::DISCONNECTED ||
      device->GetAddress() != GetDeviceAddress())
    return;

  if (sub_status() != SubStatus::WAITING_GATT_CONNECTION &&
      !device->IsConnected()) {
    PA_LOG(INFO) << "GATT connection dropped " << GetDeviceAddress()
                 << "\ndevice connected: " << device->IsConnected()
                 << "\ngatt connection: "
                 << (gatt_connection_ ? gatt_connection_->IsConnected()
                                      : false);
    Disconnect();
  }
}

void BluetoothLowEnergyConnection::DeviceRemoved(BluetoothAdapter* adapter,
                                                 BluetoothDevice* device) {
  DCHECK(device);
  if (sub_status_ == SubStatus::DISCONNECTED ||
      device->GetAddress() != GetDeviceAddress())
    return;

  PA_LOG(INFO) << "Device removed " << GetDeviceAddress();
  Disconnect();
}

void BluetoothLowEnergyConnection::GattCharacteristicValueChanged(
    BluetoothAdapter* adapter,
    BluetoothRemoteGattCharacteristic* characteristic,
    const std::vector<uint8_t>& value) {
  DCHECK_EQ(adapter, adapter_.get());
  if (sub_status() != SubStatus::WAITING_RESPONSE_SIGNAL &&
      sub_status() != SubStatus::CONNECTED)
    return;

  PA_LOG(INFO) << "Characteristic value changed: "
               << characteristic->GetUUID().canonical_value();

  if (characteristic->GetIdentifier() == from_peripheral_char_.id) {
    if (receiving_bytes_) {
      // Ignoring the first byte, as it contains a deprecated signal.
      const std::string bytes(value.begin() + 1, value.end());
      incoming_bytes_buffer_.append(bytes);
      if (incoming_bytes_buffer_.size() >= expected_number_of_incoming_bytes_) {
        OnBytesReceived(incoming_bytes_buffer_);
        receiving_bytes_ = false;
      }
      return;
    }

    if (value.size() < 4) {
      PA_LOG(WARNING) << "Incoming data corrupted, no signal found.";
      return;
    }

    const ControlSignal signal = static_cast<ControlSignal>(ToUint32(value));
    switch (signal) {
      case ControlSignal::kInvitationResponseSignal:
        if (sub_status() == SubStatus::WAITING_RESPONSE_SIGNAL)
          CompleteConnection();
        break;
      case ControlSignal::kInviteToConnectSignal:
        break;
      case ControlSignal::kSendSignal: {
        if (value.size() < 8) {
          PA_LOG(WARNING)
              << "Incoming data corrupted, expected message size not found.";
          return;
        }
        std::vector<uint8_t> size(value.begin() + 4, value.begin() + 8);
        expected_number_of_incoming_bytes_ =
            static_cast<size_t>(ToUint32(size));
        receiving_bytes_ = true;
        incoming_bytes_buffer_.clear();

        const std::string bytes(value.begin() + 8, value.end());
        incoming_bytes_buffer_.append(bytes);
        if (incoming_bytes_buffer_.size() >=
            expected_number_of_incoming_bytes_) {
          OnBytesReceived(incoming_bytes_buffer_);
          receiving_bytes_ = false;
        }
        break;
      }
      case ControlSignal::kDisconnectSignal:
        PA_LOG(INFO) << "Disconnect signal received.";
        Disconnect();
        break;
    }
  }
}

BluetoothLowEnergyConnection::WriteRequest::WriteRequest(
    const std::vector<uint8_t>& val,
    bool flag)
    : value(val),
      is_last_write_for_wire_message(flag),
      number_of_failed_attempts(0) {}

BluetoothLowEnergyConnection::WriteRequest::WriteRequest(
    const WriteRequest& other) = default;

BluetoothLowEnergyConnection::WriteRequest::~WriteRequest() {}

void BluetoothLowEnergyConnection::CompleteConnection() {
  PA_LOG(INFO) << "Connection completed. Time elapsed: "
               << base::TimeTicks::Now() - start_time_;
  SetSubStatus(SubStatus::CONNECTED);
}

void BluetoothLowEnergyConnection::OnCreateGattConnectionError(
    device::BluetoothDevice::ConnectErrorCode error_code) {
  DCHECK(sub_status_ == SubStatus::WAITING_GATT_CONNECTION);
  PA_LOG(WARNING) << "Error creating GATT connection to "
                  << remote_device().bluetooth_address
                  << "error code: " << error_code;
  Disconnect();
}

void BluetoothLowEnergyConnection::OnGattConnectionCreated(
    std::unique_ptr<device::BluetoothGattConnection> gatt_connection) {
  DCHECK(sub_status() == SubStatus::WAITING_GATT_CONNECTION);
  PA_LOG(INFO) << "GATT connection with " << gatt_connection->GetDeviceAddress()
               << " created.";
  PrintTimeElapsed();

  // Informing |bluetooth_trottler_| a new connection was established.
  bluetooth_throttler_->OnConnection(this);

  gatt_connection_ = std::move(gatt_connection);
  SetSubStatus(SubStatus::WAITING_CHARACTERISTICS);
  characteristic_finder_.reset(CreateCharacteristicsFinder(
      base::Bind(&BluetoothLowEnergyConnection::OnCharacteristicsFound,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&BluetoothLowEnergyConnection::OnCharacteristicsFinderError,
                 weak_ptr_factory_.GetWeakPtr())));
}

cryptauth::BluetoothLowEnergyCharacteristicsFinder*
BluetoothLowEnergyConnection::CreateCharacteristicsFinder(
    const cryptauth::BluetoothLowEnergyCharacteristicsFinder::SuccessCallback&
        success_callback,
    const cryptauth::BluetoothLowEnergyCharacteristicsFinder::ErrorCallback&
        error_callback) {
  return new cryptauth::BluetoothLowEnergyCharacteristicsFinder(
      adapter_, GetRemoteDevice(), remote_service_, to_peripheral_char_,
      from_peripheral_char_, success_callback, error_callback);
}

void BluetoothLowEnergyConnection::OnCharacteristicsFound(
    const cryptauth::RemoteAttribute& service,
    const cryptauth::RemoteAttribute& to_peripheral_char,
    const cryptauth::RemoteAttribute& from_peripheral_char) {
  PA_LOG(INFO) << "Remote chacteristics found.";
  PrintTimeElapsed();

  DCHECK(sub_status() == SubStatus::WAITING_CHARACTERISTICS);
  remote_service_ = service;
  to_peripheral_char_ = to_peripheral_char;
  from_peripheral_char_ = from_peripheral_char;

  SetSubStatus(SubStatus::CHARACTERISTICS_FOUND);
  StartNotifySession();
}

void BluetoothLowEnergyConnection::OnCharacteristicsFinderError(
    const cryptauth::RemoteAttribute& to_peripheral_char,
    const cryptauth::RemoteAttribute& from_peripheral_char) {
  DCHECK(sub_status() == SubStatus::WAITING_CHARACTERISTICS);
  PA_LOG(WARNING) << "Connection error, missing characteristics for SmartLock "
                     "service.\n"
                  << (to_peripheral_char.id.empty()
                          ? to_peripheral_char.uuid.canonical_value()
                          : "")
                  << (from_peripheral_char.id.empty()
                          ? ", " + from_peripheral_char.uuid.canonical_value()
                          : "") << " not found.";

  Disconnect();
}

void BluetoothLowEnergyConnection::StartNotifySession() {
  if (sub_status() == SubStatus::CHARACTERISTICS_FOUND) {
    BluetoothRemoteGattCharacteristic* characteristic =
        GetGattCharacteristic(from_peripheral_char_.id);
    DCHECK(characteristic);

    // This is a workaround for crbug.com/507325. If |characteristic| is already
    // notifying |characteristic->StartNotifySession()| will fail with
    // GATT_ERROR_FAILED.
    if (characteristic->IsNotifying()) {
      PA_LOG(INFO) << characteristic->GetUUID().canonical_value()
                   << " already notifying.";
      SetSubStatus(SubStatus::NOTIFY_SESSION_READY);
      SendInviteToConnectSignal();
      return;
    }

    SetSubStatus(SubStatus::WAITING_NOTIFY_SESSION);
    characteristic->StartNotifySession(
        base::Bind(&BluetoothLowEnergyConnection::OnNotifySessionStarted,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&BluetoothLowEnergyConnection::OnNotifySessionError,
                   weak_ptr_factory_.GetWeakPtr()));
  }
}

void BluetoothLowEnergyConnection::OnNotifySessionError(
    BluetoothRemoteGattService::GattErrorCode error) {
  DCHECK(sub_status() == SubStatus::WAITING_NOTIFY_SESSION);
  PA_LOG(WARNING) << "Error starting notification session: " << error;
  Disconnect();
}

void BluetoothLowEnergyConnection::OnNotifySessionStarted(
    std::unique_ptr<BluetoothGattNotifySession> notify_session) {
  DCHECK(sub_status() == SubStatus::WAITING_NOTIFY_SESSION);
  PA_LOG(INFO) << "Notification session started "
               << notify_session->GetCharacteristicIdentifier();
  PrintTimeElapsed();

  SetSubStatus(SubStatus::NOTIFY_SESSION_READY);
  notify_session_ = std::move(notify_session);

  SendInviteToConnectSignal();
}

void BluetoothLowEnergyConnection::StopNotifySession() {
  if (notify_session_) {
    notify_session_->Stop(base::Bind(&base::DoNothing));
    notify_session_.reset();
  }
}

void BluetoothLowEnergyConnection::SendInviteToConnectSignal() {
  if (sub_status() == SubStatus::NOTIFY_SESSION_READY) {
    PA_LOG(INFO) << "Sending invite to connect signal";
    SetSubStatus(SubStatus::WAITING_RESPONSE_SIGNAL);

    WriteRequest write_request = BuildWriteRequest(
        ToByteVector(
            static_cast<uint32_t>(ControlSignal::kInviteToConnectSignal)),
        std::vector<uint8_t>(), false);

    WriteRemoteCharacteristic(write_request);
  }
}

void BluetoothLowEnergyConnection::WriteRemoteCharacteristic(
    WriteRequest request) {
  write_requests_queue_.push(request);
  ProcessNextWriteRequest();
}

void BluetoothLowEnergyConnection::ProcessNextWriteRequest() {
  BluetoothRemoteGattCharacteristic* characteristic =
      GetGattCharacteristic(to_peripheral_char_.id);
  if (!write_requests_queue_.empty() && !write_remote_characteristic_pending_ &&
      characteristic) {
    write_remote_characteristic_pending_ = true;
    WriteRequest next_request = write_requests_queue_.front();
    PA_LOG(INFO) << "Writing characteristic...";
    characteristic->WriteRemoteCharacteristic(
        next_request.value,
        base::Bind(&BluetoothLowEnergyConnection::OnRemoteCharacteristicWritten,
                   weak_ptr_factory_.GetWeakPtr(),
                   next_request.is_last_write_for_wire_message),
        base::Bind(
            &BluetoothLowEnergyConnection::OnWriteRemoteCharacteristicError,
            weak_ptr_factory_.GetWeakPtr(),
            next_request.is_last_write_for_wire_message));
  }
}

void BluetoothLowEnergyConnection::OnRemoteCharacteristicWritten(
    bool run_did_send_message_callback) {
  PA_LOG(INFO) << "Characteristic written.";
  write_remote_characteristic_pending_ = false;
  // TODO(sacomoto): Actually pass the current message to the observer.
  if (run_did_send_message_callback)
    OnDidSendMessage(cryptauth::WireMessage(std::string(), std::string()),
                     true);

  // Removes the top of queue (already processed) and process the next request.
  DCHECK(!write_requests_queue_.empty());
  write_requests_queue_.pop();
  ProcessNextWriteRequest();
}

void BluetoothLowEnergyConnection::OnWriteRemoteCharacteristicError(
    bool run_did_send_message_callback,
    BluetoothRemoteGattService::GattErrorCode error) {
  PA_LOG(WARNING) << "Error " << error << " writing characteristic: "
                  << to_peripheral_char_.uuid.canonical_value();
  write_remote_characteristic_pending_ = false;
  // TODO(sacomoto): Actually pass the current message to the observer.
  if (run_did_send_message_callback)
    OnDidSendMessage(cryptauth::WireMessage(std::string(), std::string()),
                     false);

  // Increases the number of failed attempts and retry.
  DCHECK(!write_requests_queue_.empty());
  if (++write_requests_queue_.front().number_of_failed_attempts >=
      max_number_of_write_attempts_) {
    Disconnect();
    return;
  }
  ProcessNextWriteRequest();
}

BluetoothLowEnergyConnection::WriteRequest
BluetoothLowEnergyConnection::BuildWriteRequest(
    const std::vector<uint8_t>& signal,
    const std::vector<uint8_t>& bytes,
    bool is_last_write_for_wire_message) {
  std::vector<uint8_t> value(signal.begin(), signal.end());
  value.insert(value.end(), bytes.begin(), bytes.end());
  return WriteRequest(value, is_last_write_for_wire_message);
}

void BluetoothLowEnergyConnection::PrintTimeElapsed() {
  PA_LOG(INFO) << "Time elapsed: " << base::TimeTicks::Now() - start_time_;
}

std::string BluetoothLowEnergyConnection::GetDeviceAddress() {
  // When the remote device is connected we should rely on the address given by
  // |gatt_connection_|. As the device address may change if the device is
  // paired. The address in |gatt_connection_| is automatically updated in this
  // case.
  return gatt_connection_ ? gatt_connection_->GetDeviceAddress()
                          : remote_device().bluetooth_address;
}

BluetoothDevice* BluetoothLowEnergyConnection::GetRemoteDevice() {
  // It's not possible to simply use
  // |adapter_->GetDevice(GetDeviceAddress())| to find the device with MAC
  // address |GetDeviceAddress()|. For paired devices,
  // BluetoothAdapter::GetDevice(XXX) searches for the temporary MAC address
  // XXX, whereas |GetDeviceAddress()| is the real MAC address. This is a
  // bug in the way device::BluetoothAdapter is storing the devices (see
  // crbug.com/497841).
  std::vector<BluetoothDevice*> devices = adapter_->GetDevices();
  for (auto* device : devices) {
    if (device->GetAddress() == GetDeviceAddress())
      return device;
  }

  return nullptr;
}

BluetoothRemoteGattService* BluetoothLowEnergyConnection::GetRemoteService() {
  BluetoothDevice* remote_device = GetRemoteDevice();
  if (!remote_device) {
    PA_LOG(WARNING) << "Remote device not found.";
    return NULL;
  }
  if (remote_service_.id.empty()) {
    std::vector<BluetoothRemoteGattService*> services =
        remote_device->GetGattServices();
    for (const auto* service : services)
      if (service->GetUUID() == remote_service_.uuid) {
        remote_service_.id = service->GetIdentifier();
        break;
      }
  }
  return remote_device->GetGattService(remote_service_.id);
}

BluetoothRemoteGattCharacteristic*
BluetoothLowEnergyConnection::GetGattCharacteristic(
    const std::string& gatt_characteristic) {
  BluetoothRemoteGattService* remote_service = GetRemoteService();
  if (!remote_service) {
    PA_LOG(WARNING) << "Remote service not found.";
    return NULL;
  }
  return remote_service->GetCharacteristic(gatt_characteristic);
}

// TODO(sacomoto): make this robust to byte ordering in both sides of the
// SmartLock BLE socket.
uint32_t BluetoothLowEnergyConnection::ToUint32(
    const std::vector<uint8_t>& bytes) {
  return bytes[0] | (bytes[1] << 8) | (bytes[2] << 16) | (bytes[3] << 24);
}

// TODO(sacomoto): make this robust to byte ordering in both sides of the
// SmartLock BLE socket.
const std::vector<uint8_t> BluetoothLowEnergyConnection::ToByteVector(
    const uint32_t value) {
  std::vector<uint8_t> bytes(4, 0);
  bytes[0] = static_cast<uint8_t>(value);
  bytes[1] = static_cast<uint8_t>(value >> 8);
  bytes[2] = static_cast<uint8_t>(value >> 16);
  bytes[3] = static_cast<uint8_t>(value >> 24);
  return bytes;
}

}  // namespace proximity_auth
