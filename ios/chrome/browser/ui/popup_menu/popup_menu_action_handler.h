// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_POPUP_MENU_POPUP_MENU_ACTION_HANDLER_H_
#define IOS_CHROME_BROWSER_UI_POPUP_MENU_POPUP_MENU_ACTION_HANDLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/popup_menu/public/popup_menu_table_view_controller_delegate.h"

namespace feature_engagement {
class Tracker;
}
@protocol ApplicationCommands;
@protocol BrowserCommands;
@protocol LoadQueryCommands;
@protocol PopupMenuActionHandlerCommands;

// Handles user interactions with the popup menu.
@interface PopupMenuActionHandler
    : NSObject <PopupMenuTableViewControllerDelegate>

// The view controller that presents |popupMenu|.
@property(nonatomic, weak) UIViewController* baseViewController;

// Command handler.
@property(nonatomic, weak) id<PopupMenuActionHandlerCommands> commandHandler;

// Records events for the use menu items with in-product help. This instance
// does not take ownership of tracker. Tracker must not be destroyed during
// lifetime of this instance.
@property(nonatomic, assign) feature_engagement::Tracker* engagementTracker;

// Dispatcher.
@property(nonatomic, weak)
    id<ApplicationCommands, BrowserCommands, LoadQueryCommands>
        dispatcher;

@end

#endif  // IOS_CHROME_BROWSER_UI_POPUP_MENU_POPUP_MENU_ACTION_HANDLER_H_
