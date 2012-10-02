// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_BLUETOOTH_OUT_OF_BAND_PAIRING_DATA_H_
#define CHROMEOS_DBUS_BLUETOOTH_OUT_OF_BAND_PAIRING_DATA_H_

#include "base/basictypes.h"

namespace chromeos {

const size_t kBluetoothOutOfBandPairingDataSize = 16;

// A simple structure representing the data required to perform Out Of Band
// Pairing.  See
// http://mclean-linsky.net/joel/cv/Simple%20Pairing_WP_V10r00.pdf
struct BluetoothOutOfBandPairingData {
  // Simple Pairing Hash C.
  uint8_t hash[kBluetoothOutOfBandPairingDataSize];

  // Simple Pairing Randomizer R.
  uint8_t randomizer[kBluetoothOutOfBandPairingDataSize];
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_BLUETOOTH_OUT_OF_BAND_PAIRING_DATA_H_
