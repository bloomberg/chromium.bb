// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_TOOLS_TOOLS_MENU_MODEL_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_TOOLS_TOOLS_MENU_MODEL_H_

#import <Foundation/Foundation.h>

// Total number of possible menu items.
const int kToolsMenuNumberOfItems = 15;

// Struct for a single MenuModelItem.
struct MenuModelItem {
  // Item Title ID.
  int title_id;
  // Item Accessibility ID.
  // TODO(crbug.com/682880): Move tools_menu_constants.h file to shared/.
  NSString* accessibility_id;
  // The ToolbarType on which the item is visible.
  int toolbar_types;
  // |visibility| is applied if a menu item is included for a given
  // |toolbar_types|. A value of 0 means the menu item is always visible for
  // the given |toolbar_types|.
  int visibility;
  // The selector name which will be called on the dispatcher, when the
  // item is selected.
  NSString* selector;
  // If visible, this is the condition in which the item will be enabled.
  int enabled;
};

// Menu items can be marked as visible or not when Incognito is enabled.
// The following bits are used for |visibility| field in |MenuModelItem|.
typedef NS_OPTIONS(NSUInteger, ItemVisible) {
  // clang-format off
  ItemVisibleAlways            = 0,
  ItemVisibleIncognitoOnly     = 1 << 0,
  ItemVisibleNotIncognitoOnly  = 1 << 1,
  // clang-format on
};

// Menu items can be enabled or not based on the ToolsMenuConfiguration.
// The following bits are used for |enabled| field in |MenuModelItem|.
typedef NS_OPTIONS(NSUInteger, ItemEnable) {
  // clang-format off
  ItemEnabledAlways            = 0,
  ItemEnabledWhenOpenTabs      = 1 << 0,
  ItemEnabledNotInNTP          = 1 << 1,
  // clang-format on
};

// Flags for different toolbar types.
typedef NS_OPTIONS(NSUInteger, ToolbarType) {
  // clang-format off
  ToolbarTypeNone            = 0,
  ToolbarTypeWeb             = 1 << 0,
  ToolbarTypeSwitcher        = 1 << 1,
  ToolbarTypeAll             = ToolbarTypeWeb | ToolbarTypeSwitcher,
  // clang-format on
};

// All possible items.
extern const MenuModelItem itemsModelList[kToolsMenuNumberOfItems];

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_TOOLS_TOOLS_MENU_MODEL_H_
