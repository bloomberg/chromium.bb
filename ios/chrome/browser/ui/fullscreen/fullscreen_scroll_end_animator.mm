// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/fullscreen/fullscreen_scroll_end_animator.h"

#include <math.h>
#include <algorithm>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#import "ios/chrome/common/material_timing.h"
#include "ui/gfx/geometry/cubic_bezier.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface FullscreenScrollEndAnimator () {
  // The bezier backing the timing curve.
  std::unique_ptr<gfx::CubicBezier> _bezier;
}
@end

@implementation FullscreenScrollEndAnimator
@synthesize startProgress = _startProgress;
@synthesize finalProgress = _finalProgress;

- (instancetype)initWithStartProgress:(CGFloat)startProgress {
  // Control points for Material Design CurveEaseOut curve.
  UICubicTimingParameters* timingParams = [[UICubicTimingParameters alloc]
      initWithControlPoint1:CGPointMake(0.0, 0.0)
              controlPoint2:CGPointMake(0.2, 0.1)];
  self = [super initWithDuration:ios::material::kDuration1
                timingParameters:timingParams];
  if (self) {
    DCHECK_GE(startProgress, 0.0);
    DCHECK_LE(startProgress, 1.0);
    _startProgress = startProgress;
    _finalProgress = roundf(_startProgress);
    _bezier = base::MakeUnique<gfx::CubicBezier>(
        timingParams.controlPoint1.x, timingParams.controlPoint1.y,
        timingParams.controlPoint2.x, timingParams.controlPoint2.y);
  }
  return self;
}

#pragma mark Accessors

- (CGFloat)currentProgress {
  CGFloat interpolationFraction = _bezier->Solve(self.fractionComplete);
  CGFloat range = self.finalProgress - self.startProgress;
  return self.startProgress + interpolationFraction * range;
}

#pragma mark UIViewPropertyAnimator

- (BOOL)isInterruptible {
  return YES;
}

@end
