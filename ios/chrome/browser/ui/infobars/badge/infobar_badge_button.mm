// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/badge/infobar_badge_button.h"

#import "ios/chrome/browser/ui/util/uikit_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Duration of button animations, in seconds.
const CGFloat kButtonAnimationDuration = 0.2;
// Edge insets of button.
const CGFloat kButtonEdgeInset = 6;
// Corner radius of button.
const CGFloat kButtonCornerRadius = 15;
// White alpha of the button background in a selected state.
const CGFloat kSelectedWhiteAlpha = 0.80;
// Tint color of the button in an active state.
const CGFloat kActiveTintColor = 0x1A73E8;
}  // namespace

@interface InfobarBadgeButton ()
// Indicates whether the button has already been properly configured.
@property(nonatomic, assign) BOOL didSetupSubviews;
@end

@implementation InfobarBadgeButton

- (void)willMoveToSuperview:(UIView*)newSuperview {
  if (!self.didSetupSubviews) {
    self.didSetupSubviews = YES;
    [self
        setImage:[[UIImage imageNamed:@"infobar_passwords_icon"]
                     imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate]
        forState:UIControlStateNormal];
    self.imageView.contentMode = UIViewContentModeScaleToFill;
    self.imageEdgeInsets = UIEdgeInsetsMake(kButtonEdgeInset, kButtonEdgeInset,
                                            kButtonEdgeInset, kButtonEdgeInset);
    self.layer.cornerRadius = kButtonCornerRadius;
  }
  [super willMoveToSuperview:newSuperview];
}

- (void)setSelected:(BOOL)selected {
  [UIView animateWithDuration:kButtonAnimationDuration
                   animations:^{
                     self.backgroundColor =
                         selected ? [UIColor colorWithWhite:kSelectedWhiteAlpha
                                                      alpha:1.0]
                                  : [UIColor clearColor];
                   }];
}
- (void)setActive:(BOOL)active {
  [UIView animateWithDuration:kButtonAnimationDuration
                   animations:^{
                     self.tintColor = active ? UIColorFromRGB(kActiveTintColor)
                                             : [UIColor lightGrayColor];
                   }];
}

@end
