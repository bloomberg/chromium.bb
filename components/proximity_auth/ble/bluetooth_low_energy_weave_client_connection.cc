// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proximity_auth/ble/bluetooth_low_energy_weave_client_connection.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/proximity_auth/bluetooth_throttler.h"
#include "components/proximity_auth/connection_finder.h"
#include "components/proximity_auth/logging/logging.h"
#include "components/proximity_auth/wire_message.h"
#include "device/bluetooth/bluetooth_gatt_connection.h"

using device::BluetoothAdapter;
using device::BluetoothDevice;
using device::BluetoothGattConnection;
using device::BluetoothRemoteGattService;
using device::BluetoothRemoteGattCharacteristic;
using device::BluetoothGattNotifySession;
using device::BluetoothUUID;

namespace proximity_auth {
namespace weave {
namespace {

typedef BluetoothLowEnergyWeavePacketReceiver::State ReceiverState;

// The UUID of the TX characteristic used to transmit data to the server.
const char kTXCharacteristicUUID[] = "00000100-0004-1000-8000-001A11000101";

// The UUID of the RX characteristic used to receive data from the server.
const char kRXCharacteristicUUID[] = "00000100-0004-1000-8000-001A11000102";

}  // namespace

BluetoothLowEnergyWeaveClientConnection::
    BluetoothLowEnergyWeaveClientConnection(
        const RemoteDevice& device,
        scoped_refptr<device::BluetoothAdapter> adapter,
        const BluetoothUUID remote_service_uuid,
        BluetoothThrottler* bluetooth_throttler,
        int max_number_of_write_attempts)
    : Connection(device),
      adapter_(adapter),
      remote_service_({remote_service_uuid, ""}),
      packet_generator_(
          BluetoothLowEnergyWeavePacketGenerator::Factory::NewInstance()),
      packet_receiver_(
          BluetoothLowEnergyWeavePacketReceiver::Factory::NewInstance(
              BluetoothLowEnergyWeavePacketReceiver::ReceiverType::CLIENT)),
      tx_characteristic_({BluetoothUUID(kTXCharacteristicUUID), ""}),
      rx_characteristic_({BluetoothUUID(kRXCharacteristicUUID), ""}),
      bluetooth_throttler_(bluetooth_throttler),
      task_runner_(base::ThreadTaskRunnerHandle::Get()),
      sub_status_(SubStatus::DISCONNECTED),
      write_remote_characteristic_pending_(false),
      max_number_of_write_attempts_(max_number_of_write_attempts),
      weak_ptr_factory_(this) {
  DCHECK(adapter_);
  DCHECK(adapter_->IsInitialized());

  adapter_->AddObserver(this);
}

BluetoothLowEnergyWeaveClientConnection::
    ~BluetoothLowEnergyWeaveClientConnection() {
  // Since the destructor can be called at anytime, it would be unwise to send a
  // connection close since we might not be connected at all.
  DestroyConnection();

  if (adapter_) {
    adapter_->RemoveObserver(this);
    adapter_ = NULL;
  }
}

void BluetoothLowEnergyWeaveClientConnection::Connect() {
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
        base::Bind(
            &BluetoothLowEnergyWeaveClientConnection::CreateGattConnection,
            weak_ptr_factory_.GetWeakPtr()),
        throttler_delay);
    return;
  }

  CreateGattConnection();
}

void BluetoothLowEnergyWeaveClientConnection::CreateGattConnection() {
  DCHECK(sub_status() == SubStatus::WAITING_GATT_CONNECTION);

  BluetoothDevice* remote_device = GetRemoteDevice();
  if (remote_device) {
    PA_LOG(INFO) << "Creating GATT connection with "
                 << remote_device->GetAddress();

    remote_device->CreateGattConnection(
        base::Bind(
            &BluetoothLowEnergyWeaveClientConnection::OnGattConnectionCreated,
            weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&BluetoothLowEnergyWeaveClientConnection::
                       OnCreateGattConnectionError,
                   weak_ptr_factory_.GetWeakPtr()));
  } else {
    SetSubStatus(SubStatus::DISCONNECTED);
  }
}

void BluetoothLowEnergyWeaveClientConnection::Disconnect() {
  if (sub_status_ == SubStatus::CONNECTED) {
    // Friendly disconnect by sending a connection close and then destroy the
    // the connection once the connection close has been sent.
    WriteRequest request(packet_generator_->CreateConnectionClose(
                             ReasonForClose::CLOSE_WITHOUT_ERROR),
                         WriteRequestType::CONNECTION_CLOSE);
    WriteRemoteCharacteristic(request);
  } else {
    DestroyConnection();
  }
}

void BluetoothLowEnergyWeaveClientConnection::DestroyConnection() {
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

void BluetoothLowEnergyWeaveClientConnection::SetSubStatus(
    SubStatus new_sub_status) {
  sub_status_ = new_sub_status;

  // Sets the status of parent class proximity_auth::Connection accordingly.
  if (new_sub_status == SubStatus::CONNECTED) {
    SetStatus(Status::CONNECTED);
  } else if (new_sub_status == SubStatus::DISCONNECTED) {
    SetStatus(Status::DISCONNECTED);
  } else {
    SetStatus(Status::IN_PROGRESS);
  }
}

void BluetoothLowEnergyWeaveClientConnection::SetTaskRunnerForTesting(
    scoped_refptr<base::TaskRunner> task_runner) {
  task_runner_ = task_runner;
}

void BluetoothLowEnergyWeaveClientConnection::SendMessageImpl(
    std::unique_ptr<WireMessage> message) {
  PA_LOG(INFO) << "Sending message " << message->Serialize();
  std::string serialized_msg = message->Serialize();

  std::vector<Packet> packets =
      packet_generator_->EncodeDataMessage(serialized_msg);

  std::shared_ptr<WireMessage> request_message(message.release());

  for (uint32_t i = 0; i < packets.size(); ++i) {
    WriteRequestType request_type = (i == packets.size() - 1)
                                        ? WriteRequestType::MESSAGE_COMPLETE
                                        : WriteRequestType::REGULAR;
    WriteRequest request(packets[i], request_type, request_message);
    WriteRemoteCharacteristic(request);
  }
}

// Changes in the GATT connection with the remote device should be observed
// here. If the GATT connection is dropped, we should call DestroyConnection()
// anyway, so the object can notify its observers.
void BluetoothLowEnergyWeaveClientConnection::DeviceChanged(
    BluetoothAdapter* adapter,
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
    DestroyConnection();
  }
}

void BluetoothLowEnergyWeaveClientConnection::DeviceRemoved(
    BluetoothAdapter* adapter,
    BluetoothDevice* device) {
  DCHECK(device);
  if (sub_status_ == SubStatus::DISCONNECTED ||
      device->GetAddress() != GetDeviceAddress())
    return;

  PA_LOG(INFO) << "Device removed " << GetDeviceAddress();
  DestroyConnection();
}

void BluetoothLowEnergyWeaveClientConnection::GattCharacteristicValueChanged(
    BluetoothAdapter* adapter,
    BluetoothRemoteGattCharacteristic* characteristic,
    const Packet& value) {
  DCHECK_EQ(adapter, adapter_.get());
  if (sub_status() != SubStatus::WAITING_CONNECTION_RESPONSE &&
      sub_status() != SubStatus::CONNECTED)
    return;

  PA_LOG(INFO) << "Characteristic value changed: "
               << characteristic->GetUUID().canonical_value();

  if (characteristic->GetIdentifier() == rx_characteristic_.id) {
    ReceiverState state = packet_receiver_->ReceivePacket(value);

    PA_LOG(INFO) << "\nReceiver State: " << state;
    switch (state) {
      case ReceiverState::DATA_READY:
        OnBytesReceived(packet_receiver_->GetDataMessage());
        break;
      case ReceiverState::CONNECTION_CLOSED:
        PA_LOG(ERROR) << "Connection closed due to: " << GetReasonForClose();
        DestroyConnection();
        break;
      case ReceiverState::ERROR_DETECTED:
        OnPacketReceiverError();
        break;
      case ReceiverState::WAITING:
        // Receiver state should have changed from CONNECTING to WAITING if
        // a proper connection response had been received.
        // The max packet size selected from the connection response will be
        // used to generate future data packets.
        packet_generator_->SetMaxPacketSize(
            packet_receiver_->GetMaxPacketSize());
        DCHECK(sub_status() == SubStatus::WAITING_CONNECTION_RESPONSE);
        CompleteConnection();
        break;
      case ReceiverState::RECEIVING_DATA:
        // Normal in between states, so do nothing.
        break;
      default:
        NOTREACHED();
    }
  }
}

void BluetoothLowEnergyWeaveClientConnection::CompleteConnection() {
  PA_LOG(INFO) << "Connection completed. Time elapsed: "
               << base::TimeTicks::Now() - start_time_;
  SetSubStatus(SubStatus::CONNECTED);
}

void BluetoothLowEnergyWeaveClientConnection::OnCreateGattConnectionError(
    device::BluetoothDevice::ConnectErrorCode error_code) {
  DCHECK(sub_status_ == SubStatus::WAITING_GATT_CONNECTION);
  PA_LOG(WARNING) << "Error creating GATT connection to "
                  << remote_device().bluetooth_address
                  << " error code: " << error_code;
  DestroyConnection();
}

void BluetoothLowEnergyWeaveClientConnection::OnGattConnectionCreated(
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
      adapter_, GetRemoteDevice(), remote_service_, tx_characteristic_,
      rx_characteristic_, success_callback, error_callback);
}

void BluetoothLowEnergyWeaveClientConnection::OnCharacteristicsFound(
    const RemoteAttribute& service,
    const RemoteAttribute& tx_characteristic,
    const RemoteAttribute& rx_characteristic) {
  PA_LOG(INFO) << "Remote chacteristics found.";
  PrintTimeElapsed();

  DCHECK(sub_status() == SubStatus::WAITING_CHARACTERISTICS);
  remote_service_ = service;
  tx_characteristic_ = tx_characteristic;
  rx_characteristic_ = rx_characteristic;

  SetSubStatus(SubStatus::CHARACTERISTICS_FOUND);
  StartNotifySession();
}

void BluetoothLowEnergyWeaveClientConnection::OnCharacteristicsFinderError(
    const RemoteAttribute& tx_characteristic,
    const RemoteAttribute& rx_characteristic) {
  DCHECK(sub_status() == SubStatus::WAITING_CHARACTERISTICS);
  PA_LOG(WARNING) << "Connection error, missing characteristics for SmartLock "
                     "service.\n"
                  << (tx_characteristic.id.empty()
                          ? tx_characteristic.uuid.canonical_value()
                          : "")
                  << ", "
                  << (rx_characteristic.id.empty()
                          ? ", " + rx_characteristic.uuid.canonical_value()
                          : "")
                  << " not found.";

  DestroyConnection();
}

void BluetoothLowEnergyWeaveClientConnection::StartNotifySession() {
  DCHECK(sub_status() == SubStatus::CHARACTERISTICS_FOUND);
  BluetoothRemoteGattCharacteristic* characteristic =
      GetGattCharacteristic(rx_characteristic_.id);
  CHECK(characteristic);

  // This is a workaround for crbug.com/507325. If |characteristic| is already
  // notifying |characteristic->StartNotifySession()| will fail with
  // GATT_ERROR_FAILED.
  if (characteristic->IsNotifying()) {
    PA_LOG(INFO) << characteristic->GetUUID().canonical_value()
                 << " already notifying.";
    SetSubStatus(SubStatus::NOTIFY_SESSION_READY);
    SendConnectionRequest();
    return;
  }

  SetSubStatus(SubStatus::WAITING_NOTIFY_SESSION);
  characteristic->StartNotifySession(
      base::Bind(
          &BluetoothLowEnergyWeaveClientConnection::OnNotifySessionStarted,
          weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&BluetoothLowEnergyWeaveClientConnection::OnNotifySessionError,
                 weak_ptr_factory_.GetWeakPtr()));
}

void BluetoothLowEnergyWeaveClientConnection::OnNotifySessionStarted(
    std::unique_ptr<BluetoothGattNotifySession> notify_session) {
  DCHECK(sub_status() == SubStatus::WAITING_NOTIFY_SESSION);
  PA_LOG(INFO) << "Notification session started "
               << notify_session->GetCharacteristicIdentifier();
  PrintTimeElapsed();

  SetSubStatus(SubStatus::NOTIFY_SESSION_READY);
  notify_session_ = std::move(notify_session);

  SendConnectionRequest();
}

void BluetoothLowEnergyWeaveClientConnection::OnNotifySessionError(
    BluetoothRemoteGattService::GattErrorCode error) {
  DCHECK(sub_status() == SubStatus::WAITING_NOTIFY_SESSION);
  PA_LOG(WARNING) << "Error starting notification session: " << error;
  DestroyConnection();
}

void BluetoothLowEnergyWeaveClientConnection::StopNotifySession() {
  if (notify_session_) {
    notify_session_->Stop(base::Bind(&base::DoNothing));
    notify_session_.reset();
  }
}

void BluetoothLowEnergyWeaveClientConnection::SendConnectionRequest() {
  if (sub_status() == SubStatus::NOTIFY_SESSION_READY) {
    PA_LOG(INFO) << "Sending connection request to the server";
    SetSubStatus(SubStatus::WAITING_CONNECTION_RESPONSE);

    WriteRequest write_request(packet_generator_->CreateConnectionRequest(),
                               WriteRequestType::CONNECTION_REQUEST);

    WriteRemoteCharacteristic(write_request);
  }
}

void BluetoothLowEnergyWeaveClientConnection::WriteRemoteCharacteristic(
    const WriteRequest& request) {
  write_requests_queue_.push(request);
  PA_LOG(ERROR) << "you got " << request.message.get();
  ProcessNextWriteRequest();
}

void BluetoothLowEnergyWeaveClientConnection::ProcessNextWriteRequest() {
  BluetoothRemoteGattCharacteristic* characteristic =
      GetGattCharacteristic(tx_characteristic_.id);
  if (!write_requests_queue_.empty() && !write_remote_characteristic_pending_ &&
      characteristic) {
    write_remote_characteristic_pending_ = true;
    const WriteRequest& next_request = write_requests_queue_.front();

    PA_LOG(INFO) << "Writing to characteristic " << next_request.value.size()
                 << " bytes";
    characteristic->WriteRemoteCharacteristic(
        next_request.value,
        base::Bind(&BluetoothLowEnergyWeaveClientConnection::
                       OnRemoteCharacteristicWritten,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&BluetoothLowEnergyWeaveClientConnection::
                       OnWriteRemoteCharacteristicError,
                   weak_ptr_factory_.GetWeakPtr()));
  }
}

void BluetoothLowEnergyWeaveClientConnection::OnRemoteCharacteristicWritten() {
  PA_LOG(INFO) << "Characteristic written.";

  DCHECK(!write_requests_queue_.empty());

  const WriteRequest& request = write_requests_queue_.front();

  switch (request.request_type) {
    case WriteRequestType::REGULAR:
    case WriteRequestType::CONNECTION_REQUEST:
      break;
    case WriteRequestType::MESSAGE_COMPLETE:
      OnDidSendMessage(*request.message, true);
      break;
    case WriteRequestType::CONNECTION_CLOSE:
      DestroyConnection();
      break;
    default:
      NOTREACHED();
  }

  // Removes the top of queue (already processed) and process the next request.
  write_requests_queue_.pop();
  write_remote_characteristic_pending_ = false;
  ProcessNextWriteRequest();
}

void BluetoothLowEnergyWeaveClientConnection::OnWriteRemoteCharacteristicError(
    BluetoothRemoteGattService::GattErrorCode error) {
  PA_LOG(WARNING) << "Error " << error << " writing characteristic: "
                  << tx_characteristic_.uuid.canonical_value();

  write_remote_characteristic_pending_ = false;

  DCHECK(!write_requests_queue_.empty());
  WriteRequest* request = &write_requests_queue_.front();

  // Increases the number of failed attempts and retry.

  if (++request->number_of_failed_attempts >= max_number_of_write_attempts_) {
    switch (request->request_type) {
      case WriteRequestType::REGULAR:
      case WriteRequestType::MESSAGE_COMPLETE:
        OnDidSendMessage(*request->message, false);
        break;
      case WriteRequestType::CONNECTION_CLOSE:
      case WriteRequestType::CONNECTION_REQUEST:
        break;
      default:
        NOTREACHED();
    }

    // If the previous write failed that many times, probably can't write a
    // connection close either, so just destroy the connection.
    DestroyConnection();
  } else {
    ProcessNextWriteRequest();
  }
}

void BluetoothLowEnergyWeaveClientConnection::OnPacketReceiverError() {
  PA_LOG(ERROR) << "Received erroneous packet. Closing connection.";

  WriteRequest request(packet_generator_->CreateConnectionClose(
                           packet_receiver_->GetReasonToClose()),
                       WriteRequestType::CONNECTION_CLOSE);

  // Skip all other writes that's not in progress.

  // C++ queue does not have a clear method.
  // According to stackoverflow
  // "http://stackoverflow.com/questions/709146/how-do-i-clear-the-stdqueue-
  //  efficiently"
  std::queue<WriteRequest> empty;
  std::swap(write_requests_queue_, empty);

  if (write_remote_characteristic_pending_) {
    // Add the in progress write back to the queue.
    write_requests_queue_.push(empty.front());
  }

  WriteRemoteCharacteristic(request);
}

void BluetoothLowEnergyWeaveClientConnection::PrintTimeElapsed() {
  PA_LOG(INFO) << "Time elapsed: " << base::TimeTicks::Now() - start_time_;
}

std::string BluetoothLowEnergyWeaveClientConnection::GetDeviceAddress() {
  // When the remote device is connected we should rely on the address given by
  // |gatt_connection_|. As the device address may change if the device is
  // paired. The address in |gatt_connection_| is automatically updated in this
  // case.
  return gatt_connection_ ? gatt_connection_->GetDeviceAddress()
                          : remote_device().bluetooth_address;
}

BluetoothDevice* BluetoothLowEnergyWeaveClientConnection::GetRemoteDevice() {
  // It's not possible to simply use
  // |adapter_->GetDevice(GetDeviceAddress())| to find the device with MAC
  // address |GetDeviceAddress()|. For paired devices,
  // BluetoothAdapter::GetDevice(XXX) searches for the temporary MAC address
  // XXX, whereas |GetDeviceAddress()| is the real MAC address. This is a
  // bug in the way device::BluetoothAdapter is storing the devices (see
  // crbug.com/497841).
  std::vector<BluetoothDevice*> devices = adapter_->GetDevices();
  for (const auto& device : devices) {
    if (device->GetAddress() == GetDeviceAddress())
      return device;
  }

  return nullptr;
}

BluetoothRemoteGattService*
BluetoothLowEnergyWeaveClientConnection::GetRemoteService() {
  BluetoothDevice* remote_device = GetRemoteDevice();
  if (!remote_device) {
    PA_LOG(WARNING) << "Remote device not found.";
    return NULL;
  }
  if (remote_service_.id.empty()) {
    std::vector<BluetoothRemoteGattService*> services =
        remote_device->GetGattServices();
    for (const auto& service : services)
      if (service->GetUUID() == remote_service_.uuid) {
        remote_service_.id = service->GetIdentifier();
        break;
      }
  }
  return remote_device->GetGattService(remote_service_.id);
}

BluetoothRemoteGattCharacteristic*
BluetoothLowEnergyWeaveClientConnection::GetGattCharacteristic(
    const std::string& gatt_characteristic) {
  BluetoothRemoteGattService* remote_service = GetRemoteService();
  if (!remote_service) {
    PA_LOG(WARNING) << "Remote service not found.";
    return NULL;
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
    std::shared_ptr<WireMessage> message)
    : value(val),
      request_type(request_type),
      message(message),
      number_of_failed_attempts(0) {}

BluetoothLowEnergyWeaveClientConnection::WriteRequest::WriteRequest(
    const Packet& val,
    WriteRequestType request_type)
    : value(val),
      request_type(request_type),
      message(nullptr),
      number_of_failed_attempts(0) {}

BluetoothLowEnergyWeaveClientConnection::WriteRequest::WriteRequest(
    const WriteRequest& other) = default;

BluetoothLowEnergyWeaveClientConnection::WriteRequest::~WriteRequest() {}

}  // namespace weave

}  // namespace proximity_auth
