// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluez/bluetooth_local_gatt_descriptor_bluez.h"

#include <string>

#include "base/logging.h"

namespace bluez {

BluetoothLocalGattDescriptorBlueZ::BluetoothLocalGattDescriptorBlueZ(
    const dbus::ObjectPath& object_path)
    : BluetoothGattDescriptorBlueZ(object_path), weak_ptr_factory_(this) {
  VLOG(1) << "Creating local GATT descriptor with identifier: "
          << GetIdentifier();
}

BluetoothLocalGattDescriptorBlueZ::~BluetoothLocalGattDescriptorBlueZ() {}

}  // namespace bluez
