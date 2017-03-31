// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_TAB_STRIP_TAB_STRIP_VIEW_CONTROLLER_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_TAB_STRIP_TAB_STRIP_VIEW_CONTROLLER_H_

#import "ios/clean/chrome/browser/ui/tab_collection/tab_collection_view_controller.h"

@protocol TabStripCommands;

// Controller for a scrolling view displaying square cells that represent
// the user's open tabs.
@interface TabStripViewController : TabCollectionViewController
// Dispatcher to handle commands to open/close tabs.
@property(nonatomic, weak) id<TabStripCommands> dispatcher;
@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_TAB_STRIP_TAB_STRIP_VIEW_CONTROLLER_H_
