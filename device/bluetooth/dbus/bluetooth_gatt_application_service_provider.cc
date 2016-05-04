// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/dbus/bluetooth_gatt_application_service_provider.h"

#include <string>
#include <utility>
#include <vector>

#include "base/memory/ptr_util.h"
#include "device/bluetooth/bluetooth_uuid.h"
#include "device/bluetooth/bluez/bluetooth_gatt_service_bluez.h"
#include "device/bluetooth/dbus/bluetooth_gatt_application_service_provider_impl.h"
#include "device/bluetooth/dbus/bluetooth_gatt_characteristic_delegate_wrapper.h"
#include "device/bluetooth/dbus/bluetooth_gatt_characteristic_service_provider.h"
#include "device/bluetooth/dbus/bluetooth_gatt_descriptor_delegate_wrapper.h"
#include "device/bluetooth/dbus/bluetooth_gatt_descriptor_service_provider.h"
#include "device/bluetooth/dbus/bluetooth_gatt_service_service_provider.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "device/bluetooth/dbus/fake_bluetooth_gatt_application_service_provider.h"

namespace bluez {

BluetoothGattApplicationServiceProvider::
    BluetoothGattApplicationServiceProvider() {}

BluetoothGattApplicationServiceProvider::
    ~BluetoothGattApplicationServiceProvider() {}

// static
void BluetoothGattApplicationServiceProvider::CreateAttributeServiceProviders(
    dbus::Bus* bus,
    const std::map<dbus::ObjectPath, BluetoothLocalGattServiceBlueZ*>& services,
    std::vector<std::unique_ptr<BluetoothGattServiceServiceProvider>>*
        service_providers,
    std::vector<std::unique_ptr<BluetoothGattCharacteristicServiceProvider>>*
        characteristic_providers,
    std::vector<std::unique_ptr<BluetoothGattDescriptorServiceProvider>>*
        descriptor_providers) {
  for (const auto& service : services) {
    service_providers->push_back(
        base::WrapUnique(BluetoothGattServiceServiceProvider::Create(
            bus, service.second->object_path(),
            service.second->GetUUID().value(), service.second->IsPrimary(),
            std::vector<dbus::ObjectPath>())));
    for (const auto& characteristic : service.second->GetCharacteristics()) {
      characteristic_providers->push_back(
          base::WrapUnique(BluetoothGattCharacteristicServiceProvider::Create(
              bus, characteristic.second->object_path(),
              base::WrapUnique(new BluetoothGattCharacteristicDelegateWrapper(
                  service.second, characteristic.second.get())),
              characteristic.second->GetUUID().value(),
              std::vector<std::string>(), std::vector<std::string>(),
              service.second->object_path())));
      for (const auto& descriptor : characteristic.second->GetDescriptors()) {
        descriptor_providers->push_back(
            base::WrapUnique(BluetoothGattDescriptorServiceProvider::Create(
                bus, descriptor->object_path(),
                base::WrapUnique(new BluetoothGattDescriptorDelegateWrapper(
                    service.second, descriptor.get())),
                descriptor->GetUUID().value(), std::vector<std::string>(),
                characteristic.second->object_path())));
      }
    }
  }
}

// static
std::unique_ptr<BluetoothGattApplicationServiceProvider>
BluetoothGattApplicationServiceProvider::Create(
    dbus::Bus* bus,
    const dbus::ObjectPath& object_path,
    const std::map<dbus::ObjectPath, BluetoothLocalGattServiceBlueZ*>&
        services) {
  if (!bluez::BluezDBusManager::Get()->IsUsingFakes()) {
    return base::WrapUnique(new BluetoothGattApplicationServiceProviderImpl(
        bus, object_path, services));
  }
  return base::WrapUnique(
      new FakeBluetoothGattApplicationServiceProvider(object_path, services));
}

}  // namespace bluez
