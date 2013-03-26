// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/mock_experimental_bluetooth_adapter_client.h"

#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

MockExperimentalBluetoothAdapterClient::Properties::Properties()
    : ExperimentalBluetoothAdapterClient::Properties::Properties(
        NULL,
        bluetooth_adapter::kExperimentalBluetoothAdapterInterface,
        PropertyChangedCallback()) {}

MockExperimentalBluetoothAdapterClient::Properties::~Properties() {}

MockExperimentalBluetoothAdapterClient::
    MockExperimentalBluetoothAdapterClient() {}

MockExperimentalBluetoothAdapterClient::
    ~MockExperimentalBluetoothAdapterClient() {}

}  // namespace chromeos
