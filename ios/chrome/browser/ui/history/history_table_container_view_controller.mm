// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/history/history_table_container_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation HistoryTableContainerViewController
@synthesize dispatcher = _dispatcher;

- (instancetype)initWithTable:
    (ChromeTableViewController<HistoryTableUpdaterDelegate>*)table {
  return [super initWithTable:table];
}

#pragma mark - HistoryTableViewControllerDelegate

- (void)dismissHistoryWithCompletion:(ProceduralBlock)completionHandler {
  [self.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:completionHandler];
}

- (void)historyTableViewControllerDidChangeEntries {
  // TODO(crbug.com/805190): Migrate.
}

- (void)historyTableViewControllerDidChangeEntrySelection {
  // TODO(crbug.com/805190): Migrate.
}

@end
