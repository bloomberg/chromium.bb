// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FANCY_UI_COLORED_BUTTON_H_
#define IOS_CHROME_BROWSER_UI_FANCY_UI_COLORED_BUTTON_H_

#import <UIKit/UIKit.h>

// Subclass of UIButton whose tint and background colors can changes depending
// on the button's state.
// If a color is not specified for a state, uses the UIControlStateNormal
// value. If the UIControlStateNormal value is not set, defaults to the system's
// value.
@interface ColoredButton : UIButton

// Sets the tint color of a button to use for the specified state.
// Currently only supports |UIControlStateNormal| and
// |UIControlStateHighlighted|.
- (void)setTintColor:(UIColor*)color forState:(UIControlState)state;

// Sets the background color of a button to use for the specified state.
// Currently only supports |UIControlStateNormal| and
// |UIControlStateHighlighted|.
- (void)setBackgroundColor:(UIColor*)color forState:(UIControlState)state;

@end

#endif  // IOS_CHROME_BROWSER_UI_FANCY_UI_COLORED_BUTTON_H_
