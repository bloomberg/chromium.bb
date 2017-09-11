// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multidevice/service/fake_device_sync.h"

namespace multidevice {

FakeDeviceSync::FakeDeviceSync() {}

FakeDeviceSync::~FakeDeviceSync() {}

void FakeDeviceSync::ForceEnrollmentNow() {
  observers_.ForAllPtrs([this](
                            device_sync::mojom::DeviceSyncObserver* observer) {
    observer->OnEnrollmentFinished(should_enroll_successfully_ /* success */);
  });
}

void FakeDeviceSync::ForceSyncNow() {
  observers_.ForAllPtrs(
      [this](device_sync::mojom::DeviceSyncObserver* observer) {
        observer->OnDevicesSynced(should_sync_successfully_ /* success */);
      });
}

void FakeDeviceSync::AddObserver(
    device_sync::mojom::DeviceSyncObserverPtr observer) {
  observers_.AddPtr(std::move(observer));
}

}  // namespace multidevice