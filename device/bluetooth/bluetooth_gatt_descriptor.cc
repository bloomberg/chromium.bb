// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_gatt_descriptor.h"

#include "base/logging.h"

namespace device {

const BluetoothUUID BluetoothGattDescriptor::
    kCharacteristicExtendedPropertiesUuid("2900");
const BluetoothUUID BluetoothGattDescriptor::
    kCharacteristicUserDescriptionUuid("2901");
const BluetoothUUID BluetoothGattDescriptor::
    kClientCharacteristicConfigurationUuid("2902");
const BluetoothUUID BluetoothGattDescriptor::
    kServerCharacteristicConfigurationUuid("2903");
const BluetoothUUID BluetoothGattDescriptor::
    kCharacteristicPresentationFormatUuid("2904");
const BluetoothUUID BluetoothGattDescriptor::
    kCharacteristicAggregateFormatUuid("2905");

BluetoothGattDescriptor::BluetoothGattDescriptor() {
}

BluetoothGattDescriptor::~BluetoothGattDescriptor() {
}

// static
BluetoothGattDescriptor* BluetoothGattDescriptor::Create(
    const BluetoothUUID& uuid,
    const std::vector<uint8>& value,
    BluetoothGattCharacteristic::Permissions permissions) {
  LOG(ERROR) << "Creating local GATT characteristic descriptors currently not "
             << "supported.";
  return NULL;
}

}  // namespace device
