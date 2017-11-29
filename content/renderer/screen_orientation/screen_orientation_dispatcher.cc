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
  blink::WebLockOrientationCallback* callback =
      pending_callbacks_.Lookup(request_id);
  if (!callback)
    return;

  switch (result) {
    case ScreenOrientationLockResult::SCREEN_ORIENTATION_LOCK_RESULT_SUCCESS:
      callback->OnSuccess();
      break;
    case ScreenOrientationLockResult::
        SCREEN_ORIENTATION_LOCK_RESULT_ERROR_NOT_AVAILABLE:
      callback->OnError(blink::kWebLockOrientationErrorNotAvailable);
      break;
    case ScreenOrientationLockResult::
        SCREEN_ORIENTATION_LOCK_RESULT_ERROR_FULLSCREEN_REQUIRED:
      callback->OnError(blink::kWebLockOrientationErrorFullscreenRequired);
      break;
    case ScreenOrientationLockResult::
        SCREEN_ORIENTATION_LOCK_RESULT_ERROR_CANCELED:
      callback->OnError(blink::kWebLockOrientationErrorCanceled);
      break;
    default:
      NOTREACHED();
      break;
  }

  pending_callbacks_.Remove(request_id);
}

void ScreenOrientationDispatcher::CancelPendingLocks() {
  for (CallbackMap::Iterator<blink::WebLockOrientationCallback>
       iterator(&pending_callbacks_); !iterator.IsAtEnd(); iterator.Advance()) {
    iterator.GetCurrentValue()->OnError(
        blink::kWebLockOrientationErrorCanceled);
    pending_callbacks_.Remove(iterator.GetCurrentKey());
  }
}

void ScreenOrientationDispatcher::LockOrientation(
    blink::WebScreenOrientationLockType orientation,
    std::unique_ptr<blink::WebLockOrientationCallback> callback) {
  CancelPendingLocks();

  int request_id = pending_callbacks_.Add(std::move(callback));
  EnsureScreenOrientationService();
  screen_orientation_->LockOrientation(
      orientation,
      base::BindOnce(&ScreenOrientationDispatcher::OnLockOrientationResult,
                     base::Unretained(this), request_id));
}

void ScreenOrientationDispatcher::UnlockOrientation() {
  CancelPendingLocks();
  EnsureScreenOrientationService();
  screen_orientation_->UnlockOrientation();
}

void ScreenOrientationDispatcher::EnsureScreenOrientationService() {
  if (!screen_orientation_) {
    render_frame()->GetRemoteAssociatedInterfaces()->GetInterface(
        &screen_orientation_);
  }
}

int ScreenOrientationDispatcher::GetRequestIdForTests() {
  if (pending_callbacks_.IsEmpty())
    return -1;
  CallbackMap::Iterator<blink::WebLockOrientationCallback> iterator(
      &pending_callbacks_);
  return iterator.GetCurrentKey();
}

}  // namespace content
