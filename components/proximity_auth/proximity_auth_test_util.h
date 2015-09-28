// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PROXIMITY_AUTH_PROXIMITY_AUTH_TEST_UTIL_H
#define COMPONENTS_PROXIMITY_AUTH_PROXIMITY_AUTH_TEST_UTIL_H

#include "components/proximity_auth/remote_device.h"

namespace proximity_auth {

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
                      kTestRemoteDevicePublicKey, RemoteDevice::BLUETOOTH_LE,
                      kTestRemoteDeviceBluetoothAddress, kTestRemoteDevicePSK,
                      kTestRemoteDeviceSignInChallenge);
}

// Returns a classic Bluetooth RemoteDevice used for tests.
inline RemoteDevice CreateClassicRemoteDeviceForTest() {
  return RemoteDevice(kTestRemoteDeviceUserId, kTestRemoteDeviceName,
                      kTestRemoteDevicePublicKey,
                      RemoteDevice::BLUETOOTH_CLASSIC,
                      kTestRemoteDeviceBluetoothAddress, kTestRemoteDevicePSK,
                      kTestRemoteDeviceSignInChallenge);
}

}  // namespace proximity_auth

#endif  // COMPONENTS_PROXIMITY_AUTH_PROXIMITY_AUTH_TEST_UTIL_H
