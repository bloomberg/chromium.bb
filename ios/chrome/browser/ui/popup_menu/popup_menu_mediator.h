// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_POPUP_MENU_POPUP_MENU_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_POPUP_MENU_POPUP_MENU_MEDIATOR_H_

#import <UIKit/UIKit.h>

@class PopupMenuTableViewController;

// Type of popup menus.
typedef NS_ENUM(NSInteger, PopupMenuType) {
  PopupMenuTypeToolsMenu,
  PopupMenuTypeNavigationBackward,
  PopupMenuTypeNavigationForward,
  PopupMenuTypeTabGrid,
  PopupMenuTypeSearch,
};

// Mediator for the popup menu. This object is in charge of creating and
// updating the items of the popup menu.
@interface PopupMenuMediator : NSObject

- (instancetype)initWithType:(PopupMenuType)type NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Sets this mediator up.
- (void)setUp;

// Configures the items of |popupMenu|.
- (void)configurePopupMenu:(PopupMenuTableViewController*)popupMenu;

@end

#endif  // IOS_CHROME_BROWSER_UI_POPUP_MENU_POPUP_MENU_MEDIATOR_H_
