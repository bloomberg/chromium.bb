// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_HISTORY_HISTORY_TABLE_CONTAINER_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_HISTORY_HISTORY_TABLE_CONTAINER_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/history/history_table_view_controller_delegate.h"
#import "ios/chrome/browser/ui/table_view/table_container_view_controller.h"

@protocol ApplicationCommands;
@protocol HistoryTableUpdaterDelegate;

// Container for handling the interaction between its TableViewController, the
// container BottomToolbar and the SearchController.
@interface HistoryTableContainerViewController
    : TableContainerViewController<HistoryTableViewControllerDelegate>

- (instancetype)initWithTable:
    (ChromeTableViewController<HistoryTableUpdaterDelegate>*)table;
// The dispatcher used by this ViewController.
@property(nonatomic, weak) id<ApplicationCommands> dispatcher;

@end

#endif  // IOS_CHROME_BROWSER_UI_HISTORY_HISTORY_TABLE_CONTAINER_VIEW_CONTROLLER_H_
