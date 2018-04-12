// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_HISTORY_HISTORY_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_HISTORY_HISTORY_TABLE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/table_view/chrome_table_view_controller.h"

#include "base/ios/block_types.h"
#include "ios/chrome/browser/ui/history/history_consumer.h"
#include "ios/chrome/browser/ui/history/history_table_updater_delegate.h"

namespace ios {
class ChromeBrowserState;
}

@class HistoryTableViewController;
@protocol UrlLoader;

// Delegate for HistoryTableViewController.
@protocol HistoryTableViewControllerDelegate<NSObject>
// Notifies the delegate that history should be dismissed.
- (void)historyTableViewController:(HistoryTableViewController*)controller
         shouldCloseWithCompletion:(ProceduralBlock)completionHandler;
// Notifies the delegate that the tableView has scrolled to |offset|.
- (void)historyTableViewController:(HistoryTableViewController*)controller
                 didScrollToOffset:(CGPoint)offset;
// Notifies the delegate that history entries have been loaded or changed.
- (void)historyTableViewControllerDidChangeEntries:
    (HistoryTableViewController*)controller;
// Notifies the delegate that history entries have been selected or deselected.
- (void)historyTableViewControllerDidChangeEntrySelection:
    (HistoryTableViewController*)controller;
@end

// ChromeTableViewController for displaying history items.
@interface HistoryTableViewController
    : ChromeTableViewController<HistoryConsumer, HistoryTableUpdaterDelegate>
// The ViewController's BrowserState.
@property(nonatomic, assign) ios::ChromeBrowserState* browserState;
// Abstraction to communicate with HistoryService and WebHistoryService.
// Not owned by HistoryTableViewController.
@property(nonatomic, assign) history::BrowsingHistoryService* historyService;
// The UrlLoader used by this ViewController.
@property(nonatomic, weak) id<UrlLoader> loader;
// Delegate for this HistoryTableView.
@property(nonatomic, weak, readonly) id<HistoryTableViewControllerDelegate>
    delegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_HISTORY_HISTORY_TABLE_VIEW_CONTROLLER_H_
