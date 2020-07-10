// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/dbus/fake_bluetooth_debug_manager_client.h"

namespace bluez {

FakeBluetoothDebugManagerClient::FakeBluetoothDebugManagerClient() = default;
FakeBluetoothDebugManagerClient::~FakeBluetoothDebugManagerClient() = default;

void FakeBluetoothDebugManagerClient::Init(
    dbus::Bus* bus,
    const std::string& bluetooth_service_name) {}

void FakeBluetoothDebugManagerClient::SetLogLevels(
    const uint8_t dispatcher_level,
    const uint8_t newblue_level,
    const uint8_t bluez_level,
    const uint8_t kernel_level,
    base::OnceClosure callback,
    ErrorCallback error_callback) {
  if (should_next_set_log_levels_fail_) {
    should_next_set_log_levels_fail_ = false;
    set_log_levels_fail_count_++;
    std::move(error_callback).Run(kNoResponseError, "");
    return;
  }

  dispatcher_level_ = dispatcher_level;
  std::move(callback).Run();
}

void FakeBluetoothDebugManagerClient::MakeNextSetLogLevelsFail() {
  should_next_set_log_levels_fail_ = true;
}

}  // namespace bluez
