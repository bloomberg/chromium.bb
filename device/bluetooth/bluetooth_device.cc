// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_device.h"

#include <iterator>
#include <memory>
#include <string>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_gatt_connection.h"
#include "device/bluetooth/bluetooth_remote_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_remote_gatt_descriptor.h"
#include "device/bluetooth/bluetooth_remote_gatt_service.h"
#include "device/bluetooth/string_util_icu.h"
#include "device/bluetooth/strings/grit/bluetooth_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace device {

BluetoothDevice::DeviceUUIDs::DeviceUUIDs() = default;

BluetoothDevice::DeviceUUIDs::~DeviceUUIDs() = default;

BluetoothDevice::DeviceUUIDs::DeviceUUIDs(const DeviceUUIDs& other) = default;

BluetoothDevice::DeviceUUIDs& BluetoothDevice::DeviceUUIDs::operator=(
    const DeviceUUIDs& other) = default;

void BluetoothDevice::DeviceUUIDs::ReplaceAdvertisedUUIDs(
    UUIDList new_advertised_uuids) {
  advertised_uuids_.clear();
  for (auto& it : new_advertised_uuids) {
    advertised_uuids_.insert(std::move(it));
  }
  UpdateDeviceUUIDs();
}

void BluetoothDevice::DeviceUUIDs::ClearAdvertisedUUIDs() {
  advertised_uuids_.clear();
  UpdateDeviceUUIDs();
}

void BluetoothDevice::DeviceUUIDs::ReplaceServiceUUIDs(
    const GattServiceMap& gatt_services) {
  service_uuids_.clear();
  for (const auto& gatt_service_pair : gatt_services)
    service_uuids_.insert(gatt_service_pair.second->GetUUID());
  UpdateDeviceUUIDs();
}

void BluetoothDevice::DeviceUUIDs::ClearServiceUUIDs() {
  service_uuids_.clear();
  UpdateDeviceUUIDs();
}

const BluetoothDevice::UUIDSet& BluetoothDevice::DeviceUUIDs::GetUUIDs() const {
  return device_uuids_;
}

void BluetoothDevice::DeviceUUIDs::UpdateDeviceUUIDs() {
  device_uuids_.clear();
  std::set_union(advertised_uuids_.begin(), advertised_uuids_.end(),
                 service_uuids_.begin(), service_uuids_.end(),
                 std::inserter(device_uuids_, device_uuids_.begin()));
}

BluetoothDevice::BluetoothDevice(BluetoothAdapter* adapter)
    : adapter_(adapter),
      gatt_services_discovery_complete_(false),
      last_update_time_(base::Time()) {}

BluetoothDevice::~BluetoothDevice() {
  for (BluetoothGattConnection* connection : gatt_connections_) {
    connection->InvalidateConnectionReference();
  }
}

BluetoothDevice::ConnectionInfo::ConnectionInfo()
    : rssi(kUnknownPower),
      transmit_power(kUnknownPower),
      max_transmit_power(kUnknownPower) {}

BluetoothDevice::ConnectionInfo::ConnectionInfo(
    int rssi, int transmit_power, int max_transmit_power)
    : rssi(rssi),
      transmit_power(transmit_power),
      max_transmit_power(max_transmit_power) {}

BluetoothDevice::ConnectionInfo::~ConnectionInfo() = default;

base::string16 BluetoothDevice::GetNameForDisplay() const {
  base::Optional<std::string> name = GetName();
  if (name && HasGraphicCharacter(name.value())) {
    return base::UTF8ToUTF16(name.value());
  } else {
    return GetAddressWithLocalizedDeviceTypeName();
  }
}

base::string16 BluetoothDevice::GetAddressWithLocalizedDeviceTypeName() const {
  base::string16 address_utf16 = base::UTF8ToUTF16(GetAddress());
  BluetoothDeviceType device_type = GetDeviceType();
  switch (device_type) {
    case BluetoothDeviceType::COMPUTER:
      return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_COMPUTER,
                                        address_utf16);
    case BluetoothDeviceType::PHONE:
      return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_PHONE,
                                        address_utf16);
    case BluetoothDeviceType::MODEM:
      return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_MODEM,
                                        address_utf16);
    case BluetoothDeviceType::AUDIO:
      return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_AUDIO,
                                        address_utf16);
    case BluetoothDeviceType::CAR_AUDIO:
      return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_CAR_AUDIO,
                                        address_utf16);
    case BluetoothDeviceType::VIDEO:
      return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_VIDEO,
                                        address_utf16);
    case BluetoothDeviceType::JOYSTICK:
      return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_JOYSTICK,
                                        address_utf16);
    case BluetoothDeviceType::GAMEPAD:
      return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_GAMEPAD,
                                        address_utf16);
    case BluetoothDeviceType::KEYBOARD:
      return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_KEYBOARD,
                                        address_utf16);
    case BluetoothDeviceType::MOUSE:
      return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_MOUSE,
                                        address_utf16);
    case BluetoothDeviceType::TABLET:
      return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_TABLET,
                                        address_utf16);
    case BluetoothDeviceType::KEYBOARD_MOUSE_COMBO:
      return l10n_util::GetStringFUTF16(
          IDS_BLUETOOTH_DEVICE_KEYBOARD_MOUSE_COMBO, address_utf16);
    default:
      return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_UNKNOWN,
                                        address_utf16);
  }
}

BluetoothDeviceType BluetoothDevice::GetDeviceType() const {
  // https://www.bluetooth.org/Technical/AssignedNumbers/baseband.htm
  uint32_t bluetooth_class = GetBluetoothClass();
  switch ((bluetooth_class & 0x1f00) >> 8) {
    case 0x01:
      // Computer major device class.
      return BluetoothDeviceType::COMPUTER;
    case 0x02:
      // Phone major device class.
      switch ((bluetooth_class & 0xfc) >> 2) {
        case 0x01:
        case 0x02:
        case 0x03:
          // Cellular, cordless and smart phones.
          return BluetoothDeviceType::PHONE;
        case 0x04:
        case 0x05:
          // Modems: wired or voice gateway and common ISDN access.
          return BluetoothDeviceType::MODEM;
      }
      break;
    case 0x04:
      // Audio major device class.
      switch ((bluetooth_class & 0xfc) >> 2) {
        case 0x08:
          // Car audio.
          return BluetoothDeviceType::CAR_AUDIO;
        case 0x0b:
        case 0x0c:
        case 0x0d:
        case 0x0e:
        case 0x0f:
        case 0x010:
          // Video devices.
          return BluetoothDeviceType::VIDEO;
        default:
          return BluetoothDeviceType::AUDIO;
      }
      break;
    case 0x05:
      // Peripheral major device class.
      switch ((bluetooth_class & 0xc0) >> 6) {
        case 0x00:
          // "Not a keyboard or pointing device."
          switch ((bluetooth_class & 0x01e) >> 2) {
            case 0x01:
              // Joystick.
              return BluetoothDeviceType::JOYSTICK;
            case 0x02:
              // Gamepad.
              return BluetoothDeviceType::GAMEPAD;
            default:
              return BluetoothDeviceType::PERIPHERAL;
          }
          break;
        case 0x01:
          // Keyboard.
          return BluetoothDeviceType::KEYBOARD;
        case 0x02:
          // Pointing device.
          switch ((bluetooth_class & 0x01e) >> 2) {
            case 0x05:
              // Digitizer tablet.
              return BluetoothDeviceType::TABLET;
            default:
              // Mouse.
              return BluetoothDeviceType::MOUSE;
          }
          break;
        case 0x03:
          // Combo device.
          return BluetoothDeviceType::KEYBOARD_MOUSE_COMBO;
      }
      break;
  }

  // Some bluetooth devices, e.g., Microsoft Universal Foldable Keyboard,
  // do not expose its bluetooth class. Use its appearance as a work-around.
  // https://developer.bluetooth.org/gatt/characteristics/Pages/CharacteristicViewer.aspx?u=org.bluetooth.characteristic.gap.appearance.xml
  uint16_t appearance = GetAppearance();
  // appearance: 10-bit category and 6-bit sub-category
  switch ((appearance & 0xffc0) >> 6) {
    case 0x01:
      // Generic phone
      return BluetoothDeviceType::PHONE;
    case 0x02:
      // Generic computer
      return BluetoothDeviceType::COMPUTER;
    case 0x0f:
      // HID subtype
      switch (appearance & 0x3f) {
        case 0x01:
          // Keyboard.
          return BluetoothDeviceType::KEYBOARD;
        case 0x02:
          // Mouse
          return BluetoothDeviceType::MOUSE;
        case 0x03:
          // Joystick
          return BluetoothDeviceType::JOYSTICK;
        case 0x04:
          // Gamepad
          return BluetoothDeviceType::GAMEPAD;
        case 0x05:
          // Digitizer tablet
          return BluetoothDeviceType::TABLET;
      }
  }

  return BluetoothDeviceType::UNKNOWN;
}

bool BluetoothDevice::IsPairable() const {
  BluetoothDeviceType type = GetDeviceType();

  // Get the vendor part of the address: "00:11:22" for "00:11:22:33:44:55"
  std::string vendor = GetAddress().substr(0, 8);

  // Verbatim "Bluetooth Mouse", model 96674
  if (type == BluetoothDeviceType::MOUSE && vendor == "00:12:A1")
    return false;
  // Microsoft "Microsoft Bluetooth Notebook Mouse 5000", model X807028-001
  if (type == BluetoothDeviceType::MOUSE && vendor == "7C:ED:8D")
    return false;

  // TODO: Move this database into a config file.

  return true;
}

BluetoothDevice::UUIDSet BluetoothDevice::GetUUIDs() const {
  return device_uuids_.GetUUIDs();
}

const BluetoothDevice::ServiceDataMap& BluetoothDevice::GetServiceData() const {
  return service_data_;
}

BluetoothDevice::UUIDSet BluetoothDevice::GetServiceDataUUIDs() const {
  UUIDSet service_data_uuids;
  for (const auto& uuid_service_data_pair : service_data_) {
    service_data_uuids.insert(uuid_service_data_pair.first);
  }
  return service_data_uuids;
}

const std::vector<uint8_t>* BluetoothDevice::GetServiceDataForUUID(
    const BluetoothUUID& uuid) const {
  auto it = service_data_.find(uuid);
  if (it != service_data_.end()) {
    return &it->second;
  }
  return nullptr;
}

const BluetoothDevice::ManufacturerDataMap&
BluetoothDevice::GetManufacturerData() const {
  return manufacturer_data_;
}

BluetoothDevice::ManufacturerIDSet BluetoothDevice::GetManufacturerDataIDs()
    const {
  ManufacturerIDSet manufacturer_data_ids;
  for (const auto& manufacturer_data_pair : manufacturer_data_) {
    manufacturer_data_ids.insert(manufacturer_data_pair.first);
  }
  return manufacturer_data_ids;
}

const std::vector<uint8_t>* BluetoothDevice::GetManufacturerDataForID(
    const ManufacturerId manufacturerID) const {
  auto it = manufacturer_data_.find(manufacturerID);
  if (it != manufacturer_data_.end()) {
    return &it->second;
  }
  return nullptr;
}

base::Optional<int8_t> BluetoothDevice::GetInquiryRSSI() const {
  return inquiry_rssi_;
}

base::Optional<uint8_t> BluetoothDevice::GetAdvertisingDataFlags() const {
  return advertising_data_flags_;
}

base::Optional<int8_t> BluetoothDevice::GetInquiryTxPower() const {
  return inquiry_tx_power_;
}

void BluetoothDevice::CreateGattConnection(
    const GattConnectionCallback& callback,
    const ConnectErrorCallback& error_callback) {
  create_gatt_connection_success_callbacks_.push_back(callback);
  create_gatt_connection_error_callbacks_.push_back(error_callback);

  if (IsGattConnected())
    return DidConnectGatt();

  CreateGattConnectionImpl();
}

void BluetoothDevice::SetGattServicesDiscoveryComplete(bool complete) {
  gatt_services_discovery_complete_ = complete;
}

bool BluetoothDevice::IsGattServicesDiscoveryComplete() const {
  return gatt_services_discovery_complete_;
}

std::vector<BluetoothRemoteGattService*> BluetoothDevice::GetGattServices()
    const {
  std::vector<BluetoothRemoteGattService*> services;
  for (const auto& iter : gatt_services_)
    services.push_back(iter.second.get());
  return services;
}

BluetoothRemoteGattService* BluetoothDevice::GetGattService(
    const std::string& identifier) const {
  auto it = gatt_services_.find(identifier);
  if (it == gatt_services_.end())
    return nullptr;
  return it->second.get();
}

// static
std::string BluetoothDevice::CanonicalizeAddress(const std::string& address) {
  std::string canonicalized = address;
  if (address.size() == 12) {
    // Might be an address in the format "1A2B3C4D5E6F". Add separators.
    for (size_t i = 2; i < canonicalized.size(); i += 3) {
      canonicalized.insert(i, ":");
    }
  }

  // Verify that the length matches the canonical format "1A:2B:3C:4D:5E:6F".
  const size_t kCanonicalAddressLength = 17;
  if (canonicalized.size() != kCanonicalAddressLength)
    return std::string();

  const char separator = canonicalized[2];
  for (size_t i = 0; i < canonicalized.size(); ++i) {
    bool is_separator = (i + 1) % 3 == 0;
    if (is_separator) {
      // All separators in the input |address| should be consistent.
      if (canonicalized[i] != separator)
        return std::string();

      canonicalized[i] = ':';
    } else {
      if (!base::IsHexDigit(canonicalized[i]))
        return std::string();

      canonicalized[i] = base::ToUpperASCII(canonicalized[i]);
    }
  }

  return canonicalized;
}

std::string BluetoothDevice::GetIdentifier() const { return GetAddress(); }

void BluetoothDevice::UpdateAdvertisementData(
    int8_t rssi,
    base::Optional<uint8_t> flags,
    UUIDList advertised_uuids,
    base::Optional<int8_t> tx_power,
    ServiceDataMap service_data,
    ManufacturerDataMap manufacturer_data) {
  UpdateTimestamp();

  inquiry_rssi_ = rssi;
  advertising_data_flags_ = std::move(flags);
  device_uuids_.ReplaceAdvertisedUUIDs(std::move(advertised_uuids));
  inquiry_tx_power_ = std::move(tx_power);
  service_data_ = std::move(service_data);
  manufacturer_data_ = std::move(manufacturer_data);
}

void BluetoothDevice::ClearAdvertisementData() {
  inquiry_rssi_.reset();
  advertising_data_flags_.reset();
  inquiry_tx_power_.reset();
  device_uuids_.ClearAdvertisedUUIDs();
  service_data_.clear();
  manufacturer_data_.clear();
  GetAdapter()->NotifyDeviceChanged(this);
}

std::vector<BluetoothRemoteGattService*> BluetoothDevice::GetPrimaryServices() {
  std::vector<BluetoothRemoteGattService*> services;
  VLOG(2) << "Looking for services.";
  for (BluetoothRemoteGattService* service : GetGattServices()) {
    VLOG(2) << "Service in cache: " << service->GetUUID().canonical_value();
    if (service->IsPrimary()) {
      services.push_back(service);
    }
  }
  return services;
}

std::vector<BluetoothRemoteGattService*>
BluetoothDevice::GetPrimaryServicesByUUID(const BluetoothUUID& service_uuid) {
  std::vector<BluetoothRemoteGattService*> services;
  VLOG(2) << "Looking for service: " << service_uuid.canonical_value();
  for (BluetoothRemoteGattService* service : GetGattServices()) {
    VLOG(2) << "Service in cache: " << service->GetUUID().canonical_value();
    if (service->GetUUID() == service_uuid && service->IsPrimary()) {
      services.push_back(service);
    }
  }
  return services;
}

void BluetoothDevice::DidConnectGatt() {
  for (const auto& callback : create_gatt_connection_success_callbacks_) {
    callback.Run(
        std::make_unique<BluetoothGattConnection>(adapter_, GetAddress()));
  }
  create_gatt_connection_success_callbacks_.clear();
  create_gatt_connection_error_callbacks_.clear();
  GetAdapter()->NotifyDeviceChanged(this);
}

void BluetoothDevice::DidFailToConnectGatt(ConnectErrorCode error) {
  // Connection request should only be made if there are no active
  // connections.
  DCHECK(gatt_connections_.empty());

  for (const auto& error_callback : create_gatt_connection_error_callbacks_)
    error_callback.Run(error);
  create_gatt_connection_success_callbacks_.clear();
  create_gatt_connection_error_callbacks_.clear();
}

void BluetoothDevice::DidDisconnectGatt() {
  // Pending calls to connect GATT are not expected, if they were then
  // DidFailToConnectGatt should have been called.
  DCHECK(create_gatt_connection_error_callbacks_.empty());

  // Invalidate all BluetoothGattConnection objects.
  for (BluetoothGattConnection* connection : gatt_connections_) {
    connection->InvalidateConnectionReference();
  }
  gatt_connections_.clear();
  GetAdapter()->NotifyDeviceChanged(this);
}

void BluetoothDevice::AddGattConnection(BluetoothGattConnection* connection) {
  auto result = gatt_connections_.insert(connection);
  DCHECK(result.second);  // Check insert happened; there was no duplicate.
}

void BluetoothDevice::RemoveGattConnection(
    BluetoothGattConnection* connection) {
  size_t erased_count = gatt_connections_.erase(connection);
  DCHECK(erased_count);
  if (gatt_connections_.size() == 0)
    DisconnectGatt();
}

void BluetoothDevice::SetAsExpiredForTesting() {
  last_update_time_ =
      base::Time::NowFromSystemTime() -
      (BluetoothAdapter::timeoutSec + base::TimeDelta::FromSeconds(1));
}

void BluetoothDevice::Pair(PairingDelegate* pairing_delegate,
                           const base::Closure& callback,
                           const ConnectErrorCallback& error_callback) {
  NOTREACHED();
}

void BluetoothDevice::UpdateTimestamp() {
  last_update_time_ = base::Time::NowFromSystemTime();
}

base::Time BluetoothDevice::GetLastUpdateTime() const {
  return last_update_time_;
}

// static
int8_t BluetoothDevice::ClampPower(int power) {
  if (power < INT8_MIN) {
    return INT8_MIN;
  }
  if (power > INT8_MAX) {
    return INT8_MAX;
  }
  return static_cast<int8_t>(power);
}

}  // namespace device
