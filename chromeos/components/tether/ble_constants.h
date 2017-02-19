// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TETHER_BLE_CONSTANTS_H_
#define CHROMEOS_COMPONENTS_TETHER_BLE_CONSTANTS_H_

#include <string>

namespace chromeos {

namespace tether {

// The maximum number of devices to which to advertise concurrently. If more
// than this number of devices are registered, other advertisements must be
// stopped before new ones can be added.
//
// Note that this upper limit on concurrent advertisements is imposed due to a
// hardware limit of advertisements (many devices have <10 total advertisement
// slots).
extern const uint8_t kMaxConcurrentAdvertisements;

// The service UUID used for BLE advertisements.
extern const char kAdvertisingServiceUuid[];

// The GATT server UUID used for uWeave.
extern const char kGattServerUuid[];

}  // namespace tether

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TETHER_BLE_CONSTANTS_H_
