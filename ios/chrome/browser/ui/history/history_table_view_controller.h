// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_HISTORY_HISTORY_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_HISTORY_HISTORY_TABLE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/table_view/chrome_table_view_controller.h"

#include "ios/chrome/browser/ui/history/history_consumer.h"

namespace ios {
class ChromeBrowserState;
}

@protocol HistoryLocalCommands;
@protocol UrlLoader;

// ChromeTableViewController for displaying history items.
@interface HistoryTableViewController
    : ChromeTableViewController<HistoryConsumer>
// The ViewController's BrowserState.
@property(nonatomic, assign) ios::ChromeBrowserState* browserState;
// Abstraction to communicate with HistoryService and WebHistoryService.
// Not owned by HistoryTableViewController.
@property(nonatomic, assign) history::BrowsingHistoryService* historyService;
// The UrlLoader used by this ViewController.
@property(nonatomic, weak) id<UrlLoader> loader;
// Delegate for this HistoryTableView.
@property(nonatomic, weak) id<HistoryLocalCommands> localDispatcher;

// Initializers.
- (instancetype)init NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithTableViewStyle:(UITableViewStyle)style
                           appBarStyle:
                               (ChromeTableViewControllerStyle)appBarStyle
    NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_HISTORY_HISTORY_TABLE_VIEW_CONTROLLER_H_
