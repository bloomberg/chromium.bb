// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/fake_ble_scanner.h"

#include <algorithm>

#include "base/stl_util.h"

namespace chromeos {

namespace tether {

FakeBleScanner::FakeBleScanner(bool automatically_update_discovery_session)
    : automatically_update_discovery_session_(
          automatically_update_discovery_session) {}

FakeBleScanner::~FakeBleScanner() {}

void FakeBleScanner::NotifyReceivedAdvertisementFromDevice(
    const cryptauth::RemoteDevice& remote_device,
    device::BluetoothDevice* bluetooth_device) {
  BleScanner::NotifyReceivedAdvertisementFromDevice(remote_device,
                                                    bluetooth_device);
}

void FakeBleScanner::NotifyDiscoverySessionStateChanged(
    bool discovery_session_active) {
  BleScanner::NotifyDiscoverySessionStateChanged(discovery_session_active);
}

bool FakeBleScanner::RegisterScanFilterForDevice(
    const cryptauth::RemoteDevice& remote_device) {
  if (should_fail_to_register_)
    return false;

  if (std::find(registered_devices_.begin(), registered_devices_.end(),
                remote_device) != registered_devices_.end()) {
    return false;
  }

  bool was_empty = registered_devices_.empty();
  registered_devices_.push_back(remote_device);

  if (was_empty && automatically_update_discovery_session_) {
    is_discovery_session_active_ = true;
    NotifyDiscoverySessionStateChanged(true);
  }

  return true;
}

bool FakeBleScanner::UnregisterScanFilterForDevice(
    const cryptauth::RemoteDevice& remote_device) {
  if (std::find(registered_devices_.begin(), registered_devices_.end(),
                remote_device) == registered_devices_.end()) {
    return false;
  }

  base::Erase(registered_devices_, remote_device);

  if (automatically_update_discovery_session_ && registered_devices_.empty()) {
    is_discovery_session_active_ = false;
    NotifyDiscoverySessionStateChanged(false);
  }

  return true;
}

bool FakeBleScanner::ShouldDiscoverySessionBeActive() {
  return !registered_devices_.empty();
}

bool FakeBleScanner::IsDiscoverySessionActive() {
  return is_discovery_session_active_;
}

}  // namespace tether

}  // namespace chromeos
