// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/test_runner/mock_screen_orientation_client.h"

#include <memory>

#include "base/bind.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"

namespace test_runner {

MockScreenOrientationClient::MockScreenOrientationClient()
    : main_frame_(NULL),
      current_lock_(blink::WebScreenOrientationLockDefault),
      device_orientation_(blink::WebScreenOrientationPortraitPrimary),
      current_orientation_(blink::WebScreenOrientationPortraitPrimary),
      is_disabled_(false) {}

MockScreenOrientationClient::~MockScreenOrientationClient() {}

void MockScreenOrientationClient::ResetData() {
  current_lock_ = blink::WebScreenOrientationLockDefault;
  device_orientation_ = blink::WebScreenOrientationPortraitPrimary;
  current_orientation_ = blink::WebScreenOrientationPortraitPrimary;
  is_disabled_ = false;
}

void MockScreenOrientationClient::UpdateDeviceOrientation(
    blink::WebLocalFrame* main_frame,
    blink::WebScreenOrientationType orientation) {
  main_frame_ = main_frame;
  if (device_orientation_ == orientation)
    return;
  device_orientation_ = orientation;
  if (!IsOrientationAllowedByCurrentLock(orientation))
    return;
  UpdateScreenOrientation(orientation);
}

void MockScreenOrientationClient::UpdateScreenOrientation(
    blink::WebScreenOrientationType orientation) {
  if (current_orientation_ == orientation)
    return;
  current_orientation_ = orientation;
  if (main_frame_)
    main_frame_->sendOrientationChangeEvent();
}

blink::WebScreenOrientationType
MockScreenOrientationClient::CurrentOrientationType() const {
  return current_orientation_;
}

unsigned MockScreenOrientationClient::CurrentOrientationAngle() const {
  return OrientationTypeToAngle(current_orientation_);
}

void MockScreenOrientationClient::SetDisabled(bool disabled) {
  is_disabled_ = disabled;
}

unsigned MockScreenOrientationClient::OrientationTypeToAngle(
    blink::WebScreenOrientationType type) {
  unsigned angle;
  // FIXME(ostap): This relationship between orientationType and
  // orientationAngle is temporary. The test should be able to specify
  // the angle in addition to the orientation type.
  switch (type) {
    case blink::WebScreenOrientationLandscapePrimary:
      angle = 90;
      break;
    case blink::WebScreenOrientationLandscapeSecondary:
      angle = 270;
      break;
    case blink::WebScreenOrientationPortraitSecondary:
      angle = 180;
      break;
    default:
      angle = 0;
  }
  return angle;
}

bool MockScreenOrientationClient::IsOrientationAllowedByCurrentLock(
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

void MockScreenOrientationClient::lockOrientation(
    blink::WebScreenOrientationLockType orientation,
    std::unique_ptr<blink::WebLockOrientationCallback> callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&MockScreenOrientationClient::UpdateLockSync,
                 base::Unretained(this), orientation, base::Passed(&callback)));
}

void MockScreenOrientationClient::unlockOrientation() {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&MockScreenOrientationClient::ResetLockSync,
                            base::Unretained(this)));
}

void MockScreenOrientationClient::UpdateLockSync(
    blink::WebScreenOrientationLockType lock,
    std::unique_ptr<blink::WebLockOrientationCallback> callback) {
  DCHECK(lock != blink::WebScreenOrientationLockDefault);
  current_lock_ = lock;
  if (!IsOrientationAllowedByCurrentLock(current_orientation_))
    UpdateScreenOrientation(SuitableOrientationForCurrentLock());
  callback->onSuccess();
}

void MockScreenOrientationClient::ResetLockSync() {
  bool will_screen_orientation_need_updating =
      !IsOrientationAllowedByCurrentLock(device_orientation_);
  current_lock_ = blink::WebScreenOrientationLockDefault;
  if (will_screen_orientation_need_updating)
    UpdateScreenOrientation(device_orientation_);
}

blink::WebScreenOrientationType
MockScreenOrientationClient::SuitableOrientationForCurrentLock() {
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

}  // namespace test_runner
