// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/wake_lock/wake_lock_service_context.h"

#include <utility>

#include "device/power_save_blocker/power_save_blocker.h"
#include "device/wake_lock/wake_lock_service_impl.h"

namespace device {

WakeLockServiceContext::WakeLockServiceContext(
    int context_id,
    scoped_refptr<base::SingleThreadTaskRunner> file_task_runner,
    const WakeLockContextCallback& native_view_getter)
    : file_task_runner_(std::move(file_task_runner)),
      context_id_(context_id),
      native_view_getter_(native_view_getter) {}

WakeLockServiceContext::~WakeLockServiceContext() {}

void WakeLockServiceContext::GetWakeLock(
    mojom::WakeLockServiceRequest request) {
  // WakeLockServiceImpl owns itself.
  new WakeLockServiceImpl(std::move(request),
                          device::PowerSaveBlocker::PowerSaveBlockerType::
                              kPowerSaveBlockPreventDisplaySleep,
                          device::PowerSaveBlocker::Reason::kReasonOther,
                          "Wake Lock API", context_id_, native_view_getter_,
                          file_task_runner_);
}

}  // namespace device
