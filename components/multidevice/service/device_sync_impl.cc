// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multidevice/service/device_sync_impl.h"

namespace multidevice {

DeviceSyncImpl::DeviceSyncImpl(
    std::unique_ptr<service_manager::ServiceContextRef> service_ref)
    : service_ref_(std::move(service_ref)) {}

DeviceSyncImpl::~DeviceSyncImpl() {}

void DeviceSyncImpl::ForceEnrollmentNow() {
  observers_.ForAllPtrs([](device_sync::mojom::DeviceSyncObserver* observer) {
    // TODO(hsuregan): Actually enroll observers, and pass the success/failure
    // status to observer->OnEnrollmentFinished().
    observer->OnEnrollmentFinished(true /* success */);
  });
}

void DeviceSyncImpl::ForceSyncNow() {
  observers_.ForAllPtrs([](device_sync::mojom::DeviceSyncObserver* observer) {
    // TODO(hsuregan): Actually sync observers, and pass the success/failure
    // status to observer->OnEnrollmentFinished().
    observer->OnDevicesSynced(true /* success */);
  });
}

void DeviceSyncImpl::AddObserver(
    device_sync::mojom::DeviceSyncObserverPtr observer) {
  observers_.AddPtr(std::move(observer));
}

}  // namespace multidevice