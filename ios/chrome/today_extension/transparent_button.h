// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TODAY_EXTENSION_TRANSPARENT_BUTTON_H_
#define IOS_CHROME_TODAY_EXTENSION_TRANSPARENT_BUTTON_H_

#import <UIKit/UIKit.h>

// A button that has a transparent background with Vibrancy effect and an ink
// ripple effect.
@interface TransparentButton : UIButton

- (instancetype)initWithFrame:(CGRect)frame;

// A container view to add subviews to the button.
@property(nonatomic, readonly) UIView* contentView;

// The color of the border of the button.
@property(nonatomic, retain) UIColor* borderColor;

// The width of the border of the button.
@property(nonatomic) CGFloat borderWidth;

// The ink color.
@property(nonatomic, retain) UIColor* inkColor;

// The corner radius of the button.
@property(nonatomic, assign) CGFloat cornerRadius;

@end

#endif  // IOS_CHROME_TODAY_EXTENSION_TRANSPARENT_BUTTON_H_
