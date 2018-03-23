// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/popup_menu/popup_menu_table_view_controller.h"

#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/popup_menu/cells/popup_menu_item.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::UserMetricsAction;

@interface PopupMenuTableViewController ()
@end

@implementation PopupMenuTableViewController

@dynamic tableViewModel;
@synthesize dispatcher = _dispatcher;

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.tableView.rowHeight = UITableViewAutomaticDimension;
  self.tableView.sectionHeaderHeight = 0;
  self.tableView.sectionFooterHeight = 0;
  self.tableView.separatorStyle = UITableViewCellSeparatorStyleNone;
}

- (void)setPopupMenuItems:
    (NSArray<NSArray<TableViewItem<PopupMenuItem>*>*>*)items {
  [super loadModel];
  for (NSUInteger section = 0; section < items.count; section++) {
    NSInteger sectionIdentifier = kSectionIdentifierEnumZero + section;
    [self.tableViewModel addSectionWithIdentifier:sectionIdentifier];
    for (TableViewItem<PopupMenuItem>* item in items[section]) {
      [self.tableViewModel addItem:item
           toSectionWithIdentifier:sectionIdentifier];
    }
  }
  [self.tableView reloadData];
}

- (CGSize)preferredContentSize {
  CGFloat width = 0;
  CGFloat height = 0;
  for (NSInteger section = 0; section < [self.tableViewModel numberOfSections];
       section++) {
    NSInteger sectionIdentifier =
        [self.tableViewModel sectionIdentifierForSection:section];
    for (TableViewItem<PopupMenuItem>* item in
         [self.tableViewModel itemsInSectionWithIdentifier:sectionIdentifier]) {
      CGSize sizeForCell = [item cellSizeForWidth:self.view.bounds.size.width];
      width = MAX(width, ceil(sizeForCell.width));
      height += ceil(sizeForCell.height);
    }
  }
  return CGSizeMake(width, height);
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  TableViewItem<PopupMenuItem>* item =
      [self.tableViewModel itemAtIndexPath:indexPath];
  UIView* cell = [self.tableView cellForRowAtIndexPath:indexPath];
  CGPoint center = [cell convertPoint:cell.center toView:nil];
  [self executeActionForIdentifier:item.actionIdentifier origin:center];
}

#pragma mark - Private

// Executes the action associated with |identifier|, using |origin| as the point
// of origin of the action if one is needed.
- (void)executeActionForIdentifier:(PopupMenuAction)identifier
                            origin:(CGPoint)origin {
  switch (identifier) {
    case PopupMenuActionReload:
      base::RecordAction(UserMetricsAction("MobileMenuReload"));
      [self.dispatcher reload];
      break;
    case PopupMenuActionStop:
      base::RecordAction(UserMetricsAction("MobileMenuStop"));
      [self.dispatcher stopLoading];
      break;
    case PopupMenuActionOpenNewTab:
      base::RecordAction(UserMetricsAction("MobileMenuNewTab"));
      [self.dispatcher
          openNewTab:[[OpenNewTabCommand alloc] initWithIncognito:NO
                                                      originPoint:origin]];
      break;
    case PopupMenuActionOpenNewIncognitoTab:
      base::RecordAction(UserMetricsAction("MobileMenuNewIncognitoTab"));
      [self.dispatcher
          openNewTab:[[OpenNewTabCommand alloc] initWithIncognito:YES
                                                      originPoint:origin]];
      break;
    case PopupMenuActionReadLater:
      base::RecordAction(UserMetricsAction("MobileMenuReadLater"));
      // TODO(crbug.com/822703): Add action.
      break;
    case PopupMenuActionRequestDesktop:
      base::RecordAction(UserMetricsAction("MobileMenuRequestDesktopSite"));
      [self.dispatcher requestDesktopSite];
      break;
    case PopupMenuActionRequestMobile:
      base::RecordAction(UserMetricsAction("MobileMenuRequestMobileSite"));
      [self.dispatcher requestMobileSite];
      break;
    case PopupMenuActionSiteInformation:
      base::RecordAction(UserMetricsAction("MobileMenuSiteInformation"));
      // TODO(crbug.com/822703): Add action.
      break;
    case PopupMenuActionReportIssue:
      base::RecordAction(UserMetricsAction("MobileMenuReportAnIssue"));
      // TODO(crbug.com/822703): Add action.
      break;
    case PopupMenuActionHelp:
      base::RecordAction(UserMetricsAction("MobileMenuHelp"));
      [self.dispatcher showHelpPage];
      break;
    case PopupMenuActionBookmarks:
      base::RecordAction(UserMetricsAction("MobileMenuAllBookmarks"));
      [self.dispatcher showBookmarksManager];
      break;
    case PopupMenuActionReadingList:
      base::RecordAction(UserMetricsAction("MobileMenuReadingList"));
      [self.dispatcher showReadingList];
      break;
    case PopupMenuActionRecentTabs:
      base::RecordAction(UserMetricsAction("MobileMenuRecentTabs"));
      [self.dispatcher showRecentTabs];
      break;
    case PopupMenuActionHistory:
      base::RecordAction(UserMetricsAction("MobileMenuHistory"));
      [self.dispatcher showHistory];
      break;
    case PopupMenuActionSettings:
      base::RecordAction(UserMetricsAction("MobileMenuSettings"));
      // TODO(crbug.com/822703): Add action.
      break;
  }

  // Close the tools menu.
  [self.dispatcher dismissPopupMenu];
}

@end
