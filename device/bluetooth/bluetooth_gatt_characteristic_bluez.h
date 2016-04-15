// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_GATT_CHARACTERISTIC_BLUEZ_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_GATT_CHARACTERISTIC_BLUEZ_H_

#include <map>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluetooth_gatt_characteristic.h"

namespace device {

class BluetoothGattDescriptor;
class BluetoothGattService;

}  // namespace device

namespace bluez {

class BluetoothGattDescriptorBlueZ;
class BluetoothGattServiceBlueZ;

// The BluetoothGattCharacteristicBlueZ class implements
// BluetoothGattCharacteristic for GATT characteristics for platforms
// that use BlueZ.
class BluetoothGattCharacteristicBlueZ
    : public device::BluetoothGattCharacteristic {
 public:
  // device::BluetoothGattCharacteristic overrides.
  std::string GetIdentifier() const override;
  device::BluetoothGattService* GetService() const override;
  Permissions GetPermissions() const override;
  std::vector<device::BluetoothGattDescriptor*> GetDescriptors() const override;
  device::BluetoothGattDescriptor* GetDescriptor(
      const std::string& identifier) const override;

  // Object path of the underlying D-Bus characteristic.
  const dbus::ObjectPath& object_path() const { return object_path_; }

 protected:
  BluetoothGattCharacteristicBlueZ(BluetoothGattServiceBlueZ* service,
                                   const dbus::ObjectPath& object_path);
  ~BluetoothGattCharacteristicBlueZ() override;

  // Does not take ownership of the descriptor object.
  using DescriptorMap =
      std::map<dbus::ObjectPath, BluetoothGattDescriptorBlueZ*>;

  // Mapping from GATT descriptor object paths to descriptor objects owned by
  // this characteristic. Since the BlueZ implementation uses object paths
  // as unique identifiers, we also use this mapping to return descriptors by
  // identifier.
  DescriptorMap descriptors_;

  // The GATT service this GATT characteristic belongs to.
  BluetoothGattServiceBlueZ* service_;

 private:
  friend class BluetoothRemoteGattServiceBlueZ;

  // Object path of the D-Bus characteristic object.
  dbus::ObjectPath object_path_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<BluetoothGattCharacteristicBlueZ> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothGattCharacteristicBlueZ);
};

}  // namespace bluez

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_GATT_CHARACTERISTIC_BLUEZ_H_
