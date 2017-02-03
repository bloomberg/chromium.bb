// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/screen_orientation/screen_orientation.h"

#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/screen_orientation_provider.h"
#include "content/public/browser/web_contents.h"

namespace content {

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
  provider_->LockOrientation(orientation, callback);
}

void ScreenOrientation::UnlockOrientation() {
  provider_->UnlockOrientation();
}

void ScreenOrientation::DidFinishNavigation(
    NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame() ||
      !navigation_handle->HasCommitted() ||
      navigation_handle->IsSamePage()) {
    return;
  }
  provider_->UnlockOrientation();
}

ScreenOrientationProvider* ScreenOrientation::GetScreenOrientationProvider() {
  return provider_.get();
}

}  // namespace content
