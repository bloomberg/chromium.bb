// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/ble_advertiser.h"

namespace chromeos {

namespace secure_channel {

BleAdvertiser::BleAdvertiser(Delegate* delegate) : delegate_(delegate) {}

BleAdvertiser::~BleAdvertiser() = default;

void BleAdvertiser::NotifyAdvertisingSlotEnded(
    const DeviceIdPair& device_id_pair,
    bool replaced_by_higher_priority_advertisement) {
  delegate_->OnAdvertisingSlotEnded(device_id_pair,
                                    replaced_by_higher_priority_advertisement);
}

void BleAdvertiser::NotifyFailureToGenerateAdvertisement(
    const DeviceIdPair& device_id_pair) {
  delegate_->OnFailureToGenerateAdvertisement(device_id_pair);
}

}  // namespace secure_channel

}  // namespace chromeos
