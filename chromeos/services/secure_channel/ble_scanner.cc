// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/ble_scanner.h"

#include "base/logging.h"
#include "base/stl_util.h"
#include "chromeos/components/proximity_auth/logging/logging.h"

namespace chromeos {

namespace secure_channel {

BleScanner::BleScanner(Delegate* delegate) : delegate_(delegate) {}

BleScanner::~BleScanner() = default;

void BleScanner::AddScanFilter(const DeviceIdPair& scan_filter) {
  if (base::ContainsKey(scan_filters_, scan_filter)) {
    PA_LOG(ERROR) << "BleScanner::AddScanFilter(): Tried to add a scan filter "
                  << "which already existed. Filter: " << scan_filter;
    NOTREACHED();
  }

  scan_filters_.insert(scan_filter);
  HandleScanFilterChange();
}

void BleScanner::RemoveScanFilter(const DeviceIdPair& scan_filter) {
  if (!base::ContainsKey(scan_filters_, scan_filter)) {
    PA_LOG(ERROR) << "BleScanner::RemoveScanFilter(): Tried to remove a scan "
                  << "filter which was not present. Filter: " << scan_filter;
    NOTREACHED();
  }

  scan_filters_.erase(scan_filter);
  HandleScanFilterChange();
}

void BleScanner::NotifyReceivedAdvertisementFromDevice(
    const cryptauth::RemoteDeviceRef& remote_device,
    device::BluetoothDevice* bluetooth_device,
    bool is_background_advertisement) {
  delegate_->OnReceivedAdvertisement(remote_device, bluetooth_device,
                                     is_background_advertisement);
}

}  // namespace secure_channel

}  // namespace chromeos
