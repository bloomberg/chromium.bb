// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/dbus/fake_bluetooth_gatt_manager_client.h"

#include "base/logging.h"
#include "device/bluetooth/dbus/fake_bluetooth_gatt_characteristic_service_provider.h"
#include "device/bluetooth/dbus/fake_bluetooth_gatt_descriptor_service_provider.h"
#include "device/bluetooth/dbus/fake_bluetooth_gatt_service_service_provider.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace bluez {

FakeBluetoothGattManagerClient::FakeBluetoothGattManagerClient() {}

FakeBluetoothGattManagerClient::~FakeBluetoothGattManagerClient() {}

// DBusClient override.
void FakeBluetoothGattManagerClient::Init(dbus::Bus* bus) {}

// BluetoothGattManagerClient overrides.
void FakeBluetoothGattManagerClient::RegisterService(
    const dbus::ObjectPath& service_path,
    const Options& options,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  VLOG(1) << "Register GATT service: " << service_path.value();

  // If a service provider wasn't created before, return error.
  ServiceMap::iterator iter = service_map_.find(service_path);
  if (iter == service_map_.end()) {
    error_callback.Run(bluetooth_gatt_manager::kErrorInvalidArguments,
                       "GATT service doesn't exist: " + service_path.value());
    return;
  }

  // Check to see if this GATT service was already registered.
  ServiceProvider* provider = &iter->second;
  if (provider->first) {
    error_callback.Run(
        bluetooth_gatt_manager::kErrorAlreadyExists,
        "GATT service already registered: " + service_path.value());
    return;
  }

  // Success!
  provider->first = true;
  callback.Run();
}

void FakeBluetoothGattManagerClient::UnregisterService(
    const dbus::ObjectPath& service_path,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  VLOG(1) << "Unregister GATT service: " << service_path.value();

  // If a service provider wasn't created before, return error.
  ServiceMap::iterator iter = service_map_.find(service_path);
  if (iter == service_map_.end()) {
    error_callback.Run(bluetooth_gatt_manager::kErrorInvalidArguments,
                       "GATT service doesn't exist: " + service_path.value());
    return;
  }

  // Return error if the GATT service wasn't registered before.
  ServiceProvider* provider = &iter->second;
  if (!provider->first) {
    error_callback.Run(bluetooth_gatt_manager::kErrorDoesNotExist,
                       "GATT service not registered: " + service_path.value());
    return;
  }

  // Success!
  provider->first = false;
  callback.Run();
}

void FakeBluetoothGattManagerClient::RegisterServiceServiceProvider(
    FakeBluetoothGattServiceServiceProvider* provider) {
  // Ignore, if a service provider is already registered for the object path.
  ServiceMap::iterator iter = service_map_.find(provider->object_path());
  if (iter != service_map_.end()) {
    VLOG(1) << "GATT service service provider already registered for "
            << "object path: " << provider->object_path().value();
    return;
  }
  service_map_[provider->object_path()] = std::make_pair(false, provider);
}

void FakeBluetoothGattManagerClient::RegisterCharacteristicServiceProvider(
    FakeBluetoothGattCharacteristicServiceProvider* provider) {
  // Ignore, if a service provider is already registered for the object path.
  CharacteristicMap::iterator iter =
      characteristic_map_.find(provider->object_path());
  if (iter != characteristic_map_.end()) {
    VLOG(1) << "GATT characteristic service provider already registered for "
            << "object path: " << provider->object_path().value();
    return;
  }
  characteristic_map_[provider->object_path()] = provider;
}

void FakeBluetoothGattManagerClient::RegisterDescriptorServiceProvider(
    FakeBluetoothGattDescriptorServiceProvider* provider) {
  // Ignore, if a service provider is already registered for the object path.
  DescriptorMap::iterator iter = descriptor_map_.find(provider->object_path());
  if (iter != descriptor_map_.end()) {
    VLOG(1) << "GATT descriptor service provider already registered for "
            << "object path: " << provider->object_path().value();
    return;
  }
  descriptor_map_[provider->object_path()] = provider;
}

void FakeBluetoothGattManagerClient::UnregisterServiceServiceProvider(
    FakeBluetoothGattServiceServiceProvider* provider) {
  ServiceMap::iterator iter = service_map_.find(provider->object_path());
  if (iter != service_map_.end() && iter->second.second == provider)
    service_map_.erase(iter);
}

void FakeBluetoothGattManagerClient::UnregisterCharacteristicServiceProvider(
    FakeBluetoothGattCharacteristicServiceProvider* provider) {
  characteristic_map_.erase(provider->object_path());
}

void FakeBluetoothGattManagerClient::UnregisterDescriptorServiceProvider(
    FakeBluetoothGattDescriptorServiceProvider* provider) {
  descriptor_map_.erase(provider->object_path());
}

FakeBluetoothGattServiceServiceProvider*
FakeBluetoothGattManagerClient::GetServiceServiceProvider(
    const dbus::ObjectPath& object_path) const {
  ServiceMap::const_iterator iter = service_map_.find(object_path);
  if (iter == service_map_.end())
    return NULL;
  return iter->second.second;
}

FakeBluetoothGattCharacteristicServiceProvider*
FakeBluetoothGattManagerClient::GetCharacteristicServiceProvider(
    const dbus::ObjectPath& object_path) const {
  CharacteristicMap::const_iterator iter =
      characteristic_map_.find(object_path);
  if (iter == characteristic_map_.end())
    return NULL;
  return iter->second;
}

FakeBluetoothGattDescriptorServiceProvider*
FakeBluetoothGattManagerClient::GetDescriptorServiceProvider(
    const dbus::ObjectPath& object_path) const {
  DescriptorMap::const_iterator iter = descriptor_map_.find(object_path);
  if (iter == descriptor_map_.end())
    return NULL;
  return iter->second;
}

bool FakeBluetoothGattManagerClient::IsServiceRegistered(
    const dbus::ObjectPath& object_path) const {
  ServiceMap::const_iterator iter = service_map_.find(object_path);
  if (iter == service_map_.end())
    return false;
  return iter->second.first;
}

}  // namespace bluez
