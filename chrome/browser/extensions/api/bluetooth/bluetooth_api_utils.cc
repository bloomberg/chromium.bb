// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/bluetooth/bluetooth_api_utils.h"

#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/extensions/api/bluetooth.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_device.h"

namespace extensions {
namespace api {
namespace bluetooth {

void BluetoothDeviceToApiDevice(const device::BluetoothDevice& device,
                                Device* out) {
  out->name = UTF16ToUTF8(device.GetName());
  out->address = device.address();
  out->paired = device.IsPaired();
  out->bonded = device.IsBonded();
  out->connected = device.IsConnected();
}

void PopulateAdapterState(const device::BluetoothAdapter& adapter,
                          AdapterState* out) {
  out->discovering = adapter.IsDiscovering();
  out->available = adapter.IsPresent();
  out->powered = adapter.IsPowered();
  out->name = adapter.name();
  out->address = adapter.address();
}

}  // namespace bluetooth
}  // namespace api
}  // namespace extensions
