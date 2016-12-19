// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cryptauth/remote_device_test_util.h"

#include "base/base64.h"

namespace cryptauth {

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
