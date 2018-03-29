// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/device_sync/fake_device_sync_observer.h"

namespace chromeos {

namespace device_sync {

FakeDeviceSyncObserver::FakeDeviceSyncObserver() = default;

FakeDeviceSyncObserver::~FakeDeviceSyncObserver() = default;

void FakeDeviceSyncObserver::SetDelegate(Delegate* delegate) {
  delegate_ = delegate;
}

mojom::DeviceSyncObserverPtr FakeDeviceSyncObserver::GenerateInterfacePtr() {
  mojom::DeviceSyncObserverPtr interface_ptr;
  bindings_.AddBinding(this, mojo::MakeRequest(&interface_ptr));
  return interface_ptr;
}

void FakeDeviceSyncObserver::OnEnrollmentFinished(bool success) {
  if (success)
    ++num_enrollment_success_events_;
  else
    ++num_enrollment_failure_events_;

  if (delegate_)
    delegate_->OnEnrollmentFinishedCalled();
}

void FakeDeviceSyncObserver::OnDevicesSynced(bool success) {
  if (success)
    ++num_sync_success_events_;
  else
    ++num_sync_failure_events_;

  if (delegate_)
    delegate_->OnDevicesSyncedCalled();
}

}  // namespace device_sync

}  // namespace chromeos
