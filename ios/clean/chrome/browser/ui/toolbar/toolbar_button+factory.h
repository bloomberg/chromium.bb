// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_BUTTON_FACTORY_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_BUTTON_FACTORY_H_

#import <UIKit/UIKit.h>

#import "ios/clean/chrome/browser/ui/toolbar/toolbar_button.h"

// ToolbarButton Factory class that creates ToolbarButton objects with certain
// style and configuration.
@interface ToolbarButton (Factory)

// Back ToolbarButton.
+ (instancetype)backToolbarButton;
// Forward ToolbarButton.
+ (instancetype)forwardToolbarButton;
// Tab Switcher Strip ToolbarButton.
+ (instancetype)tabSwitcherStripToolbarButton;
// Tab Switcher Grid ToolbarButton.
+ (instancetype)tabSwitcherGridToolbarButton;
// Tools Menu ToolbarButton.
+ (instancetype)toolsMenuToolbarButton;
// Share ToolbarButton.
+ (instancetype)shareToolbarButton;
// Reload ToolbarButton.
+ (instancetype)reloadToolbarButton;
// Stop ToolbarButton.
+ (instancetype)stopToolbarButton;

@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_BUTTON_FACTORY_H_
