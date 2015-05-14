// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proximity_auth/ble/bluetooth_low_energy_connection.h"

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/proximity_auth/ble/fake_wire_message.h"
#include "components/proximity_auth/connection_finder.h"
#include "components/proximity_auth/wire_message.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_gatt_connection.h"
#include "device/bluetooth/bluetooth_gatt_notify_session.h"
#include "device/bluetooth/bluetooth_uuid.h"

using device::BluetoothAdapter;
using device::BluetoothDevice;
using device::BluetoothGattConnection;
using device::BluetoothGattService;
using device::BluetoothGattCharacteristic;
using device::BluetoothGattNotifySession;
using device::BluetoothUUID;

namespace proximity_auth {

BluetoothLowEnergyConnection::BluetoothLowEnergyConnection(
    const RemoteDevice& device,
    scoped_refptr<device::BluetoothAdapter> adapter,
    const BluetoothUUID remote_service_uuid,
    const BluetoothUUID to_peripheral_char_uuid,
    const BluetoothUUID from_peripheral_char_uuid,
    scoped_ptr<BluetoothGattConnection> gatt_connection)
    : Connection(device),
      adapter_(adapter),
      remote_service_({remote_service_uuid, ""}),
      to_peripheral_char_({to_peripheral_char_uuid, ""}),
      from_peripheral_char_({from_peripheral_char_uuid, ""}),
      connection_(gatt_connection.Pass()),
      notify_session_pending_(false),
      connect_signal_response_pending_(false),
      weak_ptr_factory_(this) {
  DCHECK(connection_);
  DCHECK(adapter_);
  DCHECK(adapter_->IsInitialized());

  start_time_ = base::TimeTicks::Now();
  SetStatus(IN_PROGRESS);

  // We should set the status to IN_PROGRESS before calling this function, as we
  // can set the status to CONNECTED by an inner call of
  // HandleCharacteristicUpdate().
  ScanRemoteCharacteristics(GetRemoteService());

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
  NOTREACHED();
}

// This actually forgets the remote BLE device. This is safe as long as we only
// connect to BLE devices advertising the SmartLock service (assuming this
// device has no other being used).
void BluetoothLowEnergyConnection::Disconnect() {
  StopNotifySession();
  SetStatus(DISCONNECTED);
  if (connection_) {
    connection_.reset();
    BluetoothDevice* device = GetRemoteDevice();
    if (device) {
      VLOG(1) << "Forget device " << device->GetAddress();
      device->Forget(base::Bind(&base::DoNothing));
    }
  }
}

// TODO(sacomoto): Send a SmartLock BLE socket incoming signal. Implement a
// sender with full support for messages larger than a single characteristic
// value.
void BluetoothLowEnergyConnection::SendMessageImpl(
    scoped_ptr<WireMessage> message) {
  DCHECK(!GetGattCharacteristic(to_peripheral_char_.id));
  VLOG(1) << "Sending message " << message->Serialize();

  std::string serialized_message = message->Serialize();
  std::vector<uint8> bytes(serialized_message.begin(),
                           serialized_message.end());

  GetGattCharacteristic(to_peripheral_char_.id)
      ->WriteRemoteCharacteristic(
          bytes, base::Bind(&base::DoNothing),
          base::Bind(
              &BluetoothLowEnergyConnection::OnWriteRemoteCharacteristicError,
              weak_ptr_factory_.GetWeakPtr()));
}

void BluetoothLowEnergyConnection::DeviceRemoved(BluetoothAdapter* adapter,
                                                 BluetoothDevice* device) {
  if (device && device->GetAddress() == GetRemoteDeviceAddress()) {
    VLOG(1) << "Device removed " << GetRemoteDeviceAddress();
    Disconnect();
  }
}

void BluetoothLowEnergyConnection::GattDiscoveryCompleteForService(
    BluetoothAdapter* adapter,
    BluetoothGattService* service) {
  if (service && service->GetUUID() == remote_service_.uuid) {
    VLOG(1) << "All characteristics discovered for "
            << remote_service_.uuid.canonical_value();

    if (to_peripheral_char_.id.empty() || from_peripheral_char_.id.empty()) {
      VLOG(1) << "Connection error, missing characteristics for SmartLock "
                 "service.\n"
              << (to_peripheral_char_.id.empty()
                      ? to_peripheral_char_.uuid.canonical_value()
                      : "")
              << (from_peripheral_char_.id.empty()
                      ? ", " + from_peripheral_char_.uuid.canonical_value()
                      : "") << " not found.";
      Disconnect();
    }
  }
}

void BluetoothLowEnergyConnection::GattCharacteristicAdded(
    BluetoothAdapter* adapter,
    BluetoothGattCharacteristic* characteristic) {
  VLOG(1) << "New char found: " << characteristic->GetUUID().canonical_value();
  HandleCharacteristicUpdate(characteristic);
}

// TODO(sacomoto): Parse the SmartLock BLE socket incoming signal. Implement a
// receiver with full suport for messages larger than a single characteristic
// value.
void BluetoothLowEnergyConnection::GattCharacteristicValueChanged(
    BluetoothAdapter* adapter,
    BluetoothGattCharacteristic* characteristic,
    const std::vector<uint8>& value) {
  DCHECK_EQ(adapter, adapter_.get());

  VLOG(1) << "Characteristic value changed: "
          << characteristic->GetUUID().canonical_value();

  if (characteristic->GetIdentifier() == from_peripheral_char_.id) {
    const ControlSignal signal = static_cast<ControlSignal>(ToUint32(value));

    switch (signal) {
      case ControlSignal::kInvitationResponseSignal:
        connect_signal_response_pending_ = false;
        CompleteConnection();
        break;
      case ControlSignal::kInviteToConnectSignal:
      case ControlSignal::kSendSignal:
      // TODO(sacomoto): Actually handle the message and call OnBytesReceived
      // when complete.
      case ControlSignal::kDisconnectSignal:
        Disconnect();
        break;
    }
  }
}

scoped_ptr<WireMessage> BluetoothLowEnergyConnection::DeserializeWireMessage(
    bool* is_incomplete_message) {
  return FakeWireMessage::Deserialize(received_bytes(), is_incomplete_message);
}

void BluetoothLowEnergyConnection::ScanRemoteCharacteristics(
    BluetoothGattService* service) {
  if (service) {
    std::vector<device::BluetoothGattCharacteristic*> characteristics =
        service->GetCharacteristics();
    for (auto iter = characteristics.begin(); iter != characteristics.end();
         iter++) {
      HandleCharacteristicUpdate(*iter);
    }
  }
}

void BluetoothLowEnergyConnection::HandleCharacteristicUpdate(
    BluetoothGattCharacteristic* characteristic) {
  // Checks if |characteristic| is equal to |from_peripheral_char_| or
  // |to_peripheral_char_|.
  UpdateCharacteristicsStatus(characteristic);

  // Starts a notify session for |from_peripheral_char_|.
  if (characteristic->GetIdentifier() == from_peripheral_char_.id)
    StartNotifySession();

  // Sends a invite to connect signal if ready.
  SendInviteToConnectSignal();
}

void BluetoothLowEnergyConnection::CompleteConnection() {
  if (status() == IN_PROGRESS && !connect_signal_response_pending_ &&
      !to_peripheral_char_.id.empty() && !from_peripheral_char_.id.empty() &&
      notify_session_) {
    VLOG(1) << "Connection completed";
    VLOG(1) << "Time elapsed: " << base::TimeTicks::Now() - start_time_;
    SetStatus(CONNECTED);
  }
}

void BluetoothLowEnergyConnection::StartNotifySession() {
  BluetoothGattCharacteristic* characteristic =
      GetGattCharacteristic(from_peripheral_char_.id);
  if (!characteristic) {
    VLOG(1) << "Characteristic " << from_peripheral_char_.uuid.canonical_value()
            << " not found.";
    return;
  }

  if (notify_session_ || notify_session_pending_) {
    VLOG(1) << "Notify session already started.";
    return;
  }

  notify_session_pending_ = true;
  characteristic->StartNotifySession(
      base::Bind(&BluetoothLowEnergyConnection::OnNotifySessionStarted,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&BluetoothLowEnergyConnection::OnNotifySessionError,
                 weak_ptr_factory_.GetWeakPtr()));
}

void BluetoothLowEnergyConnection::OnNotifySessionError(
    BluetoothGattService::GattErrorCode error) {
  VLOG(1) << "Error starting notification session: " << error;
  notify_session_pending_ = false;
}

void BluetoothLowEnergyConnection::OnNotifySessionStarted(
    scoped_ptr<BluetoothGattNotifySession> notify_session) {
  VLOG(1) << "Notification session started "
          << notify_session->GetCharacteristicIdentifier();
  notify_session_ = notify_session.Pass();
  notify_session_pending_ = false;

  // Sends an invite to connect signal if ready.
  SendInviteToConnectSignal();
}

void BluetoothLowEnergyConnection::StopNotifySession() {
  if (notify_session_) {
    notify_session_->Stop(base::Bind(&base::DoNothing));
    notify_session_.reset();
  }
}

void BluetoothLowEnergyConnection::OnWriteRemoteCharacteristicError(
    BluetoothGattService::GattErrorCode error) {
  VLOG(1) << "Error writing characteristic"
          << to_peripheral_char_.uuid.canonical_value();
}

void BluetoothLowEnergyConnection::SendInviteToConnectSignal() {
  if (status() == IN_PROGRESS && !connect_signal_response_pending_ &&
      !to_peripheral_char_.id.empty() && !from_peripheral_char_.id.empty() &&
      notify_session_) {
    VLOG(1) << "Sending invite to connect signal";
    connect_signal_response_pending_ = true;

    // The connection status is not CONNECTED yet, so in order to bypass the
    // check in SendMessage implementation we need to send the message using the
    // private implementation.
    SendMessageImpl(scoped_ptr<FakeWireMessage>(new FakeWireMessage(
        ToString(static_cast<uint32>(ControlSignal::kInviteToConnectSignal)))));
  }
}

void BluetoothLowEnergyConnection::UpdateCharacteristicsStatus(
    BluetoothGattCharacteristic* characteristic) {
  if (characteristic) {
    BluetoothUUID uuid = characteristic->GetUUID();
    if (to_peripheral_char_.uuid == uuid)
      to_peripheral_char_.id = characteristic->GetIdentifier();
    if (from_peripheral_char_.uuid == uuid)
      from_peripheral_char_.id = characteristic->GetIdentifier();

    BluetoothGattService* service = characteristic->GetService();
    if (service && service->GetUUID() == remote_service_.uuid)
      remote_service_.id = service->GetIdentifier();
  }
}

const std::string& BluetoothLowEnergyConnection::GetRemoteDeviceAddress() {
  return remote_device().bluetooth_address;
}

BluetoothDevice* BluetoothLowEnergyConnection::GetRemoteDevice() {
  if (!adapter_ || !adapter_->IsInitialized()) {
    VLOG(1) << "adapter not ready";
    return NULL;
  }
  return adapter_->GetDevice(GetRemoteDeviceAddress());
}

BluetoothGattService* BluetoothLowEnergyConnection::GetRemoteService() {
  BluetoothDevice* remote_device = GetRemoteDevice();
  if (!remote_device) {
    VLOG(1) << "device not found";
    return NULL;
  }
  if (remote_service_.id.empty()) {
    std::vector<BluetoothGattService*> services =
        remote_device->GetGattServices();
    for (auto iter = services.begin(); iter != services.end(); ++iter)
      if ((*iter)->GetUUID() == remote_service_.uuid) {
        remote_service_.id = (*iter)->GetIdentifier();
        break;
      }
  }
  return remote_device->GetGattService(remote_service_.id);
}

BluetoothGattCharacteristic*
BluetoothLowEnergyConnection::GetGattCharacteristic(
    const std::string& gatt_characteristic) {
  BluetoothGattService* remote_service = GetRemoteService();
  if (!remote_service) {
    VLOG(1) << "service not found";
    return NULL;
  }
  return remote_service->GetCharacteristic(gatt_characteristic);
}

// TODO(sacomoto): make this robust to byte ordering in both sides of the
// SmartLock BLE socket.
uint32 BluetoothLowEnergyConnection::ToUint32(const std::vector<uint8>& bytes) {
  return bytes[0] | (bytes[1] << 8) | (bytes[2] << 16) | (bytes[3] << 24);
}

// TODO(sacomoto): make this robust to byte ordering in both sides of the
// SmartLock BLE socket.
const std::string BluetoothLowEnergyConnection::ToString(const uint32 value) {
  char bytes[4] = {};

  bytes[0] = static_cast<uint8>(value);
  bytes[1] = static_cast<uint8>(value >> 8);
  bytes[2] = static_cast<uint8>(value >> 12);
  bytes[3] = static_cast<uint8>(value >> 24);

  return std::string(bytes);
}

}  // namespace proximity_auth
