// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/screen_orientation/mock_screen_orientation_controller.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "third_party/WebKit/public/platform/WebScreenOrientationListener.h"

namespace content {

MockScreenOrientationController::MockScreenOrientationController()
    : current_lock_(blink::WebScreenOrientationLockDefault),
      device_orientation_(blink::WebScreenOrientationPortraitPrimary),
      current_orientation_(blink::WebScreenOrientationPortraitPrimary),
      listener_(NULL) {
  // Since MockScreenOrientationController is held by LazyInstance reference,
  // add this ref for it.
  AddRef();
}

MockScreenOrientationController::~MockScreenOrientationController() {
}

void MockScreenOrientationController::SetListener(
    blink::WebScreenOrientationListener* listener) {
  listener_ = listener;
}

void MockScreenOrientationController::ResetData() {
  current_lock_ = blink::WebScreenOrientationLockDefault;
  device_orientation_ = blink::WebScreenOrientationPortraitPrimary;
  current_orientation_ = blink::WebScreenOrientationPortraitPrimary;
}

void MockScreenOrientationController::UpdateLock(
    blink::WebScreenOrientationLockType lock) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&MockScreenOrientationController::UpdateLockSync, this, lock));
}
void MockScreenOrientationController::UpdateLockSync(
    blink::WebScreenOrientationLockType lock) {
  DCHECK(lock != blink::WebScreenOrientationLockDefault);
  current_lock_ = lock;
  if (!IsOrientationAllowedByCurrentLock(current_orientation_))
    UpdateScreenOrientation(SuitableOrientationForCurrentLock());
}

void MockScreenOrientationController::ResetLock() {
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&MockScreenOrientationController::ResetLockSync, this));
}

void MockScreenOrientationController::ResetLockSync() {
  bool will_screen_orientation_need_updating =
      !IsOrientationAllowedByCurrentLock(device_orientation_);
  current_lock_ = blink::WebScreenOrientationLockDefault;
  if (will_screen_orientation_need_updating)
    UpdateScreenOrientation(device_orientation_);
}

void MockScreenOrientationController::UpdateDeviceOrientation(
    blink::WebScreenOrientationType orientation) {
  if (device_orientation_ == orientation)
    return;
  device_orientation_ = orientation;
  if (!IsOrientationAllowedByCurrentLock(orientation))
    return;
  UpdateScreenOrientation(orientation);
}

void MockScreenOrientationController::UpdateScreenOrientation(
    blink::WebScreenOrientationType orientation) {
  if (current_orientation_ == orientation)
    return;
  current_orientation_ = orientation;
  if (listener_)
    listener_->didChangeScreenOrientation(current_orientation_);
}

bool MockScreenOrientationController::IsOrientationAllowedByCurrentLock(
    blink::WebScreenOrientationType orientation) {
  if (current_lock_ == blink::WebScreenOrientationLockDefault ||
      current_lock_ == blink::WebScreenOrientationLockAny) {
    return true;
  }

  switch (orientation) {
    case blink::WebScreenOrientationPortraitPrimary:
      return current_lock_ == blink::WebScreenOrientationLockPortraitPrimary ||
             current_lock_ == blink::WebScreenOrientationLockPortrait;
    case blink::WebScreenOrientationPortraitSecondary:
      return current_lock_ ==
                 blink::WebScreenOrientationLockPortraitSecondary ||
             current_lock_ == blink::WebScreenOrientationLockPortrait;
    case blink::WebScreenOrientationLandscapePrimary:
      return current_lock_ == blink::WebScreenOrientationLockLandscapePrimary ||
             current_lock_ == blink::WebScreenOrientationLockLandscape;
    case blink::WebScreenOrientationLandscapeSecondary:
      return current_lock_ ==
                 blink::WebScreenOrientationLockLandscapeSecondary ||
             current_lock_ == blink::WebScreenOrientationLockLandscape;
    default:
      return false;
  }
}

blink::WebScreenOrientationType
MockScreenOrientationController::SuitableOrientationForCurrentLock() {
  switch (current_lock_) {
    case blink::WebScreenOrientationLockPortraitSecondary:
      return blink::WebScreenOrientationPortraitSecondary;
    case blink::WebScreenOrientationLockLandscapePrimary:
    case blink::WebScreenOrientationLockLandscape:
      return blink::WebScreenOrientationLandscapePrimary;
    case blink::WebScreenOrientationLockLandscapeSecondary:
      return blink::WebScreenOrientationLandscapePrimary;
    default:
      return blink::WebScreenOrientationPortraitPrimary;
  }
}

} // namespace content
