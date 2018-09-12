// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/ble/fido_ble_connection.h"

#include <algorithm>
#include <ostream>
#include <utility>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "device/bluetooth/bluetooth_gatt_connection.h"
#include "device/bluetooth/bluetooth_gatt_notify_session.h"
#include "device/bluetooth/bluetooth_remote_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_remote_gatt_service.h"
#include "device/bluetooth/bluetooth_uuid.h"
#include "device/fido/ble/fido_ble_uuids.h"

namespace device {

namespace {

using ServiceRevisionsCallback =
    base::OnceCallback<void(std::vector<FidoBleConnection::ServiceRevision>)>;

constexpr const char* ToString(BluetoothDevice::ConnectErrorCode error_code) {
  switch (error_code) {
    case BluetoothDevice::ERROR_AUTH_CANCELED:
      return "ERROR_AUTH_CANCELED";
    case BluetoothDevice::ERROR_AUTH_FAILED:
      return "ERROR_AUTH_FAILED";
    case BluetoothDevice::ERROR_AUTH_REJECTED:
      return "ERROR_AUTH_REJECTED";
    case BluetoothDevice::ERROR_AUTH_TIMEOUT:
      return "ERROR_AUTH_TIMEOUT";
    case BluetoothDevice::ERROR_FAILED:
      return "ERROR_FAILED";
    case BluetoothDevice::ERROR_INPROGRESS:
      return "ERROR_INPROGRESS";
    case BluetoothDevice::ERROR_UNKNOWN:
      return "ERROR_UNKNOWN";
    case BluetoothDevice::ERROR_UNSUPPORTED_DEVICE:
      return "ERROR_UNSUPPORTED_DEVICE";
    default:
      NOTREACHED();
      return "";
  }
}

constexpr const char* ToString(BluetoothGattService::GattErrorCode error_code) {
  switch (error_code) {
    case BluetoothGattService::GATT_ERROR_UNKNOWN:
      return "GATT_ERROR_UNKNOWN";
    case BluetoothGattService::GATT_ERROR_FAILED:
      return "GATT_ERROR_FAILED";
    case BluetoothGattService::GATT_ERROR_IN_PROGRESS:
      return "GATT_ERROR_IN_PROGRESS";
    case BluetoothGattService::GATT_ERROR_INVALID_LENGTH:
      return "GATT_ERROR_INVALID_LENGTH";
    case BluetoothGattService::GATT_ERROR_NOT_PERMITTED:
      return "GATT_ERROR_NOT_PERMITTED";
    case BluetoothGattService::GATT_ERROR_NOT_AUTHORIZED:
      return "GATT_ERROR_NOT_AUTHORIZED";
    case BluetoothGattService::GATT_ERROR_NOT_PAIRED:
      return "GATT_ERROR_NOT_PAIRED";
    case BluetoothGattService::GATT_ERROR_NOT_SUPPORTED:
      return "GATT_ERROR_NOT_SUPPORTED";
    default:
      NOTREACHED();
      return "";
  }
}

std::ostream& operator<<(std::ostream& os,
                         FidoBleConnection::ServiceRevision revision) {
  switch (revision) {
    case FidoBleConnection::ServiceRevision::kU2f11:
      return os << "U2F 1.1";
    case FidoBleConnection::ServiceRevision::kU2f12:
      return os << "U2F 1.2";
    case FidoBleConnection::ServiceRevision::kFido2:
      return os << "FIDO2";
  }

  NOTREACHED();
  return os;
}

void OnWriteRemoteCharacteristic(FidoBleConnection::WriteCallback callback) {
  VLOG(2) << "Writing Remote Characteristic Succeeded.";
  std::move(callback).Run(true);
}

void OnWriteRemoteCharacteristicError(
    FidoBleConnection::WriteCallback callback,
    BluetoothGattService::GattErrorCode error_code) {
  VLOG(2) << "Writing Remote Characteristic Failed: " << ToString(error_code);
  std::move(callback).Run(false);
}

void OnReadServiceRevisionBitfield(ServiceRevisionsCallback callback,
                                   const std::vector<uint8_t>& value) {
  if (value.empty()) {
    VLOG(2) << "Service Revision Bitfield is empty.";
    std::move(callback).Run({});
    return;
  }

  if (value.size() != 1u) {
    VLOG(2) << "Service Revision Bitfield has unexpected size: " << value.size()
            << ". Ignoring all but the first byte.";
  }

  const uint8_t bitset = value[0];
  if (bitset & 0x1F) {
    VLOG(2) << "Service Revision Bitfield has unexpected bits set: "
            << base::StringPrintf("0x%02X", bitset)
            << ". Ignoring all but the first three bits.";
  }

  std::vector<FidoBleConnection::ServiceRevision> service_revisions;
  for (auto revision : {FidoBleConnection::ServiceRevision::kU2f11,
                        FidoBleConnection::ServiceRevision::kU2f12,
                        FidoBleConnection::ServiceRevision::kFido2}) {
    if (bitset & static_cast<uint8_t>(revision)) {
      VLOG(2) << "Detected Support for " << revision << ".";
      service_revisions.push_back(revision);
    }
  }

  std::move(callback).Run(std::move(service_revisions));
}

void OnReadServiceRevisionBitfieldError(
    ServiceRevisionsCallback callback,
    BluetoothGattService::GattErrorCode error_code) {
  VLOG(2) << "Error while reading Service Revision Bitfield: "
          << ToString(error_code);
  std::move(callback).Run({});
}

}  // namespace

FidoBleConnection::FidoBleConnection(
    BluetoothAdapter* adapter,
    std::string device_address,
    ConnectionStatusCallback connection_status_callback,
    ReadCallback read_callback)
    : adapter_(adapter),
      address_(std::move(device_address)),
      connection_status_callback_(std::move(connection_status_callback)),
      read_callback_(std::move(read_callback)),
      weak_factory_(this) {
  DCHECK(adapter_);
  adapter_->AddObserver(this);
  DCHECK(!address_.empty());
}

FidoBleConnection::~FidoBleConnection() {
  adapter_->RemoveObserver(this);
}

const BluetoothDevice* FidoBleConnection::GetBleDevice() const {
  return adapter_->GetDevice(address());
}

FidoBleConnection::FidoBleConnection(BluetoothAdapter* adapter,
                                     std::string device_address)
    : adapter_(adapter),
      address_(std::move(device_address)),
      weak_factory_(this) {
  adapter_->AddObserver(this);
}

void FidoBleConnection::Connect() {
  BluetoothDevice* device = adapter_->GetDevice(address_);
  if (!device) {
    DLOG(ERROR) << "Failed to get Device.";
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&FidoBleConnection::OnConnectionError,
                                  weak_factory_.GetWeakPtr()));
    return;
  }

  device->CreateGattConnection(
      base::Bind(&FidoBleConnection::OnCreateGattConnection,
                 weak_factory_.GetWeakPtr()),
      base::Bind(&FidoBleConnection::OnCreateGattConnectionError,
                 weak_factory_.GetWeakPtr()));
}

void FidoBleConnection::ReadControlPointLength(
    ControlPointLengthCallback callback) {
  const BluetoothRemoteGattService* u2f_service = GetFidoService();
  if (!u2f_service) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), base::nullopt));
    return;
  }

  BluetoothRemoteGattCharacteristic* control_point_length =
      u2f_service->GetCharacteristic(*control_point_length_id_);
  if (!control_point_length) {
    DLOG(ERROR) << "No Control Point Length characteristic present.";
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), base::nullopt));
    return;
  }

  auto copyable_callback = base::AdaptCallbackForRepeating(std::move(callback));
  control_point_length->ReadRemoteCharacteristic(
      base::Bind(OnReadControlPointLength, copyable_callback),
      base::Bind(OnReadControlPointLengthError, copyable_callback));
}

void FidoBleConnection::WriteControlPoint(const std::vector<uint8_t>& data,
                                          WriteCallback callback) {
  const BluetoothRemoteGattService* u2f_service = GetFidoService();
  if (!u2f_service) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
    return;
  }

  BluetoothRemoteGattCharacteristic* control_point =
      u2f_service->GetCharacteristic(*control_point_id_);
  if (!control_point) {
    DLOG(ERROR) << "Control Point characteristic not present.";
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
    return;
  }

#if defined(OS_MACOSX)
  // Attempt a write without response for performance reasons. Fall back to a
  // confirmed write in case of failure, e.g. when the characteristic does not
  // provide the required property.
  if (control_point->WriteWithoutResponse(data)) {
    DVLOG(2) << "Write without response succeeded.";
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), true));
    return;
  }
#endif  // defined(OS_MACOSX)

  auto copyable_callback = base::AdaptCallbackForRepeating(std::move(callback));
  control_point->WriteRemoteCharacteristic(
      data, base::Bind(OnWriteRemoteCharacteristic, copyable_callback),
      base::Bind(OnWriteRemoteCharacteristicError, copyable_callback));
}

void FidoBleConnection::OnCreateGattConnection(
    std::unique_ptr<BluetoothGattConnection> connection) {
  connection_ = std::move(connection);

  BluetoothDevice* device = adapter_->GetDevice(address_);
  if (!device) {
    DLOG(ERROR) << "Failed to get Device.";
    OnConnectionError();
    return;
  }

  if (device->IsGattServicesDiscoveryComplete())
    ConnectToU2fService();
}

void FidoBleConnection::OnCreateGattConnectionError(
    BluetoothDevice::ConnectErrorCode error_code) {
  DLOG(ERROR) << "CreateGattConnection() failed: " << ToString(error_code);
  OnConnectionError();
}

void FidoBleConnection::ConnectToU2fService() {
  BluetoothDevice* device = adapter_->GetDevice(address_);
  if (!device) {
    DLOG(ERROR) << "Failed to get Device.";
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&FidoBleConnection::OnConnectionError,
                                  weak_factory_.GetWeakPtr()));
    return;
  }

  DCHECK(device->IsGattServicesDiscoveryComplete());
  const std::vector<BluetoothRemoteGattService*> services =
      device->GetGattServices();
  auto found =
      std::find_if(services.begin(), services.end(), [](const auto* service) {
        return service->GetUUID().canonical_value() == kFidoServiceUUID;
      });

  if (found == services.end()) {
    DLOG(ERROR) << "Failed to get U2F Service.";
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&FidoBleConnection::OnConnectionError,
                                  weak_factory_.GetWeakPtr()));
    return;
  }

  const BluetoothRemoteGattService* u2f_service = *found;
  u2f_service_id_ = u2f_service->GetIdentifier();
  DVLOG(2) << "Got U2F Service: " << *u2f_service_id_;

  for (const auto* characteristic : u2f_service->GetCharacteristics()) {
    // NOTE: Since GetUUID() returns a temporary |uuid| can't be a reference,
    // even though canonical_value() returns a const reference.
    const std::string uuid = characteristic->GetUUID().canonical_value();
    if (uuid == kFidoControlPointLengthUUID) {
      control_point_length_id_ = characteristic->GetIdentifier();
      DVLOG(2) << "Got U2F Control Point Length: " << *control_point_length_id_;
    } else if (uuid == kFidoControlPointUUID) {
      control_point_id_ = characteristic->GetIdentifier();
      DVLOG(2) << "Got U2F Control Point: " << *control_point_id_;
    } else if (uuid == kFidoStatusUUID) {
      status_id_ = characteristic->GetIdentifier();
      DVLOG(2) << "Got U2F Status: " << *status_id_;
    } else if (uuid == kFidoServiceRevisionUUID) {
      service_revision_id_ = characteristic->GetIdentifier();
      DVLOG(2) << "Got U2F Service Revision: " << *service_revision_id_;
    } else if (uuid == kFidoServiceRevisionBitfieldUUID) {
      service_revision_bitfield_id_ = characteristic->GetIdentifier();
      DVLOG(2) << "Got U2F Service Revision Bitfield: "
               << *service_revision_bitfield_id_;
    }
  }

  if (!control_point_length_id_ || !control_point_id_ || !status_id_ ||
      (!service_revision_id_ && !service_revision_bitfield_id_)) {
    DLOG(ERROR) << "U2F characteristics missing.";
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&FidoBleConnection::OnConnectionError,
                                  weak_factory_.GetWeakPtr()));
    return;
  }

  // In case the bitfield characteristic is present, the client has to select a
  // supported bersion by writing the corresponding bit. Reference:
  // https://fidoalliance.org/specs/fido-v2.0-rd-20180702/fido-client-to-authenticator-protocol-v2.0-rd-20180702.html#ble-protocol-overview
  if (service_revision_bitfield_id_) {
    auto callback = base::BindRepeating(
        &FidoBleConnection::OnReadServiceRevisions, weak_factory_.GetWeakPtr());
    u2f_service->GetCharacteristic(*service_revision_bitfield_id_)
        ->ReadRemoteCharacteristic(
            base::Bind(OnReadServiceRevisionBitfield, callback),
            base::Bind(OnReadServiceRevisionBitfieldError, callback));
    return;
  }

  StartNotifySession();
}

void FidoBleConnection::OnReadServiceRevisions(
    std::vector<ServiceRevision> service_revisions) {
  if (service_revisions.empty()) {
    VLOG(2) << "Error: Could not obtain Service Revisions.";
    OnConnectionError();
    return;
  }

  // Write the most recent supported service revision back to the
  // characteristic. Note that this information is currently not used in another
  // way, as we will still attempt a CTAP GetInfo() command, even if only U2F is
  // supported.
  // TODO(https://crbug.com/780078): Consider short circuiting to the
  // U2F logic if FIDO2 is not supported.
  DCHECK_EQ(
      *std::min_element(service_revisions.begin(), service_revisions.end()),
      service_revisions.back());
  WriteServiceRevision(service_revisions.back());
}

void FidoBleConnection::WriteServiceRevision(ServiceRevision service_revision) {
  auto callback = base::BindOnce(&FidoBleConnection::OnServiceRevisionWritten,
                                 weak_factory_.GetWeakPtr());

  const BluetoothRemoteGattService* fido_service = GetFidoService();
  if (!fido_service) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
    return;
  }

  auto copyable_callback = base::AdaptCallbackForRepeating(std::move(callback));
  DCHECK(service_revision_bitfield_id_);
  fido_service->GetCharacteristic(*service_revision_bitfield_id_)
      ->WriteRemoteCharacteristic(
          {static_cast<uint8_t>(service_revision)},
          base::Bind(OnWriteRemoteCharacteristic, copyable_callback),
          base::Bind(OnWriteRemoteCharacteristicError, copyable_callback));
}

void FidoBleConnection::OnServiceRevisionWritten(bool success) {
  if (success) {
    StartNotifySession();
    return;
  }

  OnConnectionError();
}

void FidoBleConnection::StartNotifySession() {
  const BluetoothRemoteGattService* fido_service = GetFidoService();
  if (!fido_service) {
    OnConnectionError();
    return;
  }

  DCHECK(status_id_);
  fido_service->GetCharacteristic(*status_id_)
      ->StartNotifySession(
          base::Bind(&FidoBleConnection::OnStartNotifySession,
                     weak_factory_.GetWeakPtr()),
          base::Bind(&FidoBleConnection::OnStartNotifySessionError,
                     weak_factory_.GetWeakPtr()));
}

void FidoBleConnection::OnStartNotifySession(
    std::unique_ptr<BluetoothGattNotifySession> notify_session) {
  notify_session_ = std::move(notify_session);
  DVLOG(2) << "Created notification session. Connection established.";
  connection_status_callback_.Run(true);
}

void FidoBleConnection::OnStartNotifySessionError(
    BluetoothGattService::GattErrorCode error_code) {
  DLOG(ERROR) << "StartNotifySession() failed: " << ToString(error_code);
  OnConnectionError();
}

void FidoBleConnection::OnConnectionError() {
  connection_status_callback_.Run(false);

  connection_.reset();
  notify_session_.reset();

  u2f_service_id_.reset();
  control_point_length_id_.reset();
  control_point_id_.reset();
  status_id_.reset();
  service_revision_id_.reset();
  service_revision_bitfield_id_.reset();
}

const BluetoothRemoteGattService* FidoBleConnection::GetFidoService() const {
  const BluetoothDevice* device = adapter_->GetDevice(address_);
  if (!device) {
    DLOG(ERROR) << "No device present.";
    return nullptr;
  }

  if (!u2f_service_id_) {
    DLOG(ERROR) << "Unknown U2F service id.";
    return nullptr;
  }

  const BluetoothRemoteGattService* u2f_service =
      device->GetGattService(*u2f_service_id_);
  if (!u2f_service) {
    DLOG(ERROR) << "No U2F service present.";
    return nullptr;
  }

  return u2f_service;
}

void FidoBleConnection::DeviceAdded(BluetoothAdapter* adapter,
                                    BluetoothDevice* device) {
  if (adapter != adapter_ || device->GetAddress() != address_)
    return;
  Connect();
}

void FidoBleConnection::DeviceAddressChanged(BluetoothAdapter* adapter,
                                             BluetoothDevice* device,
                                             const std::string& old_address) {
  if (adapter != adapter_ || old_address != address_)
    return;
  address_ = device->GetAddress();
}

void FidoBleConnection::DeviceChanged(BluetoothAdapter* adapter,
                                      BluetoothDevice* device) {
  if (adapter != adapter_ || device->GetAddress() != address_)
    return;
  if (connection_ && !device->IsGattConnected()) {
    DLOG(ERROR) << "GATT Disconnected: " << device->GetAddress();
    OnConnectionError();
  }
}

void FidoBleConnection::GattCharacteristicValueChanged(
    BluetoothAdapter* adapter,
    BluetoothRemoteGattCharacteristic* characteristic,
    const std::vector<uint8_t>& value) {
  if (characteristic->GetIdentifier() != status_id_)
    return;
  DVLOG(2) << "Status characteristic value changed.";
  read_callback_.Run(value);
}

void FidoBleConnection::GattServicesDiscovered(BluetoothAdapter* adapter,
                                               BluetoothDevice* device) {
  if (adapter != adapter_ || device->GetAddress() != address_)
    return;
  ConnectToU2fService();
}

// static
void FidoBleConnection::OnReadControlPointLength(
    ControlPointLengthCallback callback,
    const std::vector<uint8_t>& value) {
  if (value.size() != 2) {
    DLOG(ERROR) << "Wrong Control Point Length: " << value.size() << " bytes";
    std::move(callback).Run(base::nullopt);
    return;
  }

  uint16_t length = (value[0] << 8) | value[1];
  DVLOG(2) << "Control Point Length: " << length;
  std::move(callback).Run(length);
}

// static
void FidoBleConnection::OnReadControlPointLengthError(
    ControlPointLengthCallback callback,
    BluetoothGattService::GattErrorCode error_code) {
  DLOG(ERROR) << "Error reading Control Point Length: " << ToString(error_code);
  std::move(callback).Run(base::nullopt);
}

}  // namespace device
