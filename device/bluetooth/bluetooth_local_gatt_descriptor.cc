// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_local_gatt_descriptor.h"

#include "base/logging.h"

namespace device {

BluetoothLocalGattDescriptor::BluetoothLocalGattDescriptor() {}

BluetoothLocalGattDescriptor::~BluetoothLocalGattDescriptor() {}

// static
BluetoothLocalGattDescriptor* BluetoothLocalGattDescriptor::Create(
    const BluetoothUUID& uuid,
    const std::vector<uint8_t>& value,
    BluetoothGattCharacteristic::Permissions permissions,
    BluetoothLocalGattCharacteristic* characteristic) {
  LOG(ERROR) << "Creating local GATT characteristic descriptors currently not "
             << "supported.";
  return NULL;
}

}  // namespace device
