// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/fake_ble_advertiser.h"

#include <algorithm>

#include "base/stl_util.h"

namespace chromeos {

namespace tether {

FakeBleAdvertiser::FakeBleAdvertiser(
    bool automatically_update_active_advertisements)
    : automatically_update_active_advertisements_(
          automatically_update_active_advertisements) {}

FakeBleAdvertiser::~FakeBleAdvertiser() {}

void FakeBleAdvertiser::NotifyAllAdvertisementsUnregistered() {
  BleAdvertiser::NotifyAllAdvertisementsUnregistered();
}

bool FakeBleAdvertiser::StartAdvertisingToDevice(
    const cryptauth::RemoteDevice& remote_device) {
  if (should_fail_to_start_advertising_)
    return false;

  if (std::find(registered_devices_.begin(), registered_devices_.end(),
                remote_device) != registered_devices_.end()) {
    return false;
  }

  registered_devices_.push_back(remote_device);

  if (automatically_update_active_advertisements_)
    are_advertisements_registered_ = true;

  return true;
}

bool FakeBleAdvertiser::StopAdvertisingToDevice(
    const cryptauth::RemoteDevice& remote_device) {
  if (std::find(registered_devices_.begin(), registered_devices_.end(),
                remote_device) == registered_devices_.end()) {
    return false;
  }

  base::Erase(registered_devices_, remote_device);
  if (automatically_update_active_advertisements_ &&
      registered_devices_.empty()) {
    are_advertisements_registered_ = false;
    NotifyAllAdvertisementsUnregistered();
  }

  return true;
}

bool FakeBleAdvertiser::AreAdvertisementsRegistered() {
  return are_advertisements_registered_;
}

}  // namespace tether

}  // namespace chromeos
