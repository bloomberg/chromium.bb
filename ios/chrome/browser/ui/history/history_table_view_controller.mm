// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/history/history_table_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation HistoryTableViewController
@synthesize browserState = _browserState;
@synthesize historyService = _historyService;
@synthesize loader = _loader;

#pragma mark - HistoryConsumer

- (void)
historyQueryWasCompletedWithResults:
    (const std::vector<history::BrowsingHistoryService::HistoryEntry>&)results
                   queryResultsInfo:(const history::BrowsingHistoryService::
                                         QueryResultsInfo&)queryResultsInfo
                continuationClosure:(base::OnceClosure)continuationClosure {
  // TODO(crbug.com/805190): Implement HistoryConsumer.
}

- (void)historyWasDeleted {
  // TODO(crbug.com/805190): Implement HistoryConsumer.
}

- (void)showNoticeAboutOtherFormsOfBrowsingHistory:(BOOL)shouldShowNotice {
  // TODO(crbug.com/805190): Implement HistoryConsumer.
}

@end
