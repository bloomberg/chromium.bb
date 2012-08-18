// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/mock_bluetooth_input_client.h"

namespace chromeos {

MockBluetoothInputClient::Properties::Properties()
    : BluetoothInputClient::Properties::Properties(
        NULL, PropertyChangedCallback()) {}

MockBluetoothInputClient::Properties::~Properties() {}

MockBluetoothInputClient::MockBluetoothInputClient() {}

MockBluetoothInputClient::~MockBluetoothInputClient() {}

}  // namespace chromeos
