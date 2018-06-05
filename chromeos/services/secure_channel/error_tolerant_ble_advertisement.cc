// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/error_tolerant_ble_advertisement.h"

namespace chromeos {

namespace secure_channel {

ErrorTolerantBleAdvertisement::ErrorTolerantBleAdvertisement(
    const std::string& device_id)
    : device_id_(device_id) {}

ErrorTolerantBleAdvertisement::~ErrorTolerantBleAdvertisement() = default;

}  // namespace secure_channel

}  // namespace chromeos
