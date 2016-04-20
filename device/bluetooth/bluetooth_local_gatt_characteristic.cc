// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_local_gatt_characteristic.h"

#include "base/logging.h"
#include "device/bluetooth/bluetooth_local_gatt_service.h"

namespace device {

BluetoothLocalGattCharacteristic::BluetoothLocalGattCharacteristic() {}

BluetoothLocalGattCharacteristic::~BluetoothLocalGattCharacteristic() {}

// static
BluetoothLocalGattCharacteristic* BluetoothLocalGattCharacteristic::Create(
    const BluetoothUUID& uuid,
    const std::vector<uint8_t>& value,
    Properties properties,
    Permissions permissions,
    BluetoothLocalGattService* service) {
  NOTIMPLEMENTED();
  return NULL;
}

}  // namespace device
