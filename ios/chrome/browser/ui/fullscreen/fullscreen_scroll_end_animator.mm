// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/fullscreen/fullscreen_scroll_end_animator.h"

#include <math.h>
#include <algorithm>
#include <memory>

#include "base/logging.h"
#import "ios/chrome/common/material_timing.h"
#include "ui/gfx/geometry/cubic_bezier.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation FullscreenScrollEndAnimator
@synthesize finalProgress = _finalProgress;

- (instancetype)initWithStartProgress:(CGFloat)startProgress {
  // Scale the duration by the progress delta traversed in this animation.
  // Since |finalProgress - startProgress| <= 0.5, the delta is multiplied by
  // 2.0.
  CGFloat finalProgress = roundf(startProgress);
  NSTimeInterval duration =
      2.0 * fabs(finalProgress - startProgress) * ios::material::kDuration1;
  if (self = [super initWithStartProgress:startProgress duration:duration]) {
    _finalProgress = finalProgress;
  }
  return self;
}

@end
