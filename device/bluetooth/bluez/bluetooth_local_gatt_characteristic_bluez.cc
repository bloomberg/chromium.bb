// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluez/bluetooth_local_gatt_characteristic_bluez.h"

#include <string>

#include "base/logging.h"
#include "device/bluetooth/bluez/bluetooth_local_gatt_service_bluez.h"

namespace bluez {

BluetoothLocalGattCharacteristicBlueZ::BluetoothLocalGattCharacteristicBlueZ(
    BluetoothLocalGattServiceBlueZ* service,
    const dbus::ObjectPath& object_path)
    : BluetoothGattCharacteristicBlueZ(object_path), weak_ptr_factory_(this) {
  VLOG(1) << "Creating local GATT characteristic with identifier: "
          << GetIdentifier();
}

BluetoothLocalGattCharacteristicBlueZ::
    ~BluetoothLocalGattCharacteristicBlueZ() {}

}  // namespace bluez
