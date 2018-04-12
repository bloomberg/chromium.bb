// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_HISTORY_HISTORY_TABLE_UPDATER_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_HISTORY_HISTORY_TABLE_UPDATER_DELEGATE_H_

// Delegate to communicate updates from History Container to the History
// TableView
@protocol HistoryTableUpdaterDelegate
// Search history for text |query| and display the results. |query| may be nil.
// If query is empty, show all history items.
- (void)showHistoryMatchingQuery:(NSString*)query;
// Deletes selected items from browser history and removes them from the
// tableView.
- (void)deleteSelectedItems;
@end

#endif  // IOS_CHROME_BROWSER_UI_HISTORY_HISTORY_TABLE_UPDATER_DELEGATE_H_
