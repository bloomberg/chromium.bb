// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PROXIMITY_AUTH_BLUETOOTH_UTIL_H
#define COMPONENTS_PROXIMITY_AUTH_BLUETOOTH_UTIL_H

#include <string>

#include "base/callback_forward.h"
#include "device/bluetooth/bluetooth_device.h"

namespace device {
class BluetoothUUID;
}

namespace proximity_auth {
namespace bluetooth_util {

typedef base::Callback<void(const std::string& error_message)> ErrorCallback;

// Connects to the SDP service on the Bluetooth device with the given
// |device_address|, if possible. Calls the |callback| on success, or the
// |error_callback| with an error message on failure. Because this can be an
// expensive operation, the work will be run on the provided |task_runner|,
// which should correspond to a background thread.
void SeekDeviceByAddress(const std::string& device_address,
                         const base::Closure& callback,
                         const ErrorCallback& error_callback,
                         base::TaskRunner* task_runner);

void ConnectToServiceInsecurely(
    device::BluetoothDevice* device,
    const device::BluetoothUUID& uuid,
    const device::BluetoothDevice::ConnectToServiceCallback& callback,
    const device::BluetoothDevice::ConnectToServiceErrorCallback&
        error_callback);

}  // namespace bluetooth_util
}  // namespace proximity_auth

#endif  // COMPONENTS_PROXIMITY_AUTH_BLUETOOTH_UTIL_H
