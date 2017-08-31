// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cryptauth/ble/bluetooth_low_energy_weave_client_connection.h"

#include <sstream>
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "base/task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/cryptauth/connection_finder.h"
#include "components/cryptauth/wire_message.h"
#include "components/proximity_auth/logging/logging.h"
#include "device/bluetooth/bluetooth_gatt_connection.h"

namespace cryptauth {

namespace weave {

namespace {

typedef BluetoothLowEnergyWeavePacketReceiver::State ReceiverState;

// The UUID of the TX characteristic used to transmit data to the server.
const char kTXCharacteristicUUID[] = "00000100-0004-1000-8000-001A11000101";

// The UUID of the RX characteristic used to receive data from the server.
const char kRXCharacteristicUUID[] = "00000100-0004-1000-8000-001A11000102";

// If sending a message fails, retry up to 2 additional times. This means that
// each message gets 3 attempts: the first one, and 2 retries.
const int kMaxNumberOfRetryAttempts = 2;

// The time to wait in seconds for the server to send its connection response.
// If not received within this time, the connection will fail.
const int kConnectionResponseTimeoutSeconds = 2;

}  // namespace

// static
BluetoothLowEnergyWeaveClientConnection::Factory*
    BluetoothLowEnergyWeaveClientConnection::Factory::factory_instance_ =
        nullptr;

// static
std::unique_ptr<Connection>
BluetoothLowEnergyWeaveClientConnection::Factory::NewInstance(
    const RemoteDevice& remote_device,
    const std::string& device_address,
    scoped_refptr<device::BluetoothAdapter> adapter,
    const device::BluetoothUUID remote_service_uuid) {
  if (!factory_instance_) {
    factory_instance_ = new Factory();
  }
  return factory_instance_->BuildInstance(remote_device, device_address,
                                          adapter, remote_service_uuid);
}

// static
void BluetoothLowEnergyWeaveClientConnection::Factory::SetInstanceForTesting(
    Factory* factory) {
  factory_instance_ = factory;
}

std::unique_ptr<Connection>
BluetoothLowEnergyWeaveClientConnection::Factory::BuildInstance(
    const RemoteDevice& remote_device,
    const std::string& device_address,
    scoped_refptr<device::BluetoothAdapter> adapter,
    const device::BluetoothUUID remote_service_uuid) {
  return base::MakeUnique<BluetoothLowEnergyWeaveClientConnection>(
      remote_device, device_address, adapter, remote_service_uuid);
}

BluetoothLowEnergyWeaveClientConnection::TimerFactory::~TimerFactory() {}

std::unique_ptr<base::Timer>
BluetoothLowEnergyWeaveClientConnection::TimerFactory::CreateTimer() {
  return base::MakeUnique<base::OneShotTimer>();
}

BluetoothLowEnergyWeaveClientConnection::
    BluetoothLowEnergyWeaveClientConnection(
        const RemoteDevice& device,
        const std::string& device_address,
        scoped_refptr<device::BluetoothAdapter> adapter,
        const device::BluetoothUUID remote_service_uuid)
    : Connection(device),
      device_address_(device_address),
      adapter_(adapter),
      remote_service_({remote_service_uuid, std::string()}),
      packet_generator_(
          base::MakeUnique<BluetoothLowEnergyWeavePacketGenerator>()),
      packet_receiver_(base::MakeUnique<BluetoothLowEnergyWeavePacketReceiver>(
          BluetoothLowEnergyWeavePacketReceiver::ReceiverType::CLIENT)),
      tx_characteristic_(
          {device::BluetoothUUID(kTXCharacteristicUUID), std::string()}),
      rx_characteristic_(
          {device::BluetoothUUID(kRXCharacteristicUUID), std::string()}),
      timer_factory_(base::MakeUnique<TimerFactory>()),
      task_runner_(base::ThreadTaskRunnerHandle::Get()),
      sub_status_(SubStatus::DISCONNECTED),
      weak_ptr_factory_(this) {
  adapter_->AddObserver(this);
}

BluetoothLowEnergyWeaveClientConnection::
    ~BluetoothLowEnergyWeaveClientConnection() {
  DestroyConnection();
}

void BluetoothLowEnergyWeaveClientConnection::Connect() {
  DCHECK(sub_status() == SubStatus::DISCONNECTED);
  SetSubStatus(SubStatus::WAITING_CONNECTION_LATENCY);

  device::BluetoothDevice* bluetooth_device = GetBluetoothDevice();
  if (!bluetooth_device) {
    PA_LOG(WARNING) << "Device not found; cannot set connection latency for "
                    << GetDeviceInfoLogString() << ".";
    DestroyConnection();
    return;
  }

  PA_LOG(INFO) << "Setting connection latency for " << GetDeviceInfoLogString()
               << ".";
  bluetooth_device->SetConnectionLatency(
      device::BluetoothDevice::ConnectionLatency::CONNECTION_LATENCY_LOW,
      base::Bind(&BluetoothLowEnergyWeaveClientConnection::CreateGattConnection,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(
          &BluetoothLowEnergyWeaveClientConnection::OnSetConnectionLatencyError,
          weak_ptr_factory_.GetWeakPtr()));
}

void BluetoothLowEnergyWeaveClientConnection::CreateGattConnection() {
  DCHECK(sub_status() == SubStatus::WAITING_CONNECTION_LATENCY);
  SetSubStatus(SubStatus::WAITING_GATT_CONNECTION);

  device::BluetoothDevice* bluetooth_device = GetBluetoothDevice();
  if (!bluetooth_device) {
    PA_LOG(WARNING) << "Device not found; cannot create GATT connection to "
                    << GetDeviceInfoLogString() << ".";
    DestroyConnection();
    return;
  }

  PA_LOG(INFO) << "Creating GATT connection with " << GetDeviceInfoLogString()
               << ".";
  bluetooth_device->CreateGattConnection(
      base::Bind(
          &BluetoothLowEnergyWeaveClientConnection::OnGattConnectionCreated,
          weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&BluetoothLowEnergyWeaveClientConnection::
                     OnCreateGattConnectionError,
                 weak_ptr_factory_.GetWeakPtr()));
}

void BluetoothLowEnergyWeaveClientConnection::Disconnect() {
  if (sub_status_ == SubStatus::CONNECTED) {
    PA_LOG(INFO) << "Disconnection requested; sending \"connection close\" "
                 << "uWeave packet to " << GetDeviceInfoLogString() << ".";

    // Send a "connection close" uWeave packet. After the send has completed,
    // the connection will disconnect automatically.
    ClearQueueAndSendConnectionClose();
    return;
  }

  DestroyConnection();
}

void BluetoothLowEnergyWeaveClientConnection::DestroyConnection() {
  if (adapter_) {
    adapter_->RemoveObserver(this);
    adapter_ = nullptr;
  }

  weak_ptr_factory_.InvalidateWeakPtrs();
  notify_session_.reset();
  characteristic_finder_.reset();

  if (gatt_connection_) {
    PA_LOG(INFO) << "Disconnecting from " << GetDeviceInfoLogString();
    gatt_connection_.reset();
  }

  SetSubStatus(SubStatus::DISCONNECTED);
}

void BluetoothLowEnergyWeaveClientConnection::SetSubStatus(
    SubStatus new_sub_status) {
  sub_status_ = new_sub_status;

  // Sets the status of base class Connection.
  if (new_sub_status == SubStatus::CONNECTED)
    SetStatus(Status::CONNECTED);
  else if (new_sub_status == SubStatus::DISCONNECTED)
    SetStatus(Status::DISCONNECTED);
  else
    SetStatus(Status::IN_PROGRESS);
}

void BluetoothLowEnergyWeaveClientConnection::SetupTestDoubles(
    scoped_refptr<base::TaskRunner> test_task_runner,
    std::unique_ptr<TimerFactory> test_timer_factory,
    std::unique_ptr<BluetoothLowEnergyWeavePacketGenerator> test_generator,
    std::unique_ptr<BluetoothLowEnergyWeavePacketReceiver> test_receiver) {
  task_runner_ = test_task_runner;
  timer_factory_ = std::move(test_timer_factory);
  packet_generator_ = std::move(test_generator);
  packet_receiver_ = std::move(test_receiver);
}

void BluetoothLowEnergyWeaveClientConnection::SendMessageImpl(
    std::unique_ptr<WireMessage> message) {
  DCHECK(sub_status() == SubStatus::CONNECTED);

  // Split |message| up into multiple packets which can be sent as one uWeave
  // message.
  std::vector<Packet> weave_packets =
      packet_generator_->EncodeDataMessage(message->Serialize());

  // For each packet, create a WriteRequest and add it to the queue.
  for (uint32_t i = 0; i < weave_packets.size(); ++i) {
    WriteRequestType request_type = (i != weave_packets.size() - 1)
                                        ? WriteRequestType::REGULAR
                                        : WriteRequestType::MESSAGE_COMPLETE;
    queued_write_requests_.emplace(base::MakeUnique<WriteRequest>(
        weave_packets[i], request_type, message.get()));
  }

  // Add |message| to the queue of WireMessages.
  queued_wire_messages_.emplace(std::move(message));

  ProcessNextWriteRequest();
}

void BluetoothLowEnergyWeaveClientConnection::DeviceChanged(
    device::BluetoothAdapter* adapter,
    device::BluetoothDevice* device) {
  // Ignore updates about other devices.
  if (device->GetAddress() != GetDeviceAddress())
    return;

  if (sub_status() == SubStatus::DISCONNECTED ||
      sub_status() == SubStatus::WAITING_CONNECTION_LATENCY ||
      sub_status() == SubStatus::WAITING_GATT_CONNECTION) {
    // Ignore status change events if a connection has not yet occurred.
    return;
  }

  // If a connection has already occurred and |device| is still connected, there
  // is nothing to do.
  if (device->IsConnected())
    return;

  PA_LOG(WARNING) << "GATT connection to " << GetDeviceInfoLogString()
                  << " has been dropped.";
  DestroyConnection();
}

void BluetoothLowEnergyWeaveClientConnection::DeviceRemoved(
    device::BluetoothAdapter* adapter,
    device::BluetoothDevice* device) {
  // Ignore updates about other devices.
  if (device->GetAddress() != GetDeviceAddress())
    return;

  PA_LOG(WARNING) << "Device has been lost: " << GetDeviceInfoLogString()
                  << ".";
  DestroyConnection();
}

void BluetoothLowEnergyWeaveClientConnection::GattCharacteristicValueChanged(
    device::BluetoothAdapter* adapter,
    device::BluetoothRemoteGattCharacteristic* characteristic,
    const Packet& value) {
  DCHECK_EQ(adapter, adapter_.get());

  // Ignore characteristics which do not apply to this connection.
  if (characteristic->GetIdentifier() != rx_characteristic_.id)
    return;

  if (sub_status() != SubStatus::WAITING_CONNECTION_RESPONSE &&
      sub_status() != SubStatus::CONNECTED) {
    PA_LOG(WARNING) << "Received message from " << GetDeviceInfoLogString()
                    << ", but was not expecting one. sub_status() = "
                    << sub_status();
    return;
  }

  switch (packet_receiver_->ReceivePacket(value)) {
    case ReceiverState::DATA_READY:
      OnBytesReceived(packet_receiver_->GetDataMessage());
      break;
    case ReceiverState::CONNECTION_CLOSED:
      PA_LOG(INFO) << "Received \"connection close\" uWeave packet from "
                   << GetDeviceInfoLogString()
                   << ". Reason: " << GetReasonForClose() << ".";
      DestroyConnection();
      return;
    case ReceiverState::ERROR_DETECTED:
      PA_LOG(ERROR) << "Received invalid packet from "
                    << GetDeviceInfoLogString() << ". ";
      ClearQueueAndSendConnectionClose();
      break;
    case ReceiverState::WAITING:
      CompleteConnection();
      break;
    case ReceiverState::RECEIVING_DATA:
      // Continue to wait for more packets to arrive; once the rest of the
      // packets for this message are received, |packet_receiver_| will
      // transition to the DATA_READY state.
      break;
    default:
      NOTREACHED();
  }
}

void BluetoothLowEnergyWeaveClientConnection::CompleteConnection() {
  DCHECK(sub_status() == SubStatus::WAITING_CONNECTION_RESPONSE);
  SetSubStatus(SubStatus::CONNECTED);
  connection_response_timer_->Stop();

  uint16_t max_packet_size = packet_receiver_->GetMaxPacketSize();
  PA_LOG(INFO) << "Received uWeave \"connection response\" packet; connection "
               << "is now fully initialized for " << GetDeviceInfoLogString()
               << ". Max packet size: " << max_packet_size;

  // Now that the "connection close" uWeave packet has been received,
  // |packet_receiver_| should have received a max packet size from the GATT
  // server.
  packet_generator_->SetMaxPacketSize(max_packet_size);
}

void BluetoothLowEnergyWeaveClientConnection::OnSetConnectionLatencyError() {
  DCHECK(sub_status_ == SubStatus::WAITING_CONNECTION_LATENCY);
  PA_LOG(WARNING) << "Error setting connection latency for connection to "
                  << GetDeviceInfoLogString() << ".";

  // Even if setting the connection latency fails, continue with the
  // connection. This is unfortunate but should not be considered a fatal error.
  CreateGattConnection();
}

void BluetoothLowEnergyWeaveClientConnection::OnCreateGattConnectionError(
    device::BluetoothDevice::ConnectErrorCode error_code) {
  DCHECK(sub_status_ == SubStatus::WAITING_GATT_CONNECTION);
  PA_LOG(WARNING) << "Error creating GATT connection to "
                  << GetDeviceInfoLogString() << ". Error code: " << error_code;
  DestroyConnection();
}

void BluetoothLowEnergyWeaveClientConnection::OnGattConnectionCreated(
    std::unique_ptr<device::BluetoothGattConnection> gatt_connection) {
  DCHECK(sub_status() == SubStatus::WAITING_GATT_CONNECTION);

  gatt_connection_ = std::move(gatt_connection);
  SetSubStatus(SubStatus::WAITING_CHARACTERISTICS);

  PA_LOG(INFO) << "Finding GATT characteristics for "
               << GetDeviceInfoLogString() << ".";
  characteristic_finder_.reset(CreateCharacteristicsFinder(
      base::Bind(
          &BluetoothLowEnergyWeaveClientConnection::OnCharacteristicsFound,
          weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&BluetoothLowEnergyWeaveClientConnection::
                     OnCharacteristicsFinderError,
                 weak_ptr_factory_.GetWeakPtr())));
}

BluetoothLowEnergyCharacteristicsFinder*
BluetoothLowEnergyWeaveClientConnection::CreateCharacteristicsFinder(
    const BluetoothLowEnergyCharacteristicsFinder::SuccessCallback&
        success_callback,
    const BluetoothLowEnergyCharacteristicsFinder::ErrorCallback&
        error_callback) {
  return new BluetoothLowEnergyCharacteristicsFinder(
      adapter_, GetBluetoothDevice(), remote_service_, tx_characteristic_,
      rx_characteristic_, success_callback, error_callback);
}

void BluetoothLowEnergyWeaveClientConnection::OnCharacteristicsFound(
    const RemoteAttribute& service,
    const RemoteAttribute& tx_characteristic,
    const RemoteAttribute& rx_characteristic) {
  DCHECK(sub_status() == SubStatus::WAITING_CHARACTERISTICS);

  remote_service_ = service;
  tx_characteristic_ = tx_characteristic;
  rx_characteristic_ = rx_characteristic;
  characteristic_finder_.reset();

  SetSubStatus(SubStatus::CHARACTERISTICS_FOUND);

  StartNotifySession();
}

void BluetoothLowEnergyWeaveClientConnection::OnCharacteristicsFinderError(
    const RemoteAttribute& tx_characteristic,
    const RemoteAttribute& rx_characteristic) {
  DCHECK(sub_status() == SubStatus::WAITING_CHARACTERISTICS);

  std::stringstream ss;
  ss << "Could not find GATT characteristics for " << GetDeviceInfoLogString()
     << ": ";
  if (tx_characteristic.id.empty()) {
    ss << "[TX: " << tx_characteristic.uuid.canonical_value() << "]";
    if (!rx_characteristic.id.empty())
      ss << ", ";
  }
  if (rx_characteristic.id.empty())
    ss << "[RX: " << rx_characteristic.uuid.canonical_value() << "]";
  PA_LOG(ERROR) << ss.str();

  characteristic_finder_.reset();

  DestroyConnection();
}

void BluetoothLowEnergyWeaveClientConnection::StartNotifySession() {
  DCHECK(sub_status() == SubStatus::CHARACTERISTICS_FOUND);

  device::BluetoothRemoteGattCharacteristic* characteristic =
      GetGattCharacteristic(rx_characteristic_.id);
  if (!characteristic) {
    PA_LOG(ERROR) << "Characteristic no longer available after it was found. "
                  << "Cannot start notification session for "
                  << GetDeviceInfoLogString() << ".";
    DestroyConnection();
    return;
  }

  // Workaround for crbug.com/507325. If |characteristic| is already notifying,
  // characteristic->StartNotifySession() fails with GATT_ERROR_FAILED.
  if (characteristic->IsNotifying()) {
    SetSubStatus(SubStatus::NOTIFY_SESSION_READY);
    SendConnectionRequest();
    return;
  }

  SetSubStatus(SubStatus::WAITING_NOTIFY_SESSION);
  PA_LOG(INFO) << "Starting notification session for "
               << GetDeviceInfoLogString() << ".";
  characteristic->StartNotifySession(
      base::Bind(
          &BluetoothLowEnergyWeaveClientConnection::OnNotifySessionStarted,
          weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&BluetoothLowEnergyWeaveClientConnection::OnNotifySessionError,
                 weak_ptr_factory_.GetWeakPtr()));
}

void BluetoothLowEnergyWeaveClientConnection::OnNotifySessionStarted(
    std::unique_ptr<device::BluetoothGattNotifySession> notify_session) {
  DCHECK(sub_status() == SubStatus::WAITING_NOTIFY_SESSION);
  notify_session_ = std::move(notify_session);
  SetSubStatus(SubStatus::NOTIFY_SESSION_READY);
  SendConnectionRequest();
}

void BluetoothLowEnergyWeaveClientConnection::OnNotifySessionError(
    device::BluetoothRemoteGattService::GattErrorCode error) {
  DCHECK(sub_status() == SubStatus::WAITING_NOTIFY_SESSION);
  PA_LOG(ERROR) << "Cannot start notification session for "
                << GetDeviceInfoLogString() << ". Error: " << error << ".";
  DestroyConnection();
}

void BluetoothLowEnergyWeaveClientConnection::SendConnectionRequest() {
  DCHECK(sub_status() == SubStatus::NOTIFY_SESSION_READY);
  SetSubStatus(SubStatus::WAITING_CONNECTION_RESPONSE);

  PA_LOG(INFO) << "Sending \"connection request\" uWeave packet to "
               << GetDeviceInfoLogString();
  connection_response_timer_ = timer_factory_->CreateTimer();
  connection_response_timer_->Start(
      FROM_HERE,
      base::TimeDelta::FromSeconds(kConnectionResponseTimeoutSeconds),
      base::Bind(
          &BluetoothLowEnergyWeaveClientConnection::OnConnectionResponseTimeout,
          weak_ptr_factory_.GetWeakPtr()));

  queued_write_requests_.emplace(base::MakeUnique<WriteRequest>(
      packet_generator_->CreateConnectionRequest(),
      WriteRequestType::CONNECTION_REQUEST));
  ProcessNextWriteRequest();
}

void BluetoothLowEnergyWeaveClientConnection::ProcessNextWriteRequest() {
  // If there is already an in-progress write or there are no pending
  // WriteRequests, there is nothing to do.
  if (pending_write_request_ || queued_write_requests_.empty())
    return;

  pending_write_request_ = std::move(queued_write_requests_.front());
  queued_write_requests_.pop();

  PA_LOG(INFO) << "Writing " << pending_write_request_->value.size() << " "
               << "bytes to " << GetDeviceInfoLogString() << ".";
  SendPendingWriteRequest();
}

void BluetoothLowEnergyWeaveClientConnection::SendPendingWriteRequest() {
  DCHECK(pending_write_request_);

  device::BluetoothRemoteGattCharacteristic* characteristic =
      GetGattCharacteristic(tx_characteristic_.id);
  if (!characteristic) {
    PA_LOG(ERROR) << "Characteristic no longer available after it was found. "
                  << "Cannot process write request for "
                  << GetDeviceInfoLogString() << ".";
    DestroyConnection();
    return;
  }

  characteristic->WriteRemoteCharacteristic(
      pending_write_request_->value,
      base::Bind(&BluetoothLowEnergyWeaveClientConnection::
                     OnRemoteCharacteristicWritten,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&BluetoothLowEnergyWeaveClientConnection::
                     OnWriteRemoteCharacteristicError,
                 weak_ptr_factory_.GetWeakPtr()));
}

void BluetoothLowEnergyWeaveClientConnection::OnRemoteCharacteristicWritten() {
  DCHECK(sub_status() == SubStatus::WAITING_CONNECTION_RESPONSE ||
         sub_status() == SubStatus::CONNECTED);

  if (!pending_write_request_) {
    PA_LOG(ERROR) << "OnRemoteCharacteristicWritten() called, but no pending "
                  << "WriteRequest. Stopping connection to "
                  << GetDeviceInfoLogString();
    DestroyConnection();
    return;
  }

  if (pending_write_request_->request_type ==
      WriteRequestType::CONNECTION_CLOSE) {
    // Once a "connection close" uWeave packet has been sent, the connection
    // is ready to be disconnected.
    DestroyConnection();
    return;
  }

  if (pending_write_request_->request_type ==
      WriteRequestType::MESSAGE_COMPLETE) {
    if (queued_wire_messages_.empty()) {
      PA_LOG(ERROR) << "Sent a WriteRequest with type == MESSAGE_COMPLETE, but "
                    << "there were no queued WireMessages. Cannot process "
                    << "completed write to " << GetDeviceInfoLogString();
      DestroyConnection();
      return;
    }

    std::unique_ptr<WireMessage> sent_message =
        std::move(queued_wire_messages_.front());
    queued_wire_messages_.pop();
    DCHECK_EQ(sent_message.get(),
              pending_write_request_->associated_wire_message);

    // Notify observers of the message being sent via a task on the run loop to
    // ensure that if an observer deletes this object in response to receiving
    // the OnSendCompleted() callback, a null pointer is not deferenced.
    task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&BluetoothLowEnergyWeaveClientConnection::OnDidSendMessage,
                   weak_ptr_factory_.GetWeakPtr(), *sent_message,
                   true /* success */));
  }

  pending_write_request_.reset();
  ProcessNextWriteRequest();
}

void BluetoothLowEnergyWeaveClientConnection::OnWriteRemoteCharacteristicError(
    device::BluetoothRemoteGattService::GattErrorCode error) {
  DCHECK(sub_status() == SubStatus::WAITING_CONNECTION_RESPONSE ||
         sub_status() == SubStatus::CONNECTED);

  if (!pending_write_request_) {
    PA_LOG(ERROR) << "OnWriteRemoteCharacteristicError() called, but no "
                  << "pending WriteRequest. Stopping connection to "
                  << GetDeviceInfoLogString();
    DestroyConnection();
    return;
  }

  ++pending_write_request_->number_of_failed_attempts;
  if (pending_write_request_->number_of_failed_attempts <
      kMaxNumberOfRetryAttempts + 1) {
    PA_LOG(WARNING) << "Error sending WriteRequest to "
                    << GetDeviceInfoLogString() << "; failure number "
                    << pending_write_request_->number_of_failed_attempts
                    << ". Retrying.";
    SendPendingWriteRequest();
    return;
  }

  if (pending_write_request_->request_type == WriteRequestType::REGULAR ||
      pending_write_request_->request_type ==
          WriteRequestType::MESSAGE_COMPLETE) {
    std::unique_ptr<WireMessage> failed_message =
        std::move(queued_wire_messages_.front());
    queued_wire_messages_.pop();
    DCHECK_EQ(failed_message.get(),
              pending_write_request_->associated_wire_message);

    OnDidSendMessage(*failed_message, false /* success */);
  }

  // Since the try limit has been hit, this is a fatal error. Destroy the
  // connection, but post it as a new task to ensure that observers have a
  // chance to process the OnSendCompleted() call.
  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&BluetoothLowEnergyWeaveClientConnection::DestroyConnection,
                 weak_ptr_factory_.GetWeakPtr()));
}

void BluetoothLowEnergyWeaveClientConnection::OnDidSendMessage(
    const WireMessage& message,
    bool success) {
  Connection::OnDidSendMessage(message, success);
}

void BluetoothLowEnergyWeaveClientConnection::
    ClearQueueAndSendConnectionClose() {
  DCHECK(sub_status() == SubStatus::WAITING_CONNECTION_RESPONSE ||
         sub_status() == SubStatus::CONNECTED);

  // The connection is now in an invalid state. Clear queued writes.
  while (!queued_write_requests_.empty())
    queued_write_requests_.pop();

  // Now, queue up a "connection close" uWeave packet. If there was a pending
  // write, we must wait for it to complete before the "connection close" can
  // be sent.
  queued_write_requests_.emplace(
      base::MakeUnique<WriteRequest>(packet_generator_->CreateConnectionClose(
                                         packet_receiver_->GetReasonToClose()),
                                     WriteRequestType::CONNECTION_CLOSE));

  if (pending_write_request_) {
    PA_LOG(WARNING) << "Waiting for current write to complete, then will send "
                    << "a \"connection close\" uWeave packet.";
  } else {
    PA_LOG(INFO) << "Sending a \"connection close\" uWeave packet.";
  }

  ProcessNextWriteRequest();
}

void BluetoothLowEnergyWeaveClientConnection::OnConnectionResponseTimeout() {
  DCHECK(sub_status() == SubStatus::WAITING_CONNECTION_RESPONSE);
  PA_LOG(ERROR) << "Timed out waiting for \"connection response\" uWeave "
                << "packet from " << GetDeviceInfoLogString()
                << ". Destroying connection.";
  DestroyConnection();
}

std::string BluetoothLowEnergyWeaveClientConnection::GetDeviceAddress() {
  // When the remote device is connected, rely on the address given by
  // |gatt_connection_|. Unpaired BLE device addresses are ephemeral and are
  // expected to change periodically.
  return gatt_connection_ ? gatt_connection_->GetDeviceAddress()
                          : device_address_;
}

device::BluetoothDevice*
BluetoothLowEnergyWeaveClientConnection::GetBluetoothDevice() {
  return adapter_->GetDevice(GetDeviceAddress());
}

device::BluetoothRemoteGattService*
BluetoothLowEnergyWeaveClientConnection::GetRemoteService() {
  device::BluetoothDevice* bluetooth_device = GetBluetoothDevice();
  if (!bluetooth_device) {
    PA_LOG(WARNING) << "Cannot find Bluetooth device for "
                    << GetDeviceInfoLogString();
    return nullptr;
  }

  if (remote_service_.id.empty()) {
    for (auto* service : bluetooth_device->GetGattServices()) {
      if (service->GetUUID() == remote_service_.uuid) {
        remote_service_.id = service->GetIdentifier();
        break;
      }
    }
  }

  return bluetooth_device->GetGattService(remote_service_.id);
}

device::BluetoothRemoteGattCharacteristic*
BluetoothLowEnergyWeaveClientConnection::GetGattCharacteristic(
    const std::string& gatt_characteristic) {
  device::BluetoothRemoteGattService* remote_service = GetRemoteService();
  if (!remote_service) {
    PA_LOG(WARNING) << "Cannot find GATT service for "
                    << GetDeviceInfoLogString();
    return nullptr;
  }
  return remote_service->GetCharacteristic(gatt_characteristic);
}

std::string BluetoothLowEnergyWeaveClientConnection::GetReasonForClose() {
  switch (packet_receiver_->GetReasonForClose()) {
    case ReasonForClose::CLOSE_WITHOUT_ERROR:
      return "CLOSE_WITHOUT_ERROR";
    case ReasonForClose::UNKNOWN_ERROR:
      return "UNKNOWN_ERROR";
    case ReasonForClose::NO_COMMON_VERSION_SUPPORTED:
      return "NO_COMMON_VERSION_SUPPORTED";
    case ReasonForClose::RECEIVED_PACKET_OUT_OF_SEQUENCE:
      return "RECEIVED_PACKET_OUT_OF_SEQUENCE";
    case ReasonForClose::APPLICATION_ERROR:
      return "APPLICATION_ERROR";
    default:
      NOTREACHED();
      return "";
  }
}

BluetoothLowEnergyWeaveClientConnection::WriteRequest::WriteRequest(
    const Packet& val,
    WriteRequestType request_type,
    WireMessage* associated_wire_message)
    : value(val),
      request_type(request_type),
      associated_wire_message(associated_wire_message) {}

BluetoothLowEnergyWeaveClientConnection::WriteRequest::WriteRequest(
    const Packet& val,
    WriteRequestType request_type)
    : WriteRequest(val, request_type, nullptr /* associated_wire_message */) {}

BluetoothLowEnergyWeaveClientConnection::WriteRequest::~WriteRequest() {}

}  // namespace weave

}  // namespace cryptauth
