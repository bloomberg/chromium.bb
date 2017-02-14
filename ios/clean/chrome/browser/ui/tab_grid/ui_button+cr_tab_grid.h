// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_TAB_GRID_UI_BUTTON_CR_TAB_GRID_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_TAB_GRID_UI_BUTTON_CR_TAB_GRID_H_

#import <UIKit/UIKit.h>

// This category provides default styling for buttons used in the tab grid.
@interface UIButton (CRTabGrid)

// Constructs done button in the tab grid toolbar.
+ (instancetype)cr_tabGridToolbarDoneButton;

// Constructs the incognito button in the tab grid toolbar.
+ (instancetype)cr_tabGridToolbarIncognitoButton;

// Constructs the menu button in the tab grid toolbar.
+ (instancetype)cr_tabGridToolbarMenuButton;

@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_TAB_GRID_UI_BUTTON_CR_TAB_GRID_H_
