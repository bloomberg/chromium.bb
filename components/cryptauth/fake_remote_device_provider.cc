// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cryptauth/fake_remote_device_provider.h"

namespace cryptauth {

FakeRemoteDeviceProvider::FakeRemoteDeviceProvider() {}

FakeRemoteDeviceProvider::~FakeRemoteDeviceProvider() {}

const RemoteDeviceList FakeRemoteDeviceProvider::GetSyncedDevices() const {
  return synced_remote_devices_;
}

}  // namespace cryptauth