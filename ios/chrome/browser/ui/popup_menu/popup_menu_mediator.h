// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_POPUP_MENU_POPUP_MENU_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_POPUP_MENU_POPUP_MENU_MEDIATOR_H_

#import <UIKit/UIKit.h>

@protocol BrowserCommands;
@class PopupMenuTableViewController;
class ReadingListModel;
class WebStateList;

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

- (instancetype)initWithType:(PopupMenuType)type
                 isIncognito:(BOOL)isIncognito
            readingListModel:(ReadingListModel*)readingListModel
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// The WebStateList that this mediator listens for any changes on the current
// WebState.
@property(nonatomic, assign) WebStateList* webStateList;
// The TableView to be configured with this mediator.
@property(nonatomic, strong) PopupMenuTableViewController* popupMenu;
// Dispatcher.
@property(nonatomic, weak) id<BrowserCommands> dispatcher;

// Disconnect the mediator.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_UI_POPUP_MENU_POPUP_MENU_MEDIATOR_H_
