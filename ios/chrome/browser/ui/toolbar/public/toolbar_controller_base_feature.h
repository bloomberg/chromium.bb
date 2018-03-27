// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_CONTROLLER_BASE_FEATURE_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_CONTROLLER_BASE_FEATURE_H_

#import <Foundation/Foundation.h>

#include "base/feature_list.h"

// Enum for the different icons for the search button.
typedef NS_ENUM(NSUInteger, ToolbarSearchButtonIcon) {
  ToolbarSearchButtonIconGrey,
  ToolbarSearchButtonIconColorful,
  ToolbarSearchButtonIconMagnifying,
};

// Enum for the different positions of the toolbars' buttons.
typedef NS_ENUM(NSUInteger, ToolbarButtonPosition) {
  ToolbarButtonPositionNavigationBottomNoTop,
  ToolbarButtonPositionNavigationBottomShareTop,
  ToolbarButtonPositionNavigationTop,
};

// Feature to choose whether to use the memex prototype tab switcher or the
// regular native tab switcher.
extern const base::Feature kMemexTabSwitcher;

// Switch with different values for the layout of the toolbar buttons.
extern const char kToolbarButtonPositionsSwitch[];

ToolbarButtonPosition PositionForCurrentProcess();

// Switch with the different value for thesearch icon on the bottom adaptive
// toolbar.
// TODO(crbug.com/826192): Remove this.
extern const char kIconSearchButtonSwitch[];

extern const char kIconSearchButtonGrey[];
extern const char kIconSearchButtonColorful[];
extern const char kIconSearchButtonMagnifying[];

ToolbarSearchButtonIcon IconForSearchButton();

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_CONTROLLER_BASE_FEATURE_H_
