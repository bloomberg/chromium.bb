// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUEZ_BLUETOOTH_LOCAL_GATT_CHARACTERISTIC_BLUEZ_H_
#define DEVICE_BLUETOOTH_BLUEZ_BLUETOOTH_LOCAL_GATT_CHARACTERISTIC_BLUEZ_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluetooth_local_gatt_characteristic.h"
#include "device/bluetooth/bluez/bluetooth_gatt_characteristic_bluez.h"

namespace bluez {

class BluetoothLocalGattServiceBlueZ;

// The BluetoothLocalGattCharacteristicBlueZ class implements
// BluetoothLocalGattCharacteristic for local GATT characteristics for
// platforms that use BlueZ.
class BluetoothLocalGattCharacteristicBlueZ
    : public BluetoothGattCharacteristicBlueZ,
      public device::BluetoothLocalGattCharacteristic {
 public:
 private:
  friend class BluetoothLocalGattServiceBlueZ;

  BluetoothLocalGattCharacteristicBlueZ(BluetoothLocalGattServiceBlueZ* service,
                                        const dbus::ObjectPath& object_path);
  ~BluetoothLocalGattCharacteristicBlueZ() override;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<BluetoothLocalGattCharacteristicBlueZ> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothLocalGattCharacteristicBlueZ);
};

}  // namespace bluez

#endif  // DEVICE_BLUETOOTH_BLUEZ_BLUETOOTH_LOCAL_GATT_CHARACTERISTIC_BLUEZ_H_
