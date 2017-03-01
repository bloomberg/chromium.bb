// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRYPTAUTH_CRYPTAUTH_TEST_UTIL_H_
#define COMPONENTS_CRYPTAUTH_CRYPTAUTH_TEST_UTIL_H_

#include "components/cryptauth/remote_device.h"

namespace cryptauth {

// Attributes of the default test remote device.
extern const char kTestRemoteDeviceUserId[];
extern const char kTestRemoteDeviceName[];
extern const char kTestRemoteDevicePublicKey[];
extern const char kTestRemoteDeviceBluetoothAddress[];
extern const char kTestRemoteDevicePSK[];
extern const char kTestRemoteDeviceSignInChallenge[];

// Returns a BLE RemoteDevice used for tests.
inline RemoteDevice CreateLERemoteDeviceForTest() {
  return RemoteDevice(kTestRemoteDeviceUserId, kTestRemoteDeviceName,
                      kTestRemoteDevicePublicKey,
                      kTestRemoteDeviceBluetoothAddress, kTestRemoteDevicePSK,
                      kTestRemoteDeviceSignInChallenge);
}

// Returns a classic Bluetooth RemoteDevice used for tests.
inline RemoteDevice CreateClassicRemoteDeviceForTest() {
  return RemoteDevice(kTestRemoteDeviceUserId, kTestRemoteDeviceName,
                      kTestRemoteDevicePublicKey,
                      kTestRemoteDeviceBluetoothAddress, kTestRemoteDevicePSK,
                      kTestRemoteDeviceSignInChallenge);
}

}  // namespace cryptauth

#endif  // COMPONENTS_CRYPTAUTH_CRYPTAUTH_TEST_UTIL_H_
