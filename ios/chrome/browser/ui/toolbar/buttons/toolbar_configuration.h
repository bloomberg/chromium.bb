// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_BUTTONS_TOOLBAR_CONFIGURATION_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_BUTTONS_TOOLBAR_CONFIGURATION_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_style.h"

// Toolbar configuration object giving access to styling elements.
@interface ToolbarConfiguration : NSObject

// Init the toolbar configuration with the desired |style|.
- (instancetype)initWithStyle:(ToolbarStyle)style NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Style of this configuration.
@property(nonatomic, assign) ToolbarStyle style;

// Blur effect for the toolbar background.
@property(nonatomic, readonly) UIBlurEffect* blurEffect;

// Background color of the NTP. Used to do as if the toolbar was transparent and
// the NTP is visible behind it.
@property(nonatomic, readonly) UIColor* NTPBackgroundColor;

// Background color of the toolbar.
@property(nonatomic, readonly) UIColor* backgroundColor;

// Background color of the omnibox.
// TODO(crbug.com/800266): Remove this property.
@property(nonatomic, readonly) UIColor* omniboxBackgroundColor;

// Border color of the omnibox.
// TODO(crbug.com/800266): Remove this property.
@property(nonatomic, readonly) UIColor* omniboxBorderColor;

// Tint color of the buttons.
@property(nonatomic, readonly) UIColor* buttonsTintColor;

// Color of the title of the buttons for the normal state.
// TODO(crbug.com/800266): Remove this property.
@property(nonatomic, readonly) UIColor* buttonTitleNormalColor;

// Color of the title of the buttons for the highlighted state.
// TODO(crbug.com/800266): Remove this property.
@property(nonatomic, readonly) UIColor* buttonTitleHighlightedColor;

// Returns the background color of the location bar, with a |visibilityFactor|.
// The |visibilityFactor| is here to alter the alpha value of the background
// color. Even with a |visibilityFactor| of 1, the final color could is
// translucent.
- (UIColor*)locationBarBackgroundColorWithVisibility:(CGFloat)visibilityFactor;

// Vibrancy effect for the toolbar elements, based on the |blurEffect|. Returns
// nil of no vibrancy effect should be applied.
- (UIVisualEffect*)vibrancyEffectForBlurEffect:(UIBlurEffect*)blurEffect;

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_BUTTONS_TOOLBAR_CONFIGURATION_H_
