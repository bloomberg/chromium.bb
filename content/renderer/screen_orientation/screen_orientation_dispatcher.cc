// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/screen_orientation/screen_orientation_dispatcher.h"

#include "content/public/common/associated_interface_provider.h"
#include "content/public/renderer/render_frame.h"

namespace content {

using ::blink::mojom::ScreenOrientationLockResult;

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
      callback->onSuccess();
      break;
    case ScreenOrientationLockResult::
        SCREEN_ORIENTATION_LOCK_RESULT_ERROR_NOT_AVAILABLE:
      callback->onError(blink::WebLockOrientationErrorNotAvailable);
      break;
    case ScreenOrientationLockResult::
        SCREEN_ORIENTATION_LOCK_RESULT_ERROR_FULLSCREEN_REQUIRED:
      callback->onError(blink::WebLockOrientationErrorFullscreenRequired);
      break;
    case ScreenOrientationLockResult::
        SCREEN_ORIENTATION_LOCK_RESULT_ERROR_CANCELED:
      callback->onError(blink::WebLockOrientationErrorCanceled);
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
    iterator.GetCurrentValue()->onError(blink::WebLockOrientationErrorCanceled);
    pending_callbacks_.Remove(iterator.GetCurrentKey());
  }
}

void ScreenOrientationDispatcher::lockOrientation(
    blink::WebScreenOrientationLockType orientation,
    std::unique_ptr<blink::WebLockOrientationCallback> callback) {
  CancelPendingLocks();

  int request_id = pending_callbacks_.Add(std::move(callback));
  EnsureScreenOrientationService();
  screen_orientation_->LockOrientation(
      orientation,
      base::Bind(&ScreenOrientationDispatcher::OnLockOrientationResult,
                 base::Unretained(this), request_id));
}

void ScreenOrientationDispatcher::unlockOrientation() {
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
