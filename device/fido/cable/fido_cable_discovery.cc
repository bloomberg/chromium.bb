// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/cable/fido_cable_discovery.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "base/strings/stringprintf.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "build/build_config.h"
#include "device/bluetooth/bluetooth_advertisement.h"
#include "device/bluetooth/bluetooth_discovery_session.h"
#include "device/bluetooth/bluetooth_uuid.h"
#include "device/fido/ble/fido_ble_uuids.h"
#include "device/fido/cable/fido_cable_device.h"
#include "device/fido/cable/fido_cable_handshake_handler.h"
#include "device/fido/fido_parsing_utils.h"

namespace device {

namespace {

const BluetoothUUID& CableAdvertisementUUID128() {
  static const BluetoothUUID service_uuid(kCableAdvertisementUUID128);
  return service_uuid;
}

const BluetoothUUID& CableAdvertisementUUID16() {
  static const BluetoothUUID service_uuid(kCableAdvertisementUUID16);
  return service_uuid;
}

bool IsCableDevice(const BluetoothDevice* device) {
  return base::ContainsKey(device->GetServiceData(),
                           CableAdvertisementUUID128()) ||
         base::ContainsKey(device->GetUUIDs(), CableAdvertisementUUID16());
}

// Construct advertisement data with different formats depending on client's
// operating system. Ideally, we advertise EIDs as part of Service Data, but
// this isn't available on all platforms. On Windows we use Manufacturer Data
// instead, and on Mac our only option is to advertise an additional service
// with the EID as its UUID.
std::unique_ptr<BluetoothAdvertisement::Data> ConstructAdvertisementData(
    uint8_t version_number,
    base::span<const uint8_t, FidoCableDiscovery::kEphemeralIdSize>
        client_eid) {
  auto advertisement_data = std::make_unique<BluetoothAdvertisement::Data>(
      BluetoothAdvertisement::AdvertisementType::ADVERTISEMENT_TYPE_BROADCAST);

#if defined(OS_MACOSX)
  auto list = std::make_unique<BluetoothAdvertisement::UUIDList>();
  list->emplace_back(kCableAdvertisementUUID16);
  list->emplace_back(fido_parsing_utils::ConvertBytesToUuid(client_eid));
  advertisement_data->set_service_uuids(std::move(list));

#elif defined(OS_WIN)
  constexpr uint16_t kFidoManufacturerId = 0xFFFD;
  constexpr std::array<uint8_t, 2> kFidoManufacturerDataHeader = {0x51, 0xFE};

  auto manufacturer_data =
      std::make_unique<BluetoothAdvertisement::ManufacturerData>();
  std::vector<uint8_t> manufacturer_data_value;
  fido_parsing_utils::Append(&manufacturer_data_value,
                             kFidoManufacturerDataHeader);
  fido_parsing_utils::Append(&manufacturer_data_value, client_eid);
  manufacturer_data->emplace(kFidoManufacturerId,
                             std::move(manufacturer_data_value));
  advertisement_data->set_manufacturer_data(std::move(manufacturer_data));

#elif defined(OS_LINUX) || defined(OS_CHROMEOS)
  // Service data for ChromeOS and Linux is 1 byte corresponding to Cable flags,
  // followed by 1 byte corresponding to Cable version number, followed by 16
  // bytes corresponding to client EID.
  auto service_data = std::make_unique<BluetoothAdvertisement::ServiceData>();
  std::vector<uint8_t> service_data_value(18, 0);
  // Since the remainder of this service data field is a Cable EID, set the 5th
  // bit of the flag byte.
  service_data_value[0] = 1 << 5;
  service_data_value[1] = version_number;
  std::copy(client_eid.begin(), client_eid.end(),
            service_data_value.begin() + 2);
  service_data->emplace(kCableAdvertisementUUID128,
                        std::move(service_data_value));
  advertisement_data->set_service_data(std::move(service_data));
#endif

  return advertisement_data;
}

}  // namespace

// FidoCableDiscovery::CableDiscoveryData -------------------------------------

FidoCableDiscovery::CableDiscoveryData::CableDiscoveryData(
    uint8_t version,
    const EidArray& client_eid,
    const EidArray& authenticator_eid,
    const SessionPreKeyArray& session_pre_key)
    : version(version),
      client_eid(client_eid),
      authenticator_eid(authenticator_eid),
      session_pre_key(session_pre_key) {}

FidoCableDiscovery::CableDiscoveryData::CableDiscoveryData(
    const CableDiscoveryData& data) = default;

FidoCableDiscovery::CableDiscoveryData& FidoCableDiscovery::CableDiscoveryData::
operator=(const CableDiscoveryData& other) = default;

FidoCableDiscovery::CableDiscoveryData::~CableDiscoveryData() = default;

// FidoCableDiscovery ---------------------------------------------------------

FidoCableDiscovery::FidoCableDiscovery(
    std::vector<CableDiscoveryData> discovery_data)
    : discovery_data_(std::move(discovery_data)), weak_factory_(this) {}

// This is a workaround for https://crbug.com/846522
FidoCableDiscovery::~FidoCableDiscovery() {
  for (auto advertisement : advertisements_)
    advertisement.second->Unregister(base::DoNothing(), base::DoNothing());
}

std::unique_ptr<FidoCableHandshakeHandler>
FidoCableDiscovery::CreateHandshakeHandler(
    FidoCableDevice* device,
    base::span<const uint8_t, kSessionPreKeySize> session_pre_key,
    base::span<const uint8_t, 8> nonce) {
  return std::make_unique<FidoCableHandshakeHandler>(device, nonce,
                                                     session_pre_key);
}

void FidoCableDiscovery::DeviceAdded(BluetoothAdapter* adapter,
                                     BluetoothDevice* device) {
  if (!IsCableDevice(device))
    return;

  DVLOG(2) << "Discovered Cable device: " << device->GetAddress();
  CableDeviceFound(adapter, device);
}

void FidoCableDiscovery::DeviceChanged(BluetoothAdapter* adapter,
                                       BluetoothDevice* device) {
  if (!IsCableDevice(device))
    return;

  DVLOG(2) << "Device changed for Cable device: " << device->GetAddress();
  CableDeviceFound(adapter, device);
}

void FidoCableDiscovery::DeviceRemoved(BluetoothAdapter* adapter,
                                       BluetoothDevice* device) {
  if (IsCableDevice(device) && GetFoundCableDiscoveryData(device)) {
    const auto& device_address = device->GetAddress();
    VLOG(2) << "Cable device removed: " << device_address;
    RemoveDevice(FidoBleDevice::GetId(device_address));
  }
}

void FidoCableDiscovery::OnSetPowered() {
  DCHECK(adapter());

  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&FidoCableDiscovery::StartCableDiscovery,
                                weak_factory_.GetWeakPtr()));
}

void FidoCableDiscovery::StartCableDiscovery() {
  // Error callback OnStartDiscoverySessionError() is defined in the base class
  // FidoBleDiscoveryBase.
  adapter()->StartDiscoverySessionWithFilter(
      std::make_unique<BluetoothDiscoveryFilter>(
          BluetoothTransport::BLUETOOTH_TRANSPORT_LE),
      base::AdaptCallbackForRepeating(
          base::BindOnce(&FidoCableDiscovery::OnStartDiscoverySessionWithFilter,
                         weak_factory_.GetWeakPtr())),
      base::AdaptCallbackForRepeating(
          base::BindOnce(&FidoCableDiscovery::OnStartDiscoverySessionError,
                         weak_factory_.GetWeakPtr())));
}

void FidoCableDiscovery::OnStartDiscoverySessionWithFilter(
    std::unique_ptr<BluetoothDiscoverySession> session) {
  SetDiscoverySession(std::move(session));
  DVLOG(2) << "Discovery session started.";
  StartAdvertisement();
}

void FidoCableDiscovery::StartAdvertisement() {
  DCHECK(adapter());

  for (const auto& data : discovery_data_) {
    adapter()->RegisterAdvertisement(
        ConstructAdvertisementData(data.version, data.client_eid),
        base::AdaptCallbackForRepeating(
            base::BindOnce(&FidoCableDiscovery::OnAdvertisementRegistered,
                           weak_factory_.GetWeakPtr(), data.client_eid)),
        base::AdaptCallbackForRepeating(
            base::BindOnce(&FidoCableDiscovery::OnAdvertisementRegisterError,
                           weak_factory_.GetWeakPtr())));
  }
}

void FidoCableDiscovery::StopAdvertisements(base::OnceClosure callback) {
  auto barrier_closure =
      base::BarrierClosure(advertisement_success_counter_, std::move(callback));
  for (auto advertisement : advertisements_)
    advertisement.second->Unregister(barrier_closure, base::DoNothing());
}

void FidoCableDiscovery::OnAdvertisementRegistered(
    const EidArray& client_eid,
    scoped_refptr<BluetoothAdvertisement> advertisement) {
  DVLOG(2) << "Advertisement registered.";
  advertisements_.emplace(client_eid, std::move(advertisement));
  RecordAdvertisementResult(true /* is_success */);
}

void FidoCableDiscovery::OnAdvertisementRegisterError(
    BluetoothAdvertisement::ErrorCode error_code) {
  DLOG(ERROR) << "Failed to register advertisement: " << error_code;
  RecordAdvertisementResult(false /* is_success */);
}

void FidoCableDiscovery::RecordAdvertisementResult(bool is_success) {
  // If at least one advertisement succeeds, then notify discovery start.
  if (is_success) {
    if (!advertisement_success_counter_++)
      NotifyDiscoveryStarted(true);
    return;
  }

  // No advertisements succeeded, no point in continuing with Cable discovery.
  if (++advertisement_failure_counter_ == discovery_data_.size())
    NotifyDiscoveryStarted(false);
}

void FidoCableDiscovery::CableDeviceFound(BluetoothAdapter* adapter,
                                          BluetoothDevice* device) {
  const auto* found_cable_device_data = GetFoundCableDiscoveryData(device);
  if (!found_cable_device_data)
    return;

  DVLOG(2) << "Found new Cable device.";
  // Nonce is embedded as first 8 bytes of client EID.
  std::array<uint8_t, 8> nonce;
  bool extract_success = fido_parsing_utils::ExtractArray(
      found_cable_device_data->client_eid, 0, &nonce);
  if (!extract_success)
    return;

  auto cable_device =
      std::make_unique<FidoCableDevice>(adapter, device->GetAddress());
  StopAdvertisements(
      base::BindOnce(&FidoCableDiscovery::ConductEncryptionHandshake,
                     weak_factory_.GetWeakPtr(), std::move(cable_device),
                     found_cable_device_data->session_pre_key, nonce));
}

void FidoCableDiscovery::ConductEncryptionHandshake(
    std::unique_ptr<FidoCableDevice> cable_device,
    base::span<const uint8_t, kSessionPreKeySize> session_pre_key,
    base::span<const uint8_t, 8> nonce) {
  // At most one handshake messages should be exchanged for each Cable device.
  if (base::ContainsKey(cable_handshake_handlers_, cable_device->GetId()))
    return;

  auto handshake_handler =
      CreateHandshakeHandler(cable_device.get(), session_pre_key, nonce);
  auto* const handshake_handler_ptr = handshake_handler.get();
  cable_handshake_handlers_.emplace(cable_device->GetId(),
                                    std::move(handshake_handler));

  handshake_handler_ptr->InitiateCableHandshake(
      base::BindOnce(&FidoCableDiscovery::ValidateAuthenticatorHandshakeMessage,
                     weak_factory_.GetWeakPtr(), std::move(cable_device),
                     handshake_handler_ptr));
}

void FidoCableDiscovery::ValidateAuthenticatorHandshakeMessage(
    std::unique_ptr<FidoCableDevice> cable_device,
    FidoCableHandshakeHandler* handshake_handler,
    base::Optional<std::vector<uint8_t>> handshake_response) {
  if (!handshake_response)
    return;

  if (handshake_handler->ValidateAuthenticatorHandshakeMessage(
          *handshake_response)) {
    DVLOG(2) << "Authenticator handshake validated";
    AddDevice(std::move(cable_device));
  } else {
    DVLOG(2) << "Authenticator handshake invalid";
  }
}

const FidoCableDiscovery::CableDiscoveryData*
FidoCableDiscovery::GetFoundCableDiscoveryData(
    const BluetoothDevice* device) const {
  const auto* discovery_data =
      GetFoundCableDiscoveryDataFromServiceData(device);
  if (discovery_data != nullptr) {
    return discovery_data;
  }

  return GetFoundCableDiscoveryDataFromServiceUUIDs(device);
}

const FidoCableDiscovery::CableDiscoveryData*
FidoCableDiscovery::GetFoundCableDiscoveryDataFromServiceData(
    const BluetoothDevice* device) const {
  const auto* service_data =
      device->GetServiceDataForUUID(CableAdvertisementUUID128());
  if (!service_data) {
    return nullptr;
  }
  DCHECK(service_data);

  // Received service data from authenticator must have a flag that signals that
  // the service data includes Cable EID.
  if (service_data->empty() || !(service_data->at(0) >> 5 & 1u))
    return nullptr;

  EidArray received_authenticator_eid;
  bool extract_success = fido_parsing_utils::ExtractArray(
      *service_data, 2, &received_authenticator_eid);
  if (!extract_success)
    return nullptr;

  auto discovery_data_iterator = std::find_if(
      discovery_data_.begin(), discovery_data_.end(),
      [&received_authenticator_eid](const auto& data) {
        return received_authenticator_eid == data.authenticator_eid;
      });

  return discovery_data_iterator != discovery_data_.end()
             ? &(*discovery_data_iterator)
             : nullptr;
}

const FidoCableDiscovery::CableDiscoveryData*
FidoCableDiscovery::GetFoundCableDiscoveryDataFromServiceUUIDs(
    const BluetoothDevice* device) const {
  const auto service_uuids = device->GetUUIDs();
  for (const auto& uuid : service_uuids) {
    if (uuid == CableAdvertisementUUID128() ||
        uuid == CableAdvertisementUUID16()) {
      continue;
    }
    auto discovery_data_iterator = std::find_if(
        discovery_data_.begin(), discovery_data_.end(),
        [&uuid](const auto& data) {
          std::string received_eid_string =
              fido_parsing_utils::ConvertBytesToUuid(data.authenticator_eid);
          return uuid == BluetoothUUID(received_eid_string);
        });
    if (discovery_data_iterator != discovery_data_.end()) {
      return &(*discovery_data_iterator);
    }
  }
  return nullptr;
}

}  // namespace device
