// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/mock_experimental_bluetooth_device_client.h"

#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

MockExperimentalBluetoothDeviceClient::Properties::Properties()
    : ExperimentalBluetoothDeviceClient::Properties::Properties(
        NULL,
        bluetooth_device::kExperimentalBluetoothDeviceInterface,
        PropertyChangedCallback()) {}

MockExperimentalBluetoothDeviceClient::Properties::~Properties() {}

MockExperimentalBluetoothDeviceClient::
    MockExperimentalBluetoothDeviceClient() {}

MockExperimentalBluetoothDeviceClient::
    ~MockExperimentalBluetoothDeviceClient() {}

}  // namespace chromeos
