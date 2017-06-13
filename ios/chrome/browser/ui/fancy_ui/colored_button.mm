// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/fancy_ui/colored_button.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ColoredButton () {
  UIColor* normalStateTint_;
  UIColor* highlightedTint_;
  UIColor* normalStateBackgroundColor_;
  UIColor* highlightedBackgroundColor_;
}

// Makes the button's tint color reflect its current state.
- (void)updateTint;

// Makes the button's background color reflect its current state.
- (void)updateBackgroundColor;

@end

@implementation ColoredButton

- (void)setTintColor:(UIColor*)color forState:(UIControlState)state {
  switch (state) {
    case UIControlStateNormal:
      normalStateTint_ = [color copy];
      break;
    case UIControlStateHighlighted:
      highlightedTint_ = [color copy];
      break;
    default:
      return;
  }

  if (normalStateTint_ || highlightedTint_)
    self.adjustsImageWhenHighlighted = NO;
  else
    self.adjustsImageWhenHighlighted = YES;
  [self updateTint];
}

- (void)setBackgroundColor:(UIColor*)color forState:(UIControlState)state {
  switch (state) {
    case UIControlStateNormal:
      normalStateBackgroundColor_ = [color copy];
      break;
    case UIControlStateHighlighted:
      highlightedBackgroundColor_ = [color copy];
      break;
    default:
      return;
  }

  [self updateBackgroundColor];
}

#pragma mark - UIControl

- (void)setHighlighted:(BOOL)highlighted {
  [super setHighlighted:highlighted];
  [self updateTint];
  [self updateBackgroundColor];
}

#pragma mark - Private

- (void)updateTint {
  UIColor* newTint = nil;
  switch (self.state) {
    case UIControlStateNormal:
      newTint = normalStateTint_;
      break;
    case UIControlStateHighlighted:
      newTint = highlightedTint_;
      break;
    default:
      newTint = normalStateTint_;
      break;
  }
  self.tintColor = newTint;
}

- (void)updateBackgroundColor {
  UIColor* newBackgroundColor = nil;
  switch (self.state) {
    case UIControlStateNormal:
      newBackgroundColor = normalStateBackgroundColor_;
      break;
    case UIControlStateHighlighted:
      newBackgroundColor = highlightedBackgroundColor_;
      break;
    default:
      newBackgroundColor = normalStateBackgroundColor_;
      break;
  }
  self.backgroundColor = newBackgroundColor;
}

@end
