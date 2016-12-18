// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <NotificationCenter/NotificationCenter.h>
#import <QuartzCore/QuartzCore.h>

#import "ios/chrome/today_extension/notification_center_button.h"

#import "base/mac/scoped_nsobject.h"
#include "ios/chrome/today_extension/ui_util.h"

@implementation NotificationCenterButton {
  CGFloat _horizontalPadding;
  CGFloat _verticalPadding;
  CGSize _imageSize;
  CGFloat _imageTextSeparator;
  // Shift the centered content to the front (left in LTR, right in RTL).
  CGFloat _frontShift;
}

- (instancetype)initWithTitle:(NSString*)title
                         icon:(NSString*)icon
                       target:(id)target
                       action:(SEL)action
              backgroundColor:(UIColor*)backgroundColor
                     inkColor:(UIColor*)inkColor
                   titleColor:(UIColor*)titleColor {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    [self setInkColor:inkColor];
    if ([icon length]) {
      UIImage* iconImage = [UIImage imageNamed:icon];
      [self setImage:iconImage forState:UIControlStateNormal];
      _imageSize = [iconImage size];
    } else {
      _imageSize = CGSizeZero;
    }
    [self bringSubviewToFront:[self imageView]];

    [self addTarget:target
                  action:action
        forControlEvents:UIControlEventTouchUpInside];
    [self setExclusiveTouch:YES];

    UIFont* titleFont =
        [UIFont fontWithName:@"Helvetica" size:ui_util::kTitleFontSize];
    [super setTitle:title forState:UIControlStateNormal];
    [self setBackgroundColor:backgroundColor];
    [self titleLabel].font = titleFont;
    [self titleLabel].lineBreakMode = NSLineBreakByTruncatingTail;
    [self setTitleColor:titleColor forState:UIControlStateNormal];
    [self setTitleColor:[titleColor colorWithAlphaComponent:1]
               forState:UIControlStateHighlighted];
  }
  return self;
}

- (void)setButtonSpacesSeparator:(const CGFloat)separator
                      frontShift:(const CGFloat)frontShift
               horizontalPadding:(const CGFloat)horizontalPadding
                 verticalPadding:(const CGFloat)verticalPadding {
  _horizontalPadding = horizontalPadding;
  _verticalPadding = verticalPadding;
  _imageTextSeparator = separator;
  _frontShift = frontShift;
}

@end
