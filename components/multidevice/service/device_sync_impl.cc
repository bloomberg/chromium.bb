// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multidevice/service/device_sync_impl.h"

namespace device_sync {

DeviceSyncImpl::DeviceSyncImpl() = default;

DeviceSyncImpl::~DeviceSyncImpl() = default;

void DeviceSyncImpl::BindRequest(mojom::DeviceSyncRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void DeviceSyncImpl::ForceEnrollmentNow() {
  // TODO(khorimoto): Actually perform enrollment. Currently, we immediately
  // alert observers that a successful enrollment occurred.
  observers_.ForAllPtrs([](auto* observer) {
    observer->OnEnrollmentFinished(true /* success */);
  });
}

void DeviceSyncImpl::ForceSyncNow() {
  // TODO(khorimoto): Actually perform a sync. Currently, we immediately
  // alert observers that a successful sync occurred.
  observers_.ForAllPtrs(
      [](auto* observer) { observer->OnDevicesSynced(true /* success */); });
}

void DeviceSyncImpl::AddObserver(mojom::DeviceSyncObserverPtr observer,
                                 AddObserverCallback callback) {
  observers_.AddPtr(std::move(observer));
  std::move(callback).Run();
}

}  // namespace device_sync
