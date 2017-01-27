// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/non_presenting_gvr_delegate.h"

#include "base/callback_helpers.h"
#include "chrome/browser/android/vr_shell/vr_shell.h"

namespace vr_shell {

namespace {
static constexpr long kPredictionTimeWithoutVsyncNanos = 50000000;
}  // namespace

NonPresentingGvrDelegate::NonPresentingGvrDelegate(long context)
    : task_runner_(base::ThreadTaskRunnerHandle::Get()),
      binding_(this),
      weak_ptr_factory_(this) {
  gvr_api_ = gvr::GvrApi::WrapNonOwned(reinterpret_cast<gvr_context*>(context));
}

NonPresentingGvrDelegate::~NonPresentingGvrDelegate() {
  StopVSyncLoop();
}

gvr::Sizei NonPresentingGvrDelegate::GetWebVRCompositorSurfaceSize() {
   return device::kInvalidRenderTargetSize;
}

gvr::GvrApi* NonPresentingGvrDelegate::gvr_api() {
  return gvr_api_.get();
}

void NonPresentingGvrDelegate::OnVRVsyncProviderRequest(
    device::mojom::VRVSyncProviderRequest request) {
  binding_.Close();
  binding_.Bind(std::move(request));
  binding_.set_connection_error_handler(
      base::Bind(&NonPresentingGvrDelegate::StopVSyncLoop,
                 weak_ptr_factory_.GetWeakPtr()));
  StartVSyncLoop();
}

void NonPresentingGvrDelegate::Pause() {
  vsync_task_.Cancel();
  vsync_paused_ = true;
  gvr_api_->PauseTracking();
}

void NonPresentingGvrDelegate::Resume() {
  if (!vsync_paused_)
    return;
  vsync_paused_ = false;
  StartVSyncLoop();
}

device::mojom::VRVSyncProviderRequest
NonPresentingGvrDelegate::OnSwitchToPresentingDelegate() {
  StopVSyncLoop();
  if (binding_.is_bound())
    return binding_.Unbind();
  return nullptr;
}

void NonPresentingGvrDelegate::StopVSyncLoop() {
  vsync_task_.Cancel();
  if (!callback_.is_null()) {
    base::ResetAndReturn(&callback_).Run(nullptr, base::TimeDelta(), -1);
  }
  gvr_api_->PauseTracking();
  // If the loop is stopped, it's not considered to be paused.
  vsync_paused_ = false;
}

void NonPresentingGvrDelegate::StartVSyncLoop() {
  vsync_task_.Reset(
      base::Bind(&NonPresentingGvrDelegate::OnVSync, base::Unretained(this)));
  gvr_api_->RefreshViewerProfile();
  gvr_api_->ResumeTracking();
  OnVSync();
}

void NonPresentingGvrDelegate::OnVSync() {
  base::TimeTicks now = base::TimeTicks::Now();
  base::TimeTicks target;

  // Don't run the VSync loop if we're not bound.
  if (!binding_.is_bound()) {
    return;
  }

  // Don't send VSyncs until we have a timebase/interval.
  if (vsync_interval_.is_zero())
    return;
  target = now + vsync_interval_;
  int64_t intervals = (target - vsync_timebase_) / vsync_interval_;
  target = vsync_timebase_ + intervals * vsync_interval_;
  if (!vsync_task_.IsCancelled()) {
    task_runner_->PostDelayedTask(FROM_HERE, vsync_task_.callback(),
                                  target - now);
  }

  base::TimeDelta time = intervals * vsync_interval_;
  if (!callback_.is_null()) {
    SendVSync(time, base::ResetAndReturn(&callback_));
  } else {
    pending_vsync_ = true;
    pending_time_ = time;
  }
}

void NonPresentingGvrDelegate::GetVSync(const GetVSyncCallback& callback) {
  if (!pending_vsync_) {
    if (!callback_.is_null()) {
      mojo::ReportBadMessage("Requested VSync before waiting for response to "
          "previous request.");
      return;
    }
    callback_ = callback;
    return;
  }
  pending_vsync_ = false;
  SendVSync(pending_time_, callback);
}

void NonPresentingGvrDelegate::UpdateVSyncInterval(long timebase_nanos,
                                                   double interval_seconds) {
  vsync_timebase_ = base::TimeTicks();
  vsync_timebase_ += base::TimeDelta::FromMicroseconds(timebase_nanos / 1000);
  vsync_interval_ = base::TimeDelta::FromSecondsD(interval_seconds);
  StartVSyncLoop();
}

void NonPresentingGvrDelegate::SendVSync(base::TimeDelta time,
                                         const GetVSyncCallback& callback) {
  gvr::ClockTimePoint target_time = gvr::GvrApi::GetTimePointNow();
  target_time.monotonic_system_time_nanos += kPredictionTimeWithoutVsyncNanos;

  gvr::Mat4f head_mat = gvr_api_->ApplyNeckModel(
      gvr_api_->GetHeadSpaceFromStartSpaceRotation(target_time), 1.0f);
  callback.Run(VrShell::VRPosePtrFromGvrPose(head_mat), time, -1);
}

}  // namespace vr_shell
