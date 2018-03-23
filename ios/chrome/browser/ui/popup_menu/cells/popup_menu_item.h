// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_POPUP_MENU_CELLS_POPUP_MENU_ITEM_H_
#define IOS_CHROME_BROWSER_UI_POPUP_MENU_CELLS_POPUP_MENU_ITEM_H_

#import <UIKit/UIKit.h>

// Identifier for the action associated with a popup menu item.
typedef NS_ENUM(NSInteger, PopupMenuAction) {
  PopupMenuActionReload,
  PopupMenuActionStop,
  PopupMenuActionOpenNewTab,
  PopupMenuActionOpenNewIncognitoTab,
  PopupMenuActionReadLater,
  PopupMenuActionRequestDesktop,
  PopupMenuActionRequestMobile,
  PopupMenuActionSiteInformation,
  PopupMenuActionReportIssue,
  PopupMenuActionHelp,
  PopupMenuActionBookmarks,
  PopupMenuActionReadingList,
  PopupMenuActionRecentTabs,
  PopupMenuActionHistory,
  PopupMenuActionSettings,
};

// Protocol defining a popup item.
@protocol PopupMenuItem

// Action identifier for the popup item.
@property(nonatomic, assign) PopupMenuAction actionIdentifier;

// Returns the size needed to display the cell associated with this item.
- (CGSize)cellSizeForWidth:(CGFloat)width;

@end

#endif  // IOS_CHROME_BROWSER_UI_POPUP_MENU_CELLS_POPUP_MENU_ITEM_H_
