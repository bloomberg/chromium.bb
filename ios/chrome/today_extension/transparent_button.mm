// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/today_extension/transparent_button.h"

#import <NotificationCenter/NotificationCenter.h>
#import <QuartzCore/QuartzCore.h>

#import "base/ios/weak_nsobject.h"
#import "base/mac/scoped_nsobject.h"
#import "ios/chrome/today_extension/ui_util.h"
#import "ios/third_party/material_components_ios/src/components/Ink/src/MaterialInk.h"

@implementation TransparentButton {
  base::scoped_nsobject<MDCInkView> _backgroundView;
  CGPoint _touchLocation;
  BOOL _highlightStatus;
  base::scoped_nsobject<UIView> _effectView;
  base::WeakNSObject<UIView> _effectContentView;
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    CGRect insideFrame = frame;
    insideFrame.origin = CGPointZero;
    [self setTranslatesAutoresizingMaskIntoConstraints:NO];

    if (!UIAccessibilityIsReduceTransparencyEnabled()) {
      UIVisualEffectView* effectView = [[UIVisualEffectView alloc]
          initWithEffect:[UIVibrancyEffect notificationCenterVibrancyEffect]];
      _effectView.reset(effectView);
      _effectContentView.reset([effectView contentView]);
    } else {
      _effectView.reset([[UIView alloc] initWithFrame:insideFrame]);
      _effectContentView.reset(_effectView);
    }
    [self addSubview:_effectView];
    ui_util::ConstrainAllSidesOfViewToView(self, _effectView);
    [_effectView setTranslatesAutoresizingMaskIntoConstraints:NO];
    [_effectView setUserInteractionEnabled:NO];

    _backgroundView.reset([[MDCInkView alloc] initWithFrame:insideFrame]);
    [_backgroundView setMaxRippleRadius:frame.size.width];
    [_effectContentView addSubview:_backgroundView];
    ui_util::ConstrainAllSidesOfViewToView(_effectContentView, _backgroundView);
    [_backgroundView setTranslatesAutoresizingMaskIntoConstraints:NO];
    [self sendSubviewToBack:_effectView];
    [self setCornerRadius:0];
  }
  return self;
}

- (UIView*)contentView {
  return _effectContentView.get();
}

- (void)setInkColor:(UIColor*)color {
  [_backgroundView setInkColor:color];
}

- (UIColor*)inkColor {
  return [_backgroundView inkColor];
}

- (void)setBorderColor:(UIColor*)color {
  [[_backgroundView layer] setBorderColor:color.CGColor];
}

- (UIColor*)borderColor {
  return [UIColor colorWithCGColor:[[_backgroundView layer] borderColor]];
}

- (void)setBorderWidth:(CGFloat)width {
  [[_backgroundView layer] setBorderWidth:width];
}

- (CGFloat)borderWidth {
  return [[_backgroundView layer] borderWidth];
}

- (void)setBackgroundColor:(UIColor*)color {
  [_backgroundView setBackgroundColor:color];
}

- (void)touchesBegan:(NSSet*)touches withEvent:(UIEvent*)event {
  UITouch* touch = [touches anyObject];
  _touchLocation = [touch locationInView:self];
  [super touchesBegan:touches withEvent:event];
}

- (void)touchesEnded:(NSSet*)touches withEvent:(UIEvent*)event {
  UITouch* touch = [touches anyObject];
  _touchLocation = [touch locationInView:self];
  [super touchesEnded:touches withEvent:event];
}

- (void)setCornerRadius:(const CGFloat)radius {
  [self layer].cornerRadius = radius;
  [_backgroundView layer].cornerRadius = radius;
}

- (CGFloat)cornerRadius {
  return [self layer].cornerRadius;
}

- (void)setHighlighted:(BOOL)highlighted {
  [super setHighlighted:highlighted];
  if (highlighted == _highlightStatus) {
    return;
  }
  _highlightStatus = highlighted;

  if (highlighted) {
    [_backgroundView startTouchBeganAnimationAtPoint:_touchLocation
                                          completion:nil];
  } else {
    [_backgroundView startTouchEndedAnimationAtPoint:_touchLocation
                                          completion:nil];
  }
}

@end
