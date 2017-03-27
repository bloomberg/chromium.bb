// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/wake_lock/wake_lock_service_context.h"

#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "device/power_save_blocker/power_save_blocker.h"

namespace device {

WakeLockServiceContext::WakeLockServiceContext(
    mojom::WakeLockContextRequest request,
    int context_id,
    scoped_refptr<base::SingleThreadTaskRunner> file_task_runner,
    const WakeLockContextCallback& native_view_getter)
    : main_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      file_task_runner_(std::move(file_task_runner)),
      num_lock_requests_(0),
#if defined(OS_ANDROID)
      context_id_(context_id),
      native_view_getter_(native_view_getter),
#endif
      context_binding_(this, std::move(request)),
      context_binding_encountered_error_(false) {
  context_binding_.set_connection_error_handler(base::Bind(
      &WakeLockServiceContext::OnContextBindingError, base::Unretained(this)));
  wake_lock_bindings_.set_connection_error_handler(
      base::Bind(&WakeLockServiceContext::DestroyIfNoLongerNeeded,
                 base::Unretained(this)));
}

WakeLockServiceContext::~WakeLockServiceContext() {}

void WakeLockServiceContext::GetWakeLock(
    mojo::InterfaceRequest<mojom::WakeLockService> request) {
  wake_lock_bindings_.AddBinding(base::MakeUnique<WakeLockServiceImpl>(this),
                                 std::move(request));
}

void WakeLockServiceContext::RequestWakeLock() {
  DCHECK(main_task_runner_->RunsTasksOnCurrentThread());
  num_lock_requests_++;
  UpdateWakeLock();
}

void WakeLockServiceContext::CancelWakeLock() {
  DCHECK(main_task_runner_->RunsTasksOnCurrentThread());
  num_lock_requests_--;
  UpdateWakeLock();
}

void WakeLockServiceContext::HasWakeLockForTests(
    const HasWakeLockForTestsCallback& callback) {
  callback.Run(!!wake_lock_);
}

void WakeLockServiceContext::CreateWakeLock() {
  DCHECK(!wake_lock_);
  wake_lock_.reset(new device::PowerSaveBlocker(
      device::PowerSaveBlocker::kPowerSaveBlockPreventDisplaySleep,
      device::PowerSaveBlocker::kReasonOther, "Wake Lock API",
      main_task_runner_, file_task_runner_));

#if defined(OS_ANDROID)
  gfx::NativeView native_view = native_view_getter_.Run(context_id_);
  if (native_view) {
    wake_lock_.get()->InitDisplaySleepBlocker(native_view);
  }
#endif
}

void WakeLockServiceContext::RemoveWakeLock() {
  DCHECK(wake_lock_);
  wake_lock_.reset();
}

void WakeLockServiceContext::UpdateWakeLock() {
  DCHECK(num_lock_requests_ >= 0);
  if (num_lock_requests_) {
    if (!wake_lock_)
      CreateWakeLock();
  } else {
    if (wake_lock_)
      RemoveWakeLock();
  }
}

void WakeLockServiceContext::OnContextBindingError() {
  context_binding_encountered_error_ = true;
  DestroyIfNoLongerNeeded();
}

void WakeLockServiceContext::DestroyIfNoLongerNeeded() {
  if (context_binding_encountered_error_ && wake_lock_bindings_.empty()) {
    // Delete this instance once there are no more live connections to it.
    // However, ensure that this instance stays alive throughout the destructor
    // of a WakeLockServiceImpl instance that might be triggering this callback.
    base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
  }
}

}  // namespace device
