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

#if defined(__IPHONE_10_0) && (__IPHONE_OS_VERSION_MIN_ALLOWED >= __IPHONE_10_0)
@interface FullscreenScrollEndTimingCurveProvider
    : NSObject<UITimingCurveProvider> {
  std::unique_ptr<gfx::CubicBezier> _bezier;
}
// The bezier backing the timing curve.
@property(nonatomic, readonly) const gfx::CubicBezier& bezier;
@end

@implementation FullscreenScrollEndTimingCurveProvider

- (instancetype)init {
  if (self = [super init]) {
    // Control points for Material Design CurveEaseOut curve.
    _bezier = base::MakeUnique<gfx::CubicBezier>(0.0, 0.0, 0.2, 0.1);
  }
  return self;
}

#pragma mark Accessors

- (const gfx::CubicBezier&)bezier {
  return *_bezier.get();
}

#pragma mark UITimingCurveProvider

- (UITimingCurveType)timingCurveType {
  return UITimingCurveTypeCubic;
}

- (UICubicTimingParameters*)cubicTimingParameters {
  return [[UICubicTimingParameters alloc]
      initWithControlPoint1:CGPointMake(_bezier->GetX1(), _bezier->GetY1())
              controlPoint2:CGPointMake(_bezier->GetX2(), _bezier->GetX2())];
}

- (UISpringTimingParameters*)springTimingParameters {
  return nil;
}

#pragma mark NSCoding

- (void)encodeWithCoder:(NSCoder*)aCoder {
}

- (instancetype)initWithCoder:(NSCoder*)aDecoder {
  return [[FullscreenScrollEndTimingCurveProvider alloc] init];
}

#pragma mark NSCopying

- (instancetype)copyWithZone:(NSZone*)zone {
  return [[FullscreenScrollEndTimingCurveProvider alloc] init];
}

@end

@interface FullscreenScrollEndAnimator ()
@property(nonatomic, readonly) FullscreenScrollEndTimingCurveProvider* curve;
@end

@implementation FullscreenScrollEndAnimator
@synthesize startProgress = _startProgress;
@synthesize finalProgress = _finalProgress;
@synthesize curve = _curve;

- (instancetype)initWithStartProgress:(CGFloat)startProgress {
  FullscreenScrollEndTimingCurveProvider* curveProvider =
      [[FullscreenScrollEndTimingCurveProvider alloc] init];
  self = [super initWithDuration:ios::material::kDuration1
                timingParameters:curveProvider];
  if (self) {
    DCHECK_GE(startProgress, 0.0);
    DCHECK_LE(startProgress, 1.0);
    _startProgress = startProgress;
    _finalProgress = roundf(_startProgress);
    _curve = curveProvider;
  }
  return self;
}

#pragma mark Accessors

- (CGFloat)currentProgress {
  CGFloat interpolationFraction =
      self.curve.bezier.Solve(self.fractionComplete);
  CGFloat range = self.finalProgress - self.startProgress;
  return self.startProgress + interpolationFraction * range;
}

#pragma mark UIViewPropertyAnimator

- (BOOL)isInterruptible {
  return YES;
}

@end

#else

@implementation FullscreenScrollEndAnimator
@end

#endif  // __IPHONE_10_0
