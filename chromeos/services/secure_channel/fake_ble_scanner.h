// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_FAKE_BLE_SCANNER_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_FAKE_BLE_SCANNER_H_

#include <tuple>
#include <vector>

#include "base/macros.h"
#include "chromeos/services/secure_channel/ble_scanner.h"

namespace chromeos {

namespace secure_channel {

// Test BleScanner implementation.
class FakeBleScanner : public BleScanner {
 public:
  FakeBleScanner(Delegate* delegate);
  ~FakeBleScanner() override;

  size_t num_scan_filter_changes_handled() const {
    return num_scan_filter_changes_handled_;
  }

  // Public for testing.
  using BleScanner::NotifyReceivedAdvertisementFromDevice;

 private:
  void HandleScanFilterChange() override;

  size_t num_scan_filter_changes_handled_ = 0u;

  DISALLOW_COPY_AND_ASSIGN(FakeBleScanner);
};

// Test BleScanner::Delegate implementation.
class FakeBleScannerDelegate : public BleScanner::Delegate {
 public:
  FakeBleScannerDelegate();
  ~FakeBleScannerDelegate() override;

  using ScannedResultList = std::vector<
      std::tuple<cryptauth::RemoteDeviceRef, device::BluetoothDevice*, bool>>;

  const ScannedResultList& handled_scan_results() const {
    return handled_scan_results_;
  }

 private:
  void OnReceivedAdvertisement(cryptauth::RemoteDeviceRef remote_device,
                               device::BluetoothDevice* bluetooth_device,
                               bool is_background_advertisement) override;

  ScannedResultList handled_scan_results_;

  DISALLOW_COPY_AND_ASSIGN(FakeBleScannerDelegate);
};

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_FAKE_BLE_SCANNER_H_
