// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_TAB_GRID_MDC_FLOATING_BUTTON_CR_TAB_GRID_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_TAB_GRID_MDC_FLOATING_BUTTON_CR_TAB_GRID_H_

#import "ios/third_party/material_components_ios/src/components/Buttons/src/MaterialButtons.h"

// This category provides default styling for floating buttons used in TabGrid.
@interface MDCFloatingButton (CRTabGrid)

// Constructs floating button with styling defaults for new tab button in tab
// grid.
+ (instancetype)cr_tabGridNewTabButton;

// Rect for placement of floating button inside lower right corner of |rect|.
+ (CGRect)cr_frameForTabGridNewTabButtonInRect:(CGRect)rect;

@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_TAB_GRID_MDC_FLOATING_BUTTON_CR_TAB_GRID_H_
