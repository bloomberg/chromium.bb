// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_remote_gatt_service_chromeos.h"

#include "base/logging.h"
#include "chromeos/dbus/bluetooth_gatt_service_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"

namespace chromeos {

BluetoothRemoteGattServiceChromeOS::BluetoothRemoteGattServiceChromeOS(
    BluetoothDeviceChromeOS* device,
    const dbus::ObjectPath& object_path)
    : object_path_(object_path),
      device_(device),
      weak_ptr_factory_(this) {
  VLOG(1) << "Creating remote GATT service with identifier: "
          << object_path.value() << ", UUID: " << GetUUID().canonical_value();
  DBusThreadManager::Get()->GetBluetoothGattServiceClient()->AddObserver(this);
}

BluetoothRemoteGattServiceChromeOS::~BluetoothRemoteGattServiceChromeOS() {
  DBusThreadManager::Get()->GetBluetoothGattServiceClient()->
      RemoveObserver(this);
}

std::string BluetoothRemoteGattServiceChromeOS::GetIdentifier() const {
  return object_path_.value();
}

device::BluetoothUUID BluetoothRemoteGattServiceChromeOS::GetUUID() const {
  BluetoothGattServiceClient::Properties* properties =
      DBusThreadManager::Get()->GetBluetoothGattServiceClient()->
          GetProperties(object_path_);
  DCHECK(properties);
  return device::BluetoothUUID(properties->uuid.value());
}

bool BluetoothRemoteGattServiceChromeOS::IsLocal() const {
  return false;
}

bool BluetoothRemoteGattServiceChromeOS::IsPrimary() const {
  BluetoothGattServiceClient::Properties* properties =
      DBusThreadManager::Get()->GetBluetoothGattServiceClient()->
          GetProperties(object_path_);
  DCHECK(properties);
  return properties->primary.value();
}

const std::vector<device::BluetoothGattCharacteristic*>&
BluetoothRemoteGattServiceChromeOS::GetCharacteristics() const {
  return characteristics_;
}

const std::vector<device::BluetoothGattService*>&
BluetoothRemoteGattServiceChromeOS::GetIncludedServices() const {
  return includes_;
}

void BluetoothRemoteGattServiceChromeOS::AddObserver(
    device::BluetoothGattService::Observer* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void BluetoothRemoteGattServiceChromeOS::RemoveObserver(
    device::BluetoothGattService::Observer* observer) {
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

bool BluetoothRemoteGattServiceChromeOS::AddCharacteristic(
    device::BluetoothGattCharacteristic* characteristic) {
  VLOG(1) << "Characteristics cannot be added to a remote GATT service.";
  return false;
}

bool BluetoothRemoteGattServiceChromeOS::AddIncludedService(
    device::BluetoothGattService* service) {
  VLOG(1) << "Included services cannot be added to a remote GATT service.";
  return false;
}

void BluetoothRemoteGattServiceChromeOS::Register(
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  VLOG(1) << "A remote GATT service cannot be registered.";
  error_callback.Run();
}

void BluetoothRemoteGattServiceChromeOS::Unregister(
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  VLOG(1) << "A remote GATT service cannot be unregistered.";
  error_callback.Run();
}

void BluetoothRemoteGattServiceChromeOS::GattServicePropertyChanged(
    const dbus::ObjectPath& object_path,
    const std::string& property_name){
  FOR_EACH_OBSERVER(device::BluetoothGattService::Observer, observers_,
                    GattServiceChanged(this));
}

}  // namespace chromeos
