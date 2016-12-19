// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLS_MENU_TOOLS_POPUP_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_TOOLS_MENU_TOOLS_POPUP_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/popup_menu/popup_menu_controller.h"

// The a11y ID of the tools menu table view (used by integration tests).
extern NSString* const kToolsMenuTableViewId;

@class ToolsMenuContext;

// The view controller for the tools menu within the top toolbar.
// The menu is composed of two main view: a top view with icons and a bottom
// view with a table view of menu items.
@interface ToolsPopupController : PopupMenuController

@property(nonatomic, assign) BOOL isCurrentPageBookmarked;

// Initializes the popup with the given |context|, a set of information used to
// determine the appearance of the menu and the entries displayed.
- (instancetype)initWithContext:(ToolsMenuContext*)context;

// Called when the current tab loading state changes.
- (void)setIsTabLoading:(BOOL)isTabLoading;

// TODO(stuartmorgan): Should the set of options that are passed in to the
// constructor just have the ability to specify whether commands should be
// enabled or disabled rather than having these individual setters? b/6048639
// Informs tools popup menu whether "Find In Page..." command should be
// enabled.
- (void)setCanShowFindBar:(BOOL)enabled;

// Informs tools popup menu whether "Share..." command should be enabled.
- (void)setCanShowShareMenu:(BOOL)enabled;

// Informs tools popup menu whether the switch to reader mode is possible.
- (void)setCanUseReaderMode:(BOOL)enabled;

// Informs tools popup menu whether "Request Desktop Site" can be enabled.
- (void)setCanUseDesktopUserAgent:(BOOL)value;

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLS_MENU_TOOLS_POPUP_CONTROLLER_H_
