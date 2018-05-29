// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_COMMANDS_POPUP_MENU_COMMANDS_H_
#define IOS_CHROME_BROWSER_UI_COMMANDS_POPUP_MENU_COMMANDS_H_

#import <Foundation/Foundation.h>

// Type of a popup menu command.
typedef NS_ENUM(NSInteger, PopupMenuCommandType) {
  PopupMenuCommandTypeToolsMenu,
  PopupMenuCommandTypeDefault,
};

// Commands for the popup menu.
@protocol PopupMenuCommands

// Shows the navigation history popup containing the back history.
- (void)showNavigationHistoryBackPopupMenu;
// Shows the navigation history popup containing the forward history.
- (void)showNavigationHistoryForwardPopupMenu;
// Shows the tools menu.
- (void)showToolsMenuPopup;
// Shows the popup for the tab grid button.
- (void)showTabGridButtonPopup;
// Shows the popup for the tab grid button in the tab strip.
- (void)showTabStripTabGridButtonPopup;
// Shows the popup for the search button.
- (void)showSearchButtonPopup;
// Dismisses the currently presented popup.
- (void)dismissPopupMenuAnimated:(BOOL)animated;

@end

#endif  // IOS_CHROME_BROWSER_UI_COMMANDS_POPUP_MENU_COMMANDS_H_
