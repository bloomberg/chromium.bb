// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_CONFIGURATION_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_CONFIGURATION_H_

#import <UIKit/UIKit.h>

#import "ios/clean/chrome/browser/ui/toolbar/toolbar_style.h"

// Toolbar configuration object giving access to styling elements.
@interface ToolbarConfiguration : NSObject

// Init the toolbar configuration with the desired |style|.
- (instancetype)initWithStyle:(ToolbarStyle)style NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Style of this configuration.
@property(nonatomic, assign) ToolbarStyle style;

// Background color of the toolbar.
@property(nonatomic, readonly) UIColor* backgroundColor;

// Background color of the omnibox.
@property(nonatomic, readonly) UIColor* omniboxBackgroundColor;

// Border color of the omnibox.
@property(nonatomic, readonly) UIColor* omniboxBorderColor;

// Color of the title of the buttons for the normal state.
@property(nonatomic, readonly) UIColor* buttonTitleNormalColor;

// Color of the title of the buttons for the highlighted state.
@property(nonatomic, readonly) UIColor* buttonTitleHighlightedColor;

@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_CONFIGURATION_H_
