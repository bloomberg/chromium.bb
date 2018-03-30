// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/history/history_coordinator.h"

#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/ui/history/history_table_view_controller.h"
#import "ios/chrome/browser/ui/table_view/table_container_view_controller.h"
#import "ios/chrome/browser/ui/util/form_sheet_navigation_controller.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface HistoryCoordinator ()
// ViewController being managed by this Coordinator.
@property(nonatomic, strong)
    TableContainerViewController* historyContainerViewController;
@end

@implementation HistoryCoordinator
@synthesize dispatcher = _dispatcher;
@synthesize historyContainerViewController = _historyContainerViewController;
@synthesize loader = _loader;

- (void)start {
  // Initialize and configure HistoryTableViewController.
  HistoryTableViewController* historyTableViewController =
      [[HistoryTableViewController alloc] init];
  historyTableViewController.browserState = self.browserState;
  historyTableViewController.loader = self.loader;

  // Initialize and configure HistoryContainerViewController.
  self.historyContainerViewController = [[TableContainerViewController alloc]
      initWithTable:historyTableViewController];
  self.historyContainerViewController.title =
      l10n_util::GetNSString(IDS_HISTORY_TITLE);
  // TODO(crbug.com/805192): Move this configuration code to
  // HistoryContainerVC once its created, we will use a dispatcher then.
  [self.historyContainerViewController.dismissButton setTarget:self];
  [self.historyContainerViewController.dismissButton setAction:@selector(stop)];
  self.historyContainerViewController.navigationItem.rightBarButtonItem =
      self.historyContainerViewController.dismissButton;

  // Present HistoryContainerViewController.
  FormSheetNavigationController* navController =
      [[FormSheetNavigationController alloc]
          initWithRootViewController:self.historyContainerViewController];
  [navController setModalPresentationStyle:UIModalPresentationFormSheet];
  [self.baseViewController presentViewController:navController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [self.historyContainerViewController dismissViewControllerAnimated:YES
                                                          completion:nil];
  self.historyContainerViewController = nil;
}

@end
