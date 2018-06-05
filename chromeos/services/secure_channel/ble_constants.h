// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_BLE_CONSTANTS_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_BLE_CONSTANTS_H_

#include <string>

namespace chromeos {

namespace secure_channel {

// The maximum number of devices to which to advertise concurrently. If more
// than this number of devices are registered, other advertisements must be
// stopped before new ones can be added.
//
// Note that this upper limit on concurrent advertisements is imposed due to a
// hardware limit of advertisements (many devices have <10 total advertisement
// slots).
constexpr const size_t kMaxConcurrentAdvertisements = 2;

// The service UUID used for BLE advertisements.
constexpr const char kAdvertisingServiceUuid[] =
    "0000fe50-0000-1000-8000-00805f9b34fb";

// The GATT server UUID used for uWeave.
constexpr const char kGattServerUuid[] = "b3b7e28e-a000-3e17-bd86-6e97b9e28c11";

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_BLE_CONSTANTS_H_
