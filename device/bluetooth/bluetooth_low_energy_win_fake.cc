// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_low_energy_win_fake.h"

#include "base/strings/stringprintf.h"

namespace {
const char kPlatformNotSupported[] =
    "Bluetooth Low energy is only supported on Windows 8 and later.";
}  // namespace

namespace device {
namespace win {

BLEDevice::BLEDevice() {}
BLEDevice::~BLEDevice() {}

BLEGattService::BLEGattService() {}
BLEGattService::~BLEGattService() {}

BLEGattCharacteristic::BLEGattCharacteristic() {}
BLEGattCharacteristic::~BLEGattCharacteristic() {}

BLEGattDescriptor::BLEGattDescriptor() {}
BLEGattDescriptor::~BLEGattDescriptor() {}

BluetoothLowEnergyWrapperFake::BluetoothLowEnergyWrapperFake() {}
BluetoothLowEnergyWrapperFake::~BluetoothLowEnergyWrapperFake() {}

bool BluetoothLowEnergyWrapperFake::EnumerateKnownBluetoothLowEnergyDevices(
    ScopedVector<BluetoothLowEnergyDeviceInfo>* devices,
    std::string* error) {
  if (!IsBluetoothLowEnergySupported()) {
    *error = kPlatformNotSupported;
    return false;
  }

  for (auto& device : simulated_devices_) {
    BluetoothLowEnergyDeviceInfo* device_info =
        new BluetoothLowEnergyDeviceInfo();
    *device_info = *(device.second->device_info);
    devices->push_back(device_info);
  }
  return true;
}

bool BluetoothLowEnergyWrapperFake::
    EnumerateKnownBluetoothLowEnergyGattServiceDevices(
        ScopedVector<BluetoothLowEnergyDeviceInfo>* devices,
        std::string* error) {
  if (!IsBluetoothLowEnergySupported()) {
    *error = kPlatformNotSupported;
    return false;
  }

  for (auto& device : simulated_devices_) {
    for (auto& service : device.second->primary_services) {
      BluetoothLowEnergyDeviceInfo* device_info =
          new BluetoothLowEnergyDeviceInfo();
      *device_info = *(device.second->device_info);
      base::string16 path = GenerateBLEGattServiceDevicePath(
          device.second->device_info->path.value(),
          service.second->service_info->AttributeHandle);
      device_info->path = base::FilePath(path);
      devices->push_back(device_info);
    }
  }
  return true;
}

bool BluetoothLowEnergyWrapperFake::EnumerateKnownBluetoothLowEnergyServices(
    const base::FilePath& device_path,
    ScopedVector<BluetoothLowEnergyServiceInfo>* services,
    std::string* error) {
  if (!IsBluetoothLowEnergySupported()) {
    *error = kPlatformNotSupported;
    return false;
  }

  base::string16 device_address =
      ExtractDeviceAddressFromDevicePath(device_path.value());
  base::string16 service_attribute_handle =
      ExtractServiceAttributeHandleFromDevicePath(device_path.value());

  BLEDevicesMap::iterator it_d = simulated_devices_.find(
      std::string(device_address.begin(), device_address.end()));
  CHECK(it_d != simulated_devices_.end());

  // |service_attribute_handle| is empty means |device_path| is a BLE device
  // path, otherwise it is a BLE GATT service device path.
  if (service_attribute_handle.empty()) {
    // Return all primary services for BLE device.
    for (auto& primary_service : it_d->second->primary_services) {
      BluetoothLowEnergyServiceInfo* service_info =
          new BluetoothLowEnergyServiceInfo();
      service_info->uuid = primary_service.second->service_info->ServiceUuid;
      service_info->attribute_handle =
          primary_service.second->service_info->AttributeHandle;
      services->push_back(service_info);
    }
  } else {
    // Return corresponding GATT service for BLE GATT service device.
    BLEGattServicesMap::iterator it_s =
        it_d->second->primary_services.find(std::string(
            service_attribute_handle.begin(), service_attribute_handle.end()));
    CHECK(it_s != it_d->second->primary_services.end());
    BluetoothLowEnergyServiceInfo* service_info =
        new BluetoothLowEnergyServiceInfo();
    service_info->uuid = it_s->second->service_info->ServiceUuid;
    service_info->attribute_handle =
        it_s->second->service_info->AttributeHandle;
    services->push_back(service_info);
  }

  return true;
}

BLEDevice* BluetoothLowEnergyWrapperFake::SimulateBLEDevice(
    std::string device_name,
    BLUETOOTH_ADDRESS device_address) {
  BLEDevice* device = new BLEDevice();
  BluetoothLowEnergyDeviceInfo* device_info =
      new BluetoothLowEnergyDeviceInfo();
  std::string string_device_address =
      BluetoothAddressToCanonicalString(device_address);
  device_info->path =
      base::FilePath(GenerateBLEDevicePath(string_device_address));
  device_info->friendly_name = device_name;
  device_info->address = device_address;
  device->device_info.reset(device_info);
  simulated_devices_[string_device_address] = make_scoped_ptr(device);
  return device;
}

BLEGattService* BluetoothLowEnergyWrapperFake::SimulateBLEGattService(
    BLEDevice* device,
    std::string uuid) {
  CHECK(device);

  BLEGattService* service = new BLEGattService();
  PBTH_LE_GATT_SERVICE service_info = new BTH_LE_GATT_SERVICE[1];
  std::string string_device_address =
      BluetoothAddressToCanonicalString(device->device_info->address);
  service_info->AttributeHandle =
      GenerateAUniqueAttributeHandle(string_device_address);
  service_info->ServiceUuid = CanonicalStringToBTH_LE_UUID(uuid);
  service->service_info.reset(service_info);
  device->primary_services[std::to_string(service_info->AttributeHandle)] =
      make_scoped_ptr(service);
  return service;
}

BLEDevice* BluetoothLowEnergyWrapperFake::GetSimulatedBLEDevice(
    std::string device_address) {
  BLEDevicesMap::iterator it_d = simulated_devices_.find(device_address);
  if (it_d == simulated_devices_.end())
    return nullptr;
  return it_d->second.get();
}

USHORT BluetoothLowEnergyWrapperFake::GenerateAUniqueAttributeHandle(
    std::string device_address) {
  scoped_ptr<std::set<USHORT>>& set_of_ushort =
      attribute_handle_table_[device_address];
  if (set_of_ushort) {
    USHORT max_attribute_handle = *set_of_ushort->rbegin();
    if (max_attribute_handle < 0xFFFF) {
      USHORT new_attribute_handle = max_attribute_handle + 1;
      set_of_ushort->insert(new_attribute_handle);
      return new_attribute_handle;
    } else {
      USHORT i = 1;
      for (; i < 0xFFFF; i++) {
        if (set_of_ushort->find(i) == set_of_ushort->end())
          break;
      }
      if (i >= 0xFFFF)
        return 0;
      set_of_ushort->insert(i);
      return i;
    }
  }

  USHORT smallest_att_handle = 1;
  std::set<USHORT>* new_set = new std::set<USHORT>();
  new_set->insert(smallest_att_handle);
  set_of_ushort.reset(new_set);
  return smallest_att_handle;
}

base::string16 BluetoothLowEnergyWrapperFake::GenerateBLEDevicePath(
    std::string device_address) {
  return base::string16(device_address.begin(), device_address.end());
}

base::string16 BluetoothLowEnergyWrapperFake::GenerateBLEGattServiceDevicePath(
    base::string16 resident_device_path,
    USHORT service_attribute_handle) {
  std::string sub_path = std::to_string(service_attribute_handle);
  return resident_device_path + L"/" +
         base::string16(sub_path.begin(), sub_path.end());
}

base::string16
BluetoothLowEnergyWrapperFake::ExtractDeviceAddressFromDevicePath(
    base::string16 path) {
  std::size_t found = path.find('/');
  if (found != base::string16::npos) {
    return path.substr(0, found);
  }
  return path;
}

base::string16
BluetoothLowEnergyWrapperFake::ExtractServiceAttributeHandleFromDevicePath(
    base::string16 path) {
  std::size_t found = path.find('/');
  if (found == base::string16::npos)
    return base::string16();
  return path.substr(found + 1);
}

BTH_LE_UUID BluetoothLowEnergyWrapperFake::CanonicalStringToBTH_LE_UUID(
    std::string uuid) {
  BTH_LE_UUID win_uuid;
  // Only short UUIDs (4 hex digits) have beened used in BluetoothTest right
  // now. Need fix after using long UUIDs.
  win_uuid.IsShortUuid = true;
  unsigned int data[1];
  int result = sscanf_s(uuid.c_str(), "%04x", &data[0]);
  CHECK(result == 1);
  win_uuid.Value.ShortUuid = data[0];
  return win_uuid;
}

std::string BluetoothLowEnergyWrapperFake::BluetoothAddressToCanonicalString(
    const BLUETOOTH_ADDRESS& btha) {
  std::string result = base::StringPrintf(
      "%02X:%02X:%02X:%02X:%02X:%02X", btha.rgBytes[5], btha.rgBytes[4],
      btha.rgBytes[3], btha.rgBytes[2], btha.rgBytes[1], btha.rgBytes[0]);
  return result;
}

}  // namespace win
}  // namespace device
