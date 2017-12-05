// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/fullscreen/test/test_fullscreen_controller_observer.h"

#import "ios/chrome/browser/ui/fullscreen/fullscreen_scroll_end_animator.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

void TestFullscreenControllerObserver::FullscreenProgressUpdated(
    FullscreenController* controller,
    CGFloat progress) {
  progress_ = progress;
}
void TestFullscreenControllerObserver::FullscreenEnabledStateChanged(
    FullscreenController* controller,
    bool enabled) {
  enabled_ = enabled;
}
void TestFullscreenControllerObserver::FullscreenScrollEventEnded(
    FullscreenController* controller,
    FullscreenScrollEndAnimator* animator) {
  animator_ = animator;
  // An animator must have at least one animation block when started, so insert
  // a no-op block.
  [animator_ addAnimations:^{
  }];
}
