// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/download/md_download_item_progress_indicator.h"

#import <QuartzCore/QuartzCore.h>

#import "base/mac/scoped_cftyperef.h"
#import "base/mac/scoped_nsobject.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/themes/theme_service.h"
#import "chrome/browser/ui/cocoa/themed_window.h"
#import "ui/base/cocoa/quartzcore_additions.h"

namespace {

// An indeterminate progress indicator is a moving 50° arc…
constexpr CGFloat kIndeterminateArcSize = 50 / 360.0;
// …which completes a rotation around the circle every 4.5 seconds.
constexpr CGFloat kIndeterminateAnimationDuration = 4.5;

}  // namespace

@implementation MDDownloadItemProgressIndicator {
  CAShapeLayer* progressShapeLayer_;
}

@synthesize progress = progress_;

- (void)updateProgress {
  if (progress_ < 0) {
    progressShapeLayer_.strokeStart = 0;
    progressShapeLayer_.strokeEnd = kIndeterminateArcSize;
    CABasicAnimation* anim =
        [CABasicAnimation animationWithKeyPath:@"transform.rotation.z"];
    anim.byValue = @(M_PI * -2);
    anim.duration = kIndeterminateAnimationDuration;
    anim.repeatCount = HUGE_VALF;
    [progressShapeLayer_ addAnimation:anim forKey:@"indeterminate"];
  } else {
    [NSAnimationContext runAnimationGroup:^(NSAnimationContext* context) {
      context.duration = 0.25;
      // If the bar was in an indeterminate state, replace the continuous
      // rotation animation with a one-shot animation that resets it.
      if ([progressShapeLayer_ animationForKey:@"indeterminate"]) {
        CABasicAnimation* anim =
            [CABasicAnimation animationWithKeyPath:@"transform.rotation.z"];
        NSNumber* from = [progressShapeLayer_.presentationLayer
            valueForKeyPath:@"transform.rotation.z"];
        anim.fromValue = from;
        anim.byValue = @(-(from.doubleValue < 0 ? from.doubleValue
                                                : -(M_PI + from.doubleValue)));
        [progressShapeLayer_ addAnimation:anim forKey:@"indeterminate"];
      }
      progressShapeLayer_.strokeEnd = progress_;
    }
                        completionHandler:nil];
  }
}

- (void)setProgress:(CGFloat)progress {
  if (progress_ == progress)
    return;
  progress_ = progress;
  [self updateProgress];
  NSAccessibilityPostNotification(self,
                                  NSAccessibilityValueChangedNotification);
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
  block(animGroup);
  progressShapeLayer_.hidden = YES;
  [progressShapeLayer_ addAnimation:animGroup forKey:nil];
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
  base::scoped_nsobject<CAShapeLayer> progressShapeLayer(
      [[CAShapeLayer alloc] init]);
  progressShapeLayer_ = progressShapeLayer;
  progressShapeLayer_.bounds = layer.bounds;
  progressShapeLayer_.autoresizingMask =
      kCALayerWidthSizable | kCALayerHeightSizable;
  progressShapeLayer_.lineWidth = 2;
  [self updateProgress];
  [layer addSublayer:progressShapeLayer_];
  return layer;
}

- (void)viewWillDraw {
  {
    base::ScopedCFTypeRef<CGMutablePathRef> path(CGPathCreateMutable());
    CGPathAddArc(path, nullptr, NSMidX(self.layer.bounds),
                 NSMidY(self.layer.bounds), NSWidth(self.layer.bounds) / 2 - 1,
                 2 * M_PI + M_PI_2, M_PI_2, true);
    progressShapeLayer_.path = path;
  }
  [CATransaction begin];
  [CATransaction setDisableActions:YES];
  const ui::ThemeProvider* provider = [[self window] themeProvider];
  NSColor* indicatorColor =
      provider->GetNSColor(ThemeProperties::COLOR_TAB_THROBBER_SPINNING);
  progressShapeLayer_.strokeColor = indicatorColor.CGColor;
  progressShapeLayer_.fillColor =
      provider->ShouldIncreaseContrast()
          ? CGColorGetConstantColor(kCGColorClear)
          : [indicatorColor colorWithAlphaComponent:0.2].CGColor;
  [CATransaction commit];
  [super viewWillDraw];
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
