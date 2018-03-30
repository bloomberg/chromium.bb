// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_HISTORY_HISTORY_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_HISTORY_HISTORY_TABLE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/table_view/chrome_table_view_controller.h"

namespace ios {
class ChromeBrowserState;
}

@protocol UrlLoader;

// ChromeTableViewController for displaying history items.
@interface HistoryTableViewController : ChromeTableViewController
// The ViewController's BrowserState.
@property(nonatomic, assign) ios::ChromeBrowserState* browserState;
// The UrlLoader used by this ViewController.
@property(nonatomic, weak) id<UrlLoader> loader;
@end

#endif  // IOS_CHROME_BROWSER_UI_HISTORY_HISTORY_TABLE_VIEW_CONTROLLER_H_
