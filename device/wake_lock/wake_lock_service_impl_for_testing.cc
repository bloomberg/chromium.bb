// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/wake_lock/wake_lock_service_impl_for_testing.h"

#include <utility>

namespace device {

WakeLockServiceImplForTesting::WakeLockServiceImplForTesting(
    mojom::WakeLockServiceRequest request,
    mojom::WakeLockType type,
    mojom::WakeLockReason reason,
    const std::string& description,
    int context_id,
    WakeLockContextCallback native_view_getter,
    scoped_refptr<base::SingleThreadTaskRunner> file_task_runner)
    : WakeLockServiceImpl(std::move(request),
                          type,
                          reason,
                          description,
                          context_id,
                          native_view_getter,
                          file_task_runner),
      has_wake_lock_(false) {}

WakeLockServiceImplForTesting::~WakeLockServiceImplForTesting() {}

void WakeLockServiceImplForTesting::HasWakeLockForTests(
    HasWakeLockForTestsCallback callback) {
  std::move(callback).Run(has_wake_lock_);
}

void WakeLockServiceImplForTesting::UpdateWakeLock() {
  DCHECK(num_lock_requests_ >= 0);

  if (num_lock_requests_) {
    if (!has_wake_lock_)
      CreateWakeLock();
  } else {
    if (has_wake_lock_)
      RemoveWakeLock();
  }
}

void WakeLockServiceImplForTesting::CreateWakeLock() {
  DCHECK(!has_wake_lock_);
  has_wake_lock_ = true;
}

void WakeLockServiceImplForTesting::RemoveWakeLock() {
  DCHECK(has_wake_lock_);
  has_wake_lock_ = false;
}

}  // namespace device
