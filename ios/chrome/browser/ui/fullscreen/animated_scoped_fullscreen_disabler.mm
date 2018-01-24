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
  [UIView animateWithDuration:ios::material::kDuration1
                   animations:^{
                     controller_->IncrementDisabledCounter();
                   }
                   completion:nil];
}

AnimatedScopedFullscreenDisabler::~AnimatedScopedFullscreenDisabler() {
  controller_->DecrementDisabledCounter();
}
