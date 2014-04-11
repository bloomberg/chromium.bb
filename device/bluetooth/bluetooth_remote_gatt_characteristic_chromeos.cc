// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_remote_gatt_characteristic_chromeos.h"

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "chromeos/dbus/bluetooth_gatt_characteristic_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "device/bluetooth/bluetooth_remote_gatt_descriptor_chromeos.h"
#include "device/bluetooth/bluetooth_remote_gatt_service_chromeos.h"

namespace chromeos {

namespace {

// Stream operator for logging vector<uint8>.
std::ostream& operator<<(std::ostream& out, const std::vector<uint8> bytes) {
  out << "[";
  for (std::vector<uint8>::const_iterator iter = bytes.begin();
       iter != bytes.end(); ++iter) {
    out << base::StringPrintf("%02X", *iter);
  }
  return out << "]";
}

}  // namespace

BluetoothRemoteGattCharacteristicChromeOS::
    BluetoothRemoteGattCharacteristicChromeOS(
        BluetoothRemoteGattServiceChromeOS* service,
        const dbus::ObjectPath& object_path)
        : object_path_(object_path),
          service_(service),
          weak_ptr_factory_(this) {
  VLOG(1) << "Creating remote GATT characteristic with identifier: "
          << GetIdentifier() << ", UUID: " << GetUUID().canonical_value();
  DBusThreadManager::Get()->GetBluetoothGattDescriptorClient()->
      AddObserver(this);

  // Add all known GATT characteristic descriptors.
  const std::vector<dbus::ObjectPath>& gatt_descs =
      DBusThreadManager::Get()->GetBluetoothGattDescriptorClient()->
          GetDescriptors();
  for (std::vector<dbus::ObjectPath>::const_iterator iter = gatt_descs.begin();
       iter != gatt_descs.end(); ++iter)
    GattDescriptorAdded(*iter);
}

BluetoothRemoteGattCharacteristicChromeOS::
    ~BluetoothRemoteGattCharacteristicChromeOS() {
  DBusThreadManager::Get()->GetBluetoothGattDescriptorClient()->
      RemoveObserver(this);

  // Clean up all the descriptors. There isn't much point in notifying service
  // observers for each descriptor that gets removed, so just delete them.
  for (DescriptorMap::iterator iter = descriptors_.begin();
       iter != descriptors_.end(); ++iter)
    delete iter->second;
}

std::string BluetoothRemoteGattCharacteristicChromeOS::GetIdentifier() const {
  return object_path_.value();
}

device::BluetoothUUID
BluetoothRemoteGattCharacteristicChromeOS::GetUUID() const {
  BluetoothGattCharacteristicClient::Properties* properties =
      DBusThreadManager::Get()->GetBluetoothGattCharacteristicClient()->
          GetProperties(object_path_);
  DCHECK(properties);
  return device::BluetoothUUID(properties->uuid.value());
}

bool BluetoothRemoteGattCharacteristicChromeOS::IsLocal() const {
  return false;
}

const std::vector<uint8>&
BluetoothRemoteGattCharacteristicChromeOS::GetValue() const {
  BluetoothGattCharacteristicClient::Properties* properties =
      DBusThreadManager::Get()->GetBluetoothGattCharacteristicClient()->
          GetProperties(object_path_);
  DCHECK(properties);
  return properties->value.value();
}

device::BluetoothGattService*
BluetoothRemoteGattCharacteristicChromeOS::GetService() const {
  return service_;
}

device::BluetoothGattCharacteristic::Properties
BluetoothRemoteGattCharacteristicChromeOS::GetProperties() const {
  // TODO(armansito): Once BlueZ implements properties properly, return those
  // values here.
  return kPropertyNone;
}

device::BluetoothGattCharacteristic::Permissions
BluetoothRemoteGattCharacteristicChromeOS::GetPermissions() const {
  // TODO(armansito): Once BlueZ defines the permissions, return the correct
  // values here.
  return kPermissionNone;
}

std::vector<device::BluetoothGattDescriptor*>
BluetoothRemoteGattCharacteristicChromeOS::GetDescriptors() const {
  std::vector<device::BluetoothGattDescriptor*> descriptors;
  for (DescriptorMap::const_iterator iter = descriptors_.begin();
       iter != descriptors_.end(); ++iter)
    descriptors.push_back(iter->second);
  return descriptors;
}

bool BluetoothRemoteGattCharacteristicChromeOS::AddDescriptor(
    device::BluetoothGattDescriptor* descriptor) {
  VLOG(1) << "Descriptors cannot be added to a remote GATT characteristic.";
  return false;
}

bool BluetoothRemoteGattCharacteristicChromeOS::UpdateValue(
    const std::vector<uint8>& value) {
  VLOG(1) << "Cannot update the value of a remote GATT characteristic.";
  return false;
}

void BluetoothRemoteGattCharacteristicChromeOS::ReadRemoteCharacteristic(
    const ValueCallback& callback,
    const ErrorCallback& error_callback) {
  VLOG(1) << "Sending GATT characteristic read request to characteristic: "
          << GetIdentifier() << ", UUID: " << GetUUID().canonical_value()
          << ".";
  BluetoothGattCharacteristicClient::Properties* properties =
      DBusThreadManager::Get()->GetBluetoothGattCharacteristicClient()->
          GetProperties(object_path_);
  DCHECK(properties);
  properties->value.Get(
      base::Bind(&BluetoothRemoteGattCharacteristicChromeOS::OnGetValue,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback, error_callback));
}

void BluetoothRemoteGattCharacteristicChromeOS::WriteRemoteCharacteristic(
    const std::vector<uint8>& new_value,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  VLOG(1) << "Sending GATT characteristic write request to characteristic: "
          << GetIdentifier() << ", UUID: " << GetUUID().canonical_value()
          << ", with value: " << new_value << ".";

  // Permission and bonding are handled by BlueZ so no need check it here.
  if (new_value.empty()) {
    VLOG(1) << "Nothing to write.";
    error_callback.Run();
    return;
  }

  BluetoothGattCharacteristicClient::Properties* properties =
      DBusThreadManager::Get()->GetBluetoothGattCharacteristicClient()->
          GetProperties(object_path_);
  DCHECK(properties);
  properties->value.Set(
      new_value,
      base::Bind(&BluetoothRemoteGattCharacteristicChromeOS::OnSetValue,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback, error_callback));
}

void BluetoothRemoteGattCharacteristicChromeOS::GattDescriptorAdded(
    const dbus::ObjectPath& object_path) {
  if (descriptors_.find(object_path) != descriptors_.end()) {
    VLOG(1) << "Remote GATT characteristic descriptor already exists: "
            << object_path.value();
    return;
  }

  BluetoothGattDescriptorClient::Properties* properties =
      DBusThreadManager::Get()->GetBluetoothGattDescriptorClient()->
          GetProperties(object_path);
  DCHECK(properties);
  if (properties->characteristic.value() != object_path_) {
    VLOG(2) << "Remote GATT descriptor does not belong to this characteristic.";
    return;
  }

  VLOG(1) << "Adding new remote GATT descriptor for GATT characteristic: "
          << GetIdentifier() << ", UUID: " << GetUUID().canonical_value();

  BluetoothRemoteGattDescriptorChromeOS* descriptor =
      new BluetoothRemoteGattDescriptorChromeOS(this, object_path);
  descriptors_[object_path] = descriptor;
  DCHECK(descriptor->GetIdentifier() == object_path.value());
  DCHECK(descriptor->GetUUID().IsValid());
  DCHECK(service_);
  service_->NotifyServiceChanged();
}

void BluetoothRemoteGattCharacteristicChromeOS::GattDescriptorRemoved(
    const dbus::ObjectPath& object_path) {
  DescriptorMap::iterator iter = descriptors_.find(object_path);
  if (iter == descriptors_.end()) {
    VLOG(2) << "Unknown descriptor removed: " << object_path.value();
    return;
  }

  VLOG(1) << "Removing remote GATT descriptor from characteristic: "
          << GetIdentifier() << ", UUID: " << GetUUID().canonical_value();

  BluetoothRemoteGattDescriptorChromeOS* descriptor = iter->second;
  DCHECK(descriptor->object_path() == object_path);
  descriptors_.erase(iter);
  delete descriptor;

  DCHECK(service_);
  service_->NotifyServiceChanged();
}

void BluetoothRemoteGattCharacteristicChromeOS::GattDescriptorPropertyChanged(
    const dbus::ObjectPath& object_path,
    const std::string& property_name) {
  DescriptorMap::const_iterator iter = descriptors_.find(object_path);
  if (iter == descriptors_.end())
    return;

  VLOG(1) << "GATT descriptor property changed: " << object_path.value()
          << ", property: " << property_name;
}

void BluetoothRemoteGattCharacteristicChromeOS::OnGetValue(
    const ValueCallback& callback,
    const ErrorCallback& error_callback,
    bool success) {
  if (!success) {
    VLOG(1) << "Failed to read the value from the remote characteristic.";
    error_callback.Run();
    return;
  }

  VLOG(1) << "Read value of remote characteristic.";
  BluetoothGattCharacteristicClient::Properties* properties =
      DBusThreadManager::Get()->GetBluetoothGattCharacteristicClient()->
          GetProperties(object_path_);
  DCHECK(properties);
  callback.Run(properties->value.value());
}

void BluetoothRemoteGattCharacteristicChromeOS::OnSetValue(
    const base::Closure& callback,
    const ErrorCallback& error_callback,
    bool success) {
  if (!success) {
    VLOG(1) << "Failed to write the value of remote characteristic.";
    error_callback.Run();
    return;
  }

  VLOG(1) << "Wrote value of remote characteristic.";
  callback.Run();
}

}  // namespace chromeos
