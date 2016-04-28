// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUEZ_BLUETOOTH_LOCAL_GATT_CHARACTERISTIC_BLUEZ_H_
#define DEVICE_BLUETOOTH_BLUEZ_BLUETOOTH_LOCAL_GATT_CHARACTERISTIC_BLUEZ_H_

#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "device/bluetooth/bluetooth_local_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_local_gatt_service.h"
#include "device/bluetooth/bluetooth_uuid.h"
#include "device/bluetooth/bluez/bluetooth_gatt_characteristic_bluez.h"
#include "device/bluetooth/bluez/bluetooth_local_gatt_descriptor_bluez.h"

namespace bluez {

class BluetoothLocalGattServiceBlueZ;

// The BluetoothLocalGattCharacteristicBlueZ class implements
// BluetoothLocalGattCharacteristic for local GATT characteristics for
// platforms that use BlueZ.
class BluetoothLocalGattCharacteristicBlueZ
    : public BluetoothGattCharacteristicBlueZ,
      public device::BluetoothLocalGattCharacteristic {
 public:
  static base::WeakPtr<device::BluetoothLocalGattCharacteristic> Create(
      const device::BluetoothUUID& uuid,
      BluetoothGattCharacteristic::Properties properties,
      BluetoothGattCharacteristic::Permissions permissions,
      device::BluetoothLocalGattService* service);

  BluetoothLocalGattCharacteristicBlueZ(
      const device::BluetoothUUID& uuid,
      BluetoothLocalGattServiceBlueZ* service);
  ~BluetoothLocalGattCharacteristicBlueZ() override;

  // device::BluetoothLocalGattCharacteristic overrides.
  device::BluetoothUUID GetUUID() const override;
  Properties GetProperties() const override;
  Permissions GetPermissions() const override;

  BluetoothLocalGattServiceBlueZ* GetService();

  const std::vector<std::unique_ptr<BluetoothLocalGattDescriptorBlueZ>>&
  GetDescriptors() const;

 private:
  friend class BluetoothLocalGattDescriptorBlueZ;
  // Needs access to weak_ptr_factory_.
  friend device::BluetoothLocalGattCharacteristic;

  // Adds a descriptor to this characteristic.
  void AddDescriptor(
      std::unique_ptr<BluetoothLocalGattDescriptorBlueZ> descriptor);

  // UUID of this characteristic.
  device::BluetoothUUID uuid_;

  // Service that contains this characteristic.
  BluetoothLocalGattServiceBlueZ* service_;

  // Descriptors contained by this characteristic.
  std::vector<std::unique_ptr<BluetoothLocalGattDescriptorBlueZ>> descriptors_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<BluetoothLocalGattCharacteristicBlueZ> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothLocalGattCharacteristicBlueZ);
};

}  // namespace bluez

#endif  // DEVICE_BLUETOOTH_BLUEZ_BLUETOOTH_LOCAL_GATT_CHARACTERISTIC_BLUEZ_H_
