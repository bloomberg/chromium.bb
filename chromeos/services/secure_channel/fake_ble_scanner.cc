// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/fake_ble_scanner.h"

namespace chromeos {

namespace secure_channel {

FakeBleScanner::FakeBleScanner(Delegate* delegate) : BleScanner(delegate) {}

FakeBleScanner::~FakeBleScanner() = default;

void FakeBleScanner::HandleScanFilterChange() {
  ++num_scan_filter_changes_handled_;
}

FakeBleScannerDelegate::FakeBleScannerDelegate() = default;

FakeBleScannerDelegate::~FakeBleScannerDelegate() = default;

void FakeBleScannerDelegate::OnReceivedAdvertisement(
    cryptauth::RemoteDeviceRef remote_device,
    device::BluetoothDevice* bluetooth_device,
    bool is_background_advertisement) {
  handled_scan_results_.push_back(std::make_tuple(
      remote_device, bluetooth_device, is_background_advertisement));
}

}  // namespace secure_channel

}  // namespace chromeos
