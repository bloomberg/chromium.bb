// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/test/test_device_status_listener.h"

namespace download {

TestDeviceStatusListener::TestDeviceStatusListener() = default;

TestDeviceStatusListener::~TestDeviceStatusListener() = default;

void TestDeviceStatusListener::NotifyObserver(
    const DeviceStatus& device_status) {
  status_ = device_status;
  DCHECK(observer_);
  observer_->OnDeviceStatusChanged(status_);
}

void TestDeviceStatusListener::Start(DeviceStatusListener::Observer* observer) {
  observer_ = observer;
}

void TestDeviceStatusListener::Stop() {
  status_ = DeviceStatus();
  observer_ = nullptr;
}

}  // namespace download
