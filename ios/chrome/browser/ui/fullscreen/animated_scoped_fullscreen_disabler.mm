// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/fullscreen/animated_scoped_fullscreen_disabler.h"

#include "base/logging.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller.h"
#import "ios/chrome/common/material_timing.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

AnimatedScopedFullscreenDisabler::AnimatedScopedFullscreenDisabler(
    FullscreenController* controller)
    : controller_(controller) {
  DCHECK(controller_);
}

AnimatedScopedFullscreenDisabler::~AnimatedScopedFullscreenDisabler() {
  if (disabling_)
    controller_->DecrementDisabledCounter();
}

void AnimatedScopedFullscreenDisabler::AddObserver(
    AnimatedScopedFullscreenDisablerObserver* observer) {
  observers_.AddObserver(observer);
}

void AnimatedScopedFullscreenDisabler::RemoveObserver(
    AnimatedScopedFullscreenDisablerObserver* observer) {
  observers_.RemoveObserver(observer);
}

void AnimatedScopedFullscreenDisabler::StartAnimation() {
  // StartAnimation() should be idempotent, so early return if this disabler has
  // already incremented the disabled counter.
  if (disabling_)
    return;
  disabling_ = true;

  if (controller_->IsEnabled()) {
    // Increment the disabled counter in an animation block if the controller is
    // not already disabled.
    for (auto& observer : observers_) {
      observer.FullscreenDisablingAnimationDidStart(this);
    }
    [UIView animateWithDuration:ios::material::kDuration1
        animations:^{
          controller_->IncrementDisabledCounter();
        }
        completion:^(BOOL finished) {
          OnAnimationFinished();
        }];
  } else {
    // If |controller_| is already disabled, no animation is necessary.
    controller_->IncrementDisabledCounter();
  }
}

void AnimatedScopedFullscreenDisabler::OnAnimationFinished() {
  for (auto& observer : observers_) {
    observer.FullscreenDisablingAnimationDidFinish(this);
  }
}
