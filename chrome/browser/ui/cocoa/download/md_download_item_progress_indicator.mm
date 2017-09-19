// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/download/md_download_item_progress_indicator.h"

#import <QuartzCore/QuartzCore.h>

#import "base/mac/scoped_cftyperef.h"
#import "base/mac/scoped_nsobject.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/themes/theme_service.h"
#import "chrome/browser/ui/cocoa/md_util.h"
#import "chrome/browser/ui/cocoa/themed_window.h"
#import "ui/base/cocoa/quartzcore_additions.h"

namespace {

// An indeterminate progress indicator is a moving 50° arc…
constexpr CGFloat kIndeterminateArcSize = 50 / 360.0;
// …which completes a rotation around the circle every 4.5 seconds…
constexpr CGFloat kIndeterminateAnimationDuration = 4.5;
// …stored under this key.
NSString* const kIndeterminateAnimationKey = @"indeterminate";

base::ScopedCFTypeRef<CGMutablePathRef> CirclePathWithBounds(CGRect bounds) {
  base::ScopedCFTypeRef<CGMutablePathRef> path(CGPathCreateMutable());
  CGPathAddArc(path, nullptr, NSMidX(bounds), NSMidY(bounds),
               NSWidth(bounds) / 2, 2 * M_PI + M_PI_2, M_PI_2, true);
  return path;
}

}  // namespace

@implementation MDDownloadItemProgressIndicator {
  CAShapeLayer* progressLayer_;
  CAShapeLayer* strokeLayer_;
}

@synthesize progress = progress_;
@synthesize paused = paused_;

- (void)updateProgress {
  BOOL isIndeterminate = progress_ < 0;
  if (isIndeterminate &&
      ![strokeLayer_ animationForKey:kIndeterminateAnimationKey]) {
    CABasicAnimation* anim =
        [CABasicAnimation animationWithKeyPath:@"transform.rotation.z"];
    anim.byValue = @(M_PI * -2);
    anim.duration = kIndeterminateAnimationDuration;
    anim.repeatCount = HUGE_VALF;
    [strokeLayer_ addAnimation:anim forKey:kIndeterminateAnimationKey];
  }
  [NSAnimationContext runAnimationGroup:^(NSAnimationContext* context) {
    context.duration = 0.25;
    context.timingFunction =
        CAMediaTimingFunction.cr_materialEaseInOutTimingFunction;
    // If the bar was in an indeterminate state, replace the continuous
    // rotation animation with a one-shot animation that resets it.
    CGFloat rotation =
        static_cast<NSNumber*>([strokeLayer_.presentationLayer
                                   valueForKeyPath:@"transform.rotation.z"])
            .doubleValue;
    if (!isIndeterminate && rotation) {
      CABasicAnimation* anim =
          [CABasicAnimation animationWithKeyPath:@"transform.rotation.z"];
      anim.fromValue = @(rotation);
      anim.byValue = @(-(rotation < 0 ? rotation : -(M_PI + rotation)));
      [strokeLayer_ addAnimation:anim forKey:kIndeterminateAnimationKey];
    }
    strokeLayer_.strokeEnd =
        isIndeterminate ? kIndeterminateArcSize : progress_;
  }
                      completionHandler:nil];
}

- (void)setProgress:(CGFloat)progress {
  if (progress_ == progress)
    return;
  progress_ = progress;
  [self updateProgress];
  NSAccessibilityPostNotification(self,
                                  NSAccessibilityValueChangedNotification);
}

- (void)setPaused:(BOOL)paused {
  if (paused_ == paused)
    return;
  paused_ = paused;
  if (paused_) {
    if (CAAnimation* indeterminateAnimation =
            [strokeLayer_ animationForKey:kIndeterminateAnimationKey]) {
      [strokeLayer_ setValue:[strokeLayer_.presentationLayer
                                 valueForKeyPath:@"transform.rotation.z"]
                  forKeyPath:@"transform.rotation.z"];
      [strokeLayer_ removeAnimationForKey:kIndeterminateAnimationKey];
    }
  } else {
    [strokeLayer_ setValue:@0 forKey:@"transform.rotation.z"];
    [self updateProgress];
  }
}

- (void)hideAnimated:(void (^)(CAAnimation*))block {
  // Set the progress layer's model to hidden, but also add a fade out
  // animation (which is friendly to being reversed and repeated by -complete).
  CAAnimationGroup* animGroup = [CAAnimationGroup animation];
  animGroup.animations = @[
    [CABasicAnimation cr_animationWithKeyPath:@"opacity"
                                    fromValue:@1
                                      toValue:@0],
    [CABasicAnimation cr_animationWithKeyPath:@"hidden"
                                    fromValue:@NO
                                      toValue:@NO],
  ];
  animGroup.timingFunction =
      CAMediaTimingFunction.cr_materialEaseInOutTimingFunction;
  block(animGroup);
  progressLayer_.hidden = YES;
  [progressLayer_ addAnimation:animGroup forKey:nil];
}

- (void)cancel {
  self.progress = 0;
  // If the layer is set to hidden in the same transaction you see the progress
  // bar animate, but you also see a ghostly version of it continue in its
  // current state until the hide animation completes. Core Animation bug?
  // Delaying the hide until the next transaction works around it.
  [CATransaction setCompletionBlock:^{
    [self hideAnimated:^(CAAnimation* anim) {
      anim.duration = 0.5;
    }];
  }];
}

- (void)complete {
  self.progress = 1;
  [CATransaction setCompletionBlock:^{
    [self hideAnimated:^(CAAnimation* anim) {
      anim.autoreverses = YES;
      anim.duration = 0.5;
      anim.repeatCount = 2.5;
    }];
  }];
}

// NSView overrides.

- (CALayer*)makeBackingLayer {
  CALayer* layer = [super makeBackingLayer];

  base::scoped_nsobject<CAShapeLayer> progressLayer(
      [[CAShapeLayer alloc] init]);
  progressLayer_ = progressLayer;
  progressLayer_.bounds = layer.bounds;
  progressLayer_.autoresizingMask =
      kCALayerWidthSizable | kCALayerHeightSizable;
  [layer addSublayer:progressLayer_];

  base::scoped_nsobject<CAShapeLayer> strokeLayer([[CAShapeLayer alloc] init]);
  strokeLayer_ = strokeLayer;
  strokeLayer_.bounds = progressLayer_.bounds;
  strokeLayer_.autoresizingMask = kCALayerWidthSizable | kCALayerHeightSizable;
  strokeLayer_.fillColor = nil;
  strokeLayer_.lineWidth = 1.5;
  [progressLayer_ addSublayer:strokeLayer_];

  [self updateProgress];
  return layer;
}

- (void)layout {
  [CATransaction begin];
  [CATransaction setDisableActions:YES];
  progressLayer_.path = CirclePathWithBounds(self.layer.bounds);
  strokeLayer_.path =
      CirclePathWithBounds(CGRectInset(self.layer.bounds, 0.75, 0.75));
  const ui::ThemeProvider* provider = [[self window] themeProvider];
  NSColor* indicatorColor =
      provider->GetNSColor(ThemeProperties::COLOR_TAB_THROBBER_SPINNING);
  strokeLayer_.strokeColor = indicatorColor.CGColor;
  progressLayer_.fillColor =
      provider->ShouldIncreaseContrast()
          ? CGColorGetConstantColor(kCGColorClear)
          : [indicatorColor colorWithAlphaComponent:0.2].CGColor;
  [CATransaction commit];
  [super layout];
}

// NSObject overrides.

- (BOOL)accessibilityIsIgnored {
  return NO;
}

- (id)accessibilityAttributeValue:(NSString*)attribute {
  if ([NSAccessibilityRoleAttribute isEqualToString:attribute])
    return NSAccessibilityProgressIndicatorRole;
  if (progress_ >= 0) {
    if ([NSAccessibilityValueAttribute isEqualToString:attribute])
      return @(progress_);
    if ([NSAccessibilityMinValueAttribute isEqualToString:attribute])
      return @0;
    if ([NSAccessibilityMaxValueAttribute isEqualToString:attribute])
      return @1;
  }
  return [super accessibilityAttributeValue:attribute];
}

@end
