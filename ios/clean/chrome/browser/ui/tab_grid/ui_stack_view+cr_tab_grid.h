// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_TAB_GRID_UI_STACK_VIEW_CR_TAB_GRID_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_TAB_GRID_UI_STACK_VIEW_CR_TAB_GRID_H_

#import <UIKit/UIKit.h>

// This category provides default styling for the toolbar used in the tab grid.
@interface UIStackView (CRTabGrid)

// Constructs a tab grid toolbar with buttons.
+ (instancetype)cr_tabGridToolbarStackView;

@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_TAB_GRID_UI_STACK_VIEW_CR_TAB_GRID_H_
