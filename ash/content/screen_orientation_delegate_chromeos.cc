// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/content/screen_orientation_delegate_chromeos.h"

#include "ash/display/screen_orientation_controller_chromeos.h"
#include "ash/shell.h"
#include "ash/wm_window.h"
#include "content/public/browser/web_contents.h"

namespace ash {

ScreenOrientationDelegateChromeos::ScreenOrientationDelegateChromeos() {
  content::WebContents::SetScreenOrientationDelegate(this);
}

ScreenOrientationDelegateChromeos::~ScreenOrientationDelegateChromeos() {
  content::WebContents::SetScreenOrientationDelegate(nullptr);
}

bool ScreenOrientationDelegateChromeos::FullScreenRequired(
    content::WebContents* web_contents) {
  return true;
}

void ScreenOrientationDelegateChromeos::Lock(
    content::WebContents* web_contents,
    blink::WebScreenOrientationLockType lock_orientation) {
  Shell::Get()->screen_orientation_controller()->LockOrientationForWindow(
      WmWindow::Get(web_contents->GetNativeView()), lock_orientation,
      ScreenOrientationController::LockCompletionBehavior::None);
}

bool ScreenOrientationDelegateChromeos::ScreenOrientationProviderSupported() {
  return Shell::Get()
      ->screen_orientation_controller()
      ->ScreenOrientationProviderSupported();
}

void ScreenOrientationDelegateChromeos::Unlock(
    content::WebContents* web_contents) {
  Shell::Get()->screen_orientation_controller()->UnlockOrientationForWindow(
      WmWindow::Get(web_contents->GetNativeView()));
}

}  // namespace ash
