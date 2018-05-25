// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_BLE_INITIATOR_FAILURE_TYPE_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_BLE_INITIATOR_FAILURE_TYPE_H_

namespace chromeos {

namespace secure_channel {

enum class BleInitiatorFailureType {
  // A connection was formed successfully, but there was an error
  // authenticating the connection.
  kAuthenticationError,

  // This device successfully picked up a scan result for the remote device, but
  // there was an error forming the connection.
  kGattConnectionError,

  // A higher-priority message needed to be sent, so this attempt needed to be
  // stopped temporarily.
  kInterruptedByHigherPriorityConnectionAttempt,

  // No scan result was ever discovered for the remote device.
  kTimeoutContactingRemoteDevice,

  // BeaconSeeds for the remote device were either unavailable or stale.
  kInvalidBeaconSeeds
};

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_BLE_INITIATOR_FAILURE_TYPE_H_
