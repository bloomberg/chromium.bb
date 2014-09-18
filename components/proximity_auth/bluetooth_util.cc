// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(OS_CHROMEOS)

#include "components/proximity_auth/bluetooth_util.h"

#include "base/callback.h"

using device::BluetoothDevice;

namespace proximity_auth {
namespace bluetooth_util {
namespace {
const char kApiUnavailable[] = "This API is not implemented on this platform.";
}  // namespace

void SeekDeviceByAddress(const std::string& device_address,
                         const base::Closure& callback,
                         const ErrorCallback& error_callback,
                         base::TaskRunner* task_runner) {
  error_callback.Run(kApiUnavailable);
}

void ConnectToServiceInsecurely(
    BluetoothDevice* device,
    const device::BluetoothUUID& uuid,
    const BluetoothDevice::ConnectToServiceCallback& callback,
    const BluetoothDevice::ConnectToServiceErrorCallback& error_callback) {
  error_callback.Run(kApiUnavailable);
}

}  // namespace bluetooth_util
}  // namespace proximity_auth

#endif  // !defined(OS_CHROMEOS)
