// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_local_gatt_service.h"

#include "base/logging.h"

namespace device {

BluetoothLocalGattService::BluetoothLocalGattService() {}

BluetoothLocalGattService::~BluetoothLocalGattService() {}

// static
BluetoothLocalGattService* BluetoothLocalGattService::Create(
    const BluetoothUUID& uuid,
    bool is_primary,
    BluetoothLocalGattService* included_service,
    Delegate* delegate) {
  NOTIMPLEMENTED();
  return NULL;
}

}  // namespace device
