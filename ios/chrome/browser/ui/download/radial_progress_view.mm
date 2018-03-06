// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/download/radial_progress_view.h"

#import <QuartzCore/QuartzCore.h>

#include "base/mac/foundation_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface RadialProgressView ()
// CALayer that backs this view up.
@property(nonatomic, readonly) CAShapeLayer* shapeLayer;
@end

@implementation RadialProgressView

@synthesize progress = _progress;
@synthesize lineWidth = _lineWidth;

#pragma mark - UIView overrides

+ (Class)layerClass {
  return [CAShapeLayer class];
}

- (void)willMoveToSuperview:(UIView*)newSuperview {
  if (newSuperview) {
    self.shapeLayer.fillColor = [UIColor clearColor].CGColor;
    self.shapeLayer.strokeColor = self.tintColor.CGColor;
    self.shapeLayer.lineWidth = self.lineWidth;
  }
}

#pragma mark - Public

- (void)setProgress:(float)progress {
  if (_progress == progress)
    return;

  _progress = progress;
  CGPoint center =
      CGPointMake(CGRectGetMidX(self.bounds), CGRectGetMidY(self.bounds));
  CGFloat radius = CGRectGetWidth(self.bounds) / 2 - 1;
  UIBezierPath* path =
      [UIBezierPath bezierPathWithArcCenter:center
                                     radius:radius
                                 startAngle:0 - M_PI_2  // 12 o'clock
                                   endAngle:M_PI * 2 * progress - M_PI_2
                                  clockwise:YES];
  self.shapeLayer.path = path.CGPath;
}

#pragma mark - Private

- (CAShapeLayer*)shapeLayer {
  return base::mac::ObjCCastStrict<CAShapeLayer>(self.layer);
}

@end
