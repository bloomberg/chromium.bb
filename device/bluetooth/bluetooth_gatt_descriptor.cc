// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_gatt_descriptor.h"

#include "base/logging.h"

namespace device {

using bluetooth_utils::UUID;

const UUID BluetoothGattDescriptor::
    kCharacteristicExtendedPropertiesUuid("0x2900");
const UUID BluetoothGattDescriptor::
    kCharacteristicUserDescriptionUuid("0x2901");
const UUID BluetoothGattDescriptor::
    kClientCharacteristicConfigurationUuid("0x2902");
const UUID BluetoothGattDescriptor::
    kServerCharacteristicConfigurationUuid("0x2903");
const UUID BluetoothGattDescriptor::
    kCharacteristicPresentationFormatUuid("0x2904");
const UUID BluetoothGattDescriptor::
    kCharacteristicAggregateFormatUuid("0x2905");

BluetoothGattDescriptor::BluetoothGattDescriptor() {
}

BluetoothGattDescriptor::~BluetoothGattDescriptor() {
}

// static
BluetoothGattDescriptor* BluetoothGattDescriptor::Create(
    const bluetooth_utils::UUID& uuid,
    const std::vector<uint8>& value) {
  LOG(ERROR) << "Creating local GATT characteristic descriptors currently not "
             << "supported.";
  return NULL;
}

}  // namespace device
