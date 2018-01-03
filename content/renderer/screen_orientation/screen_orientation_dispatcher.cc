// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/screen_orientation/screen_orientation_dispatcher.h"

#include "content/public/renderer/render_frame.h"
#include "third_party/WebKit/common/associated_interfaces/associated_interface_provider.h"

namespace content {

using device::mojom::ScreenOrientationLockResult;

ScreenOrientationDispatcher::ScreenOrientationDispatcher(
    RenderFrame* render_frame)
    : RenderFrameObserver(render_frame) {
}

ScreenOrientationDispatcher::~ScreenOrientationDispatcher() {
}

void ScreenOrientationDispatcher::OnDestruct() {
  delete this;
}

void ScreenOrientationDispatcher::OnLockOrientationResult(
    int request_id,
    ScreenOrientationLockResult result) {
  if (!pending_callback_ || request_id != request_id_)
    return;

  switch (result) {
    case ScreenOrientationLockResult::SCREEN_ORIENTATION_LOCK_RESULT_SUCCESS:
      pending_callback_->OnSuccess();
      break;
    case ScreenOrientationLockResult::
        SCREEN_ORIENTATION_LOCK_RESULT_ERROR_NOT_AVAILABLE:
      pending_callback_->OnError(blink::kWebLockOrientationErrorNotAvailable);
      break;
    case ScreenOrientationLockResult::
        SCREEN_ORIENTATION_LOCK_RESULT_ERROR_FULLSCREEN_REQUIRED:
      pending_callback_->OnError(
          blink::kWebLockOrientationErrorFullscreenRequired);
      break;
    case ScreenOrientationLockResult::
        SCREEN_ORIENTATION_LOCK_RESULT_ERROR_CANCELED:
      pending_callback_->OnError(blink::kWebLockOrientationErrorCanceled);
      break;
    default:
      NOTREACHED();
      break;
  }

  pending_callback_.reset();
}

void ScreenOrientationDispatcher::CancelPendingLocks() {
  if (!pending_callback_)
    return;
  pending_callback_->OnError(blink::kWebLockOrientationErrorCanceled);
  pending_callback_.reset();
}

void ScreenOrientationDispatcher::LockOrientation(
    blink::WebScreenOrientationLockType orientation,
    std::unique_ptr<blink::WebLockOrientationCallback> callback) {
  CancelPendingLocks();

  pending_callback_ = std::move(callback);
  EnsureScreenOrientationService();
  screen_orientation_->LockOrientation(
      orientation,
      base::BindOnce(&ScreenOrientationDispatcher::OnLockOrientationResult,
                     base::Unretained(this), ++request_id_));
}

void ScreenOrientationDispatcher::UnlockOrientation() {
  CancelPendingLocks();
  EnsureScreenOrientationService();
  screen_orientation_->UnlockOrientation();
}

int ScreenOrientationDispatcher::GetRequestIdForTests() {
  return pending_callback_ ? request_id_ : -1;
}

void ScreenOrientationDispatcher::EnsureScreenOrientationService() {
  if (!screen_orientation_) {
    render_frame()->GetRemoteAssociatedInterfaces()->GetInterface(
        &screen_orientation_);
  }
}

}  // namespace content
