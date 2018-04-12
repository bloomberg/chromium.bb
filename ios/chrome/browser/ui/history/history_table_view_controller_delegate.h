// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_HISTORY_HISTORY_TABLE_VIEW_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_HISTORY_HISTORY_TABLE_VIEW_CONTROLLER_DELEGATE_H_

#include "base/ios/block_types.h"

// Delegate for handling HistoryTableViewController actions.
@protocol HistoryTableViewControllerDelegate
// Notifies the delegate that history should be dismissed.
- (void)dismissHistoryWithCompletion:(ProceduralBlock)completionHandler;
// Notifies the delegate that history entries have been loaded or changed.
- (void)historyTableViewControllerDidChangeEntries;
// Notifies the delegate that history entries have been selected or deselected.
- (void)historyTableViewControllerDidChangeEntrySelection;
@end

#endif  // IOS_CHROME_BROWSER_UI_HISTORY_HISTORY_TABLE_VIEW_CONTROLLER_DELEGATE_H_
