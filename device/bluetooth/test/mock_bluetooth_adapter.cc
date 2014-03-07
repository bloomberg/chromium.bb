// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/test/mock_bluetooth_adapter.h"

namespace device {

MockBluetoothAdapter::Observer::Observer() {}
MockBluetoothAdapter::Observer::~Observer() {}

MockBluetoothAdapter::MockBluetoothAdapter() {
}

MockBluetoothAdapter::~MockBluetoothAdapter() {}

void MockBluetoothAdapter::AddDiscoverySession(
    const base::Closure& callback,
    const ErrorCallback& error_callback) {}

void MockBluetoothAdapter::RemoveDiscoverySession(
    const base::Closure& callback,
    const ErrorCallback& error_callback) {}

}  // namespace device
