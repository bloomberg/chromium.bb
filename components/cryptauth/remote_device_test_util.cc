// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cryptauth/remote_device_test_util.h"

#include "base/base64.h"

namespace cryptauth {

// Attributes of the default test remote device.
const char kTestRemoteDeviceUserId[] = "example@gmail.com";
const char kTestRemoteDeviceName[] = "remote device";
const char kTestRemoteDevicePublicKey[] = "public key";
const char kTestRemoteDevicePSK[] = "remote device psk";
const bool kTestRemoteDeviceUnlockKey = true;
const bool kTestRemoteDeviceSupportsMobileHotspot = true;
const int64_t kTestRemoteDeviceLastUpdateTimeMillis = 0L;

RemoteDevice CreateRemoteDeviceForTest() {
  return RemoteDevice(kTestRemoteDeviceUserId, kTestRemoteDeviceName,
                      kTestRemoteDevicePublicKey, kTestRemoteDevicePSK,
                      kTestRemoteDeviceUnlockKey,
                      kTestRemoteDeviceSupportsMobileHotspot,
                      kTestRemoteDeviceLastUpdateTimeMillis);
}

std::vector<RemoteDevice> GenerateTestRemoteDevices(size_t num_to_create) {
  std::vector<RemoteDevice> generated_devices;

  for (size_t i = 0; i < num_to_create; i++) {
    RemoteDevice device;
    device.public_key = "publicKey" + std::to_string(i);
    generated_devices.push_back(device);
  }

  return generated_devices;
}

}  // namespace cryptauth
