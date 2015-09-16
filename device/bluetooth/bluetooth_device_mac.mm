// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_device_mac.h"

#include "device/bluetooth/bluetooth_adapter_mac.h"

namespace device {

BluetoothDeviceMac::BluetoothDeviceMac(BluetoothAdapterMac* adapter)
    : BluetoothDevice(adapter) {}

BluetoothDeviceMac::~BluetoothDeviceMac() {
}

}  // namespace device
