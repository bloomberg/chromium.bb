// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/history/history_table_container_view_controller.h"

#include "base/mac/foundation_util.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#include "ios/chrome/browser/ui/history/history_table_view_controller.h"
#import "ios/chrome/browser/ui/table_view/table_container_bottom_toolbar.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface HistoryTableContainerViewController ()
// This ViewController's searchController;
@property(nonatomic, strong) UISearchController* searchController;
@end

@implementation HistoryTableContainerViewController
@synthesize dispatcher = _dispatcher;
@synthesize searchController = _searchController;

- (instancetype)initWithTable:
    (ChromeTableViewController<HistoryTableUpdaterDelegate>*)table {
  self = [super initWithTable:table];
  if (self) {
    // Setup the bottom toolbar.
    NSString* leadingButtonString = l10n_util::GetNSStringWithFixup(
        IDS_HISTORY_OPEN_CLEAR_BROWSING_DATA_DIALOG);
    NSString* trailingButtonString =
        l10n_util::GetNSString(IDS_HISTORY_START_EDITING_BUTTON);
    TableContainerBottomToolbar* toolbar = [[TableContainerBottomToolbar alloc]
        initWithLeadingButtonText:leadingButtonString
               trailingButtonText:trailingButtonString];
    [toolbar.leadingButton setTitleColor:[UIColor redColor]
                                forState:UIControlStateNormal];
    self.bottomToolbar = toolbar;

    // Setup toolbar buttons.
    [self.bottomToolbar.leadingButton addTarget:self
                                         action:@selector(openPrivacySettings)
                               forControlEvents:UIControlEventTouchUpInside];
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  [self addSearchController];
}

// Creates and adds a UISearchController.
- (void)addSearchController {
  // Init the searchController with nil so the results are displayed on the same
  // TableView.
  self.searchController =
      [[UISearchController alloc] initWithSearchResultsController:nil];
  self.searchController.dimsBackgroundDuringPresentation = NO;
  // For iOS 11 and later, we place the search bar in the navigation bar, if not
  // we place the search bar in the table view's header.
  if (@available(iOS 11, *)) {
    self.navigationItem.searchController = self.searchController;
    // We want the search bar visible all the time.
    self.navigationItem.hidesSearchBarWhenScrolling = NO;
  } else {
    self.tableViewController.tableView.tableHeaderView =
        self.searchController.searchBar;
  }
}

#pragma mark - Private Methods

- (void)openPrivacySettings {
  // Ignore the button tap if |self| is presenting another ViewController.
  // TODO(crbug.com/831865): Find a way to disable the button when a VC is
  // presented.
  if ([self presentedViewController]) {
    return;
  }
  base::RecordAction(
      base::UserMetricsAction("HistoryPage_InitClearBrowsingData"));
  [self.dispatcher showClearBrowsingDataSettingsFromViewController:
                       self.navigationController];
}

@end
