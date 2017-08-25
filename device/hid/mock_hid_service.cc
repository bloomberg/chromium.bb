// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/hid/mock_hid_service.h"

namespace device {

MockHidService::MockHidService() : weak_factory_(this) {}

MockHidService::~MockHidService() = default;

base::WeakPtr<HidService> MockHidService::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void MockHidService::AddDevice(scoped_refptr<HidDeviceInfo> info) {
  HidService::AddDevice(info);
}

void MockHidService::RemoveDevice(
    const HidPlatformDeviceId& platform_device_id) {
  HidService::RemoveDevice(platform_device_id);
}

void MockHidService::FirstEnumerationComplete() {
  HidService::FirstEnumerationComplete();
}

const std::map<std::string, scoped_refptr<HidDeviceInfo>>&
MockHidService::devices() const {
  return HidService::devices();
}

}  // namespace device
