// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/screen_orientation/screen_orientation.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/screen_orientation_provider.h"
#include "content/public/browser/web_contents.h"

namespace content {

using LockResult = device::mojom::ScreenOrientationLockResult;

ScreenOrientation::ScreenOrientation(WebContents* web_contents)
    : WebContentsObserver(web_contents),
      bindings_(web_contents, this),
      weak_factory_(this) {
  provider_.reset(new ScreenOrientationProvider(web_contents));
}

ScreenOrientation::~ScreenOrientation() = default;

void ScreenOrientation::LockOrientation(
    blink::WebScreenOrientationLockType orientation,
    const LockOrientationCallback& callback) {
  if (!on_result_callback_.is_null()) {
    NotifyLockResult(LockResult::SCREEN_ORIENTATION_LOCK_RESULT_ERROR_CANCELED);
  }

  on_result_callback_ = callback;

  provider_->LockOrientation(
      orientation,
      base::Bind(&ScreenOrientation::NotifyLockResult, base::Unretained(this)));
}

void ScreenOrientation::UnlockOrientation() {
  // Cancel any pending lock request.
  NotifyLockResult(LockResult::SCREEN_ORIENTATION_LOCK_RESULT_ERROR_CANCELED);

  provider_->UnlockOrientation();
}

void ScreenOrientation::DidNavigateMainFrame(
    const LoadCommittedDetails& details,
    const FrameNavigateParams& params) {
  if (details.is_in_page)
    return;
  provider_->UnlockOrientation();
}

void ScreenOrientation::NotifyLockResult(LockResult result) {
  if (on_result_callback_.is_null())
    return;

  base::ResetAndReturn(&on_result_callback_).Run(result);
}

ScreenOrientationProvider* ScreenOrientation::GetScreenOrientationProvider() {
  return provider_.get();
}

}  // namespace content
