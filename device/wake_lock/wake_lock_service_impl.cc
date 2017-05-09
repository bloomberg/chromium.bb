// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/wake_lock/wake_lock_service_impl.h"

#include <utility>

#include "base/memory/ptr_util.h"

namespace device {

WakeLockServiceImpl::WakeLockServiceImpl(
    mojom::WakeLockServiceRequest request,
    device::PowerSaveBlocker::PowerSaveBlockerType type,
    device::PowerSaveBlocker::Reason reason,
    const std::string& description,
    int context_id,
    WakeLockContextCallback native_view_getter,
    scoped_refptr<base::SingleThreadTaskRunner> file_task_runner)
    : type_(type),
      reason_(reason),
      description_(base::MakeUnique<std::string>(description)),
      num_lock_requests_(0),
#if defined(OS_ANDROID)
      context_id_(context_id),
      native_view_getter_(native_view_getter),
#endif
      main_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      file_task_runner_(std::move(file_task_runner)) {
  AddClient(std::move(request));
  binding_set_.set_connection_error_handler(base::Bind(
      &WakeLockServiceImpl::OnConnectionError, base::Unretained(this)));
}

WakeLockServiceImpl::~WakeLockServiceImpl() {}

void WakeLockServiceImpl::AddClient(mojom::WakeLockServiceRequest request) {
  binding_set_.AddBinding(this, std::move(request),
                          base::MakeUnique<bool>(false));
}

void WakeLockServiceImpl::RequestWakeLock() {
  DCHECK(main_task_runner_->RunsTasksOnCurrentThread());
  DCHECK(binding_set_.dispatch_context());

  // Uses the Context to get the outstanding status of current binding.
  // Two consecutive requests from the same client should be coalesced
  // as one request.
  if (*binding_set_.dispatch_context())
    return;

  *binding_set_.dispatch_context() = true;
  num_lock_requests_++;
  UpdateWakeLock();
}

void WakeLockServiceImpl::CancelWakeLock() {
  DCHECK(main_task_runner_->RunsTasksOnCurrentThread());
  DCHECK(binding_set_.dispatch_context());

  if (!(*binding_set_.dispatch_context()))
    return;

  *binding_set_.dispatch_context() = false;
  if (num_lock_requests_ > 0) {
    num_lock_requests_--;
    UpdateWakeLock();
  }
}

void WakeLockServiceImpl::HasWakeLockForTests(
    const HasWakeLockForTestsCallback& callback) {
  callback.Run(!!wake_lock_);
}
void WakeLockServiceImpl::UpdateWakeLock() {
  DCHECK(num_lock_requests_ >= 0);

  if (num_lock_requests_) {
    if (!wake_lock_)
      CreateWakeLock();
  } else {
    if (wake_lock_)
      RemoveWakeLock();
  }
}

void WakeLockServiceImpl::CreateWakeLock() {
  DCHECK(!wake_lock_);

  if (type_ != device::PowerSaveBlocker::kPowerSaveBlockPreventDisplaySleep ||
      reason_ != device::PowerSaveBlocker::kReasonOther ||
      *description_ != "Wake Lock API") {
    // TODO(ke.he@intel.com): Fully generalize the WakeLock interface and impl.
    NOTREACHED();
    return;
  }

  wake_lock_ = base::MakeUnique<device::PowerSaveBlocker>(
      type_, reason_, *description_, main_task_runner_, file_task_runner_);

#if defined(OS_ANDROID)
  gfx::NativeView native_view = native_view_getter_.Run(context_id_);
  if (native_view)
    wake_lock_.get()->InitDisplaySleepBlocker(native_view);
#endif
}

void WakeLockServiceImpl::RemoveWakeLock() {
  DCHECK(wake_lock_);
  wake_lock_.reset();
}

void WakeLockServiceImpl::OnConnectionError() {
  DCHECK(binding_set_.dispatch_context());

  // If the error-happening client's wakelock is in outstanding status,
  // decrease the num_lock_requests and call UpdateWakeLock().
  if (*binding_set_.dispatch_context() && num_lock_requests_ > 0) {
    num_lock_requests_--;
    UpdateWakeLock();
  }

  // If |binding_set_| is empty, WakeLockServiceImpl should delele itself.
  if (binding_set_.empty())
    base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
}

}  // namespace device
