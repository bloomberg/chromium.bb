// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FANCY_UI_TINTED_BUTTON_H_
#define IOS_CHROME_BROWSER_UI_FANCY_UI_TINTED_BUTTON_H_

#import <UIKit/UIKit.h>

// Subclass of UIButton whose tint color changes depending on its state.
// If the value is not specified for a state, uses the UIControlStateNormal
// value. If the UIControlStateNormal value is not set, defaults to the system's
// value.
@interface TintedButton : UIButton

// Sets the tint color of a button to use for the specified state.
// Currently only supports |UIControlStateNormal| and
// |UIControlStateHighlighted|.
- (void)setTintColor:(UIColor*)color forState:(UIControlState)state;

@end

#endif  // IOS_CHROME_BROWSER_UI_FANCY_UI_TINTED_BUTTON_H_
