// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_BLUETOOTH_BLUETOOTH_API_UTILS_H_
#define CHROME_BROWSER_EXTENSIONS_API_BLUETOOTH_BLUETOOTH_API_UTILS_H_
#pragma once

#if defined(OS_CHROMEOS)

#include "base/values.h"
#include "chrome/browser/chromeos/bluetooth/bluetooth_device.h"
#include "chrome/common/extensions/api/experimental_bluetooth.h"

namespace extensions {
namespace api {
namespace experimental_bluetooth {

// Fill in a Device object from a chromeos::BluetoothDevice.
void BluetoothDeviceToApiDevice(
    const chromeos::BluetoothDevice& device,
    Device* out);

// The caller takes ownership of the returned pointer.
base::Value* BluetoothDeviceToValue(const chromeos::BluetoothDevice& device);

}  // namespace experimental_bluetooth
}  // namespace api
}  // namespace extensions

#endif

#endif  // CHROME_BROWSER_EXTENSIONS_API_BLUETOOTH_BLUETOOTH_API_UTILS_H_
