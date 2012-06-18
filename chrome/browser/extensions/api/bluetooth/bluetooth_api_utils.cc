// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/bluetooth/bluetooth_api_utils.h"

#if defined(OS_CHROMEOS)

#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/chromeos/bluetooth/bluetooth_device.h"
#include "chrome/common/extensions/api/experimental_bluetooth.h"

namespace extensions {
namespace api {
namespace experimental_bluetooth {

// Fill in a Device object from a chromeos::BluetoothDevice.
void BluetoothDeviceToApiDevice(
    const chromeos::BluetoothDevice& device,
    Device* out) {
  out->name = UTF16ToUTF8(device.GetName());
  out->address = device.address();
  out->paired = device.IsPaired();
  out->bonded = device.IsBonded();
  out->connected = device.IsConnected();
}

// The caller takes ownership of the returned pointer.
base::Value* BluetoothDeviceToValue(const chromeos::BluetoothDevice& device) {
  extensions::api::experimental_bluetooth::Device api_device;
  BluetoothDeviceToApiDevice(device, &api_device);
  return api_device.ToValue().release();
}

}  // namespace experimental_bluetooth
}  // namespace api
}  // namespace extensions

#endif
