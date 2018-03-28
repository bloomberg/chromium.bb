// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/popup_menu/popup_menu_table_view_controller.h"

#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/popup_menu/cells/popup_menu_footer_item.h"
#import "ios/chrome/browser/ui/popup_menu/cells/popup_menu_item.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_styler.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::UserMetricsAction;

namespace {
const CGFloat kFooterHeight = 21;
const CGFloat kPopupMenuVerticalInsets = 7;
const CGFloat kScrollIndicatorVerticalInsets = 11;
}  // namespace

@implementation PopupMenuTableViewController

@dynamic tableViewModel;
@synthesize baseViewController = _baseViewController;
@synthesize dispatcher = _dispatcher;

#pragma mark - UIViewController

- (void)viewDidLoad {
  self.styler.tableViewBackgroundColor = nil;
  [super viewDidLoad];
  self.tableView.contentInset = UIEdgeInsetsMake(kPopupMenuVerticalInsets, 0,
                                                 kPopupMenuVerticalInsets, 0);
  self.tableView.scrollIndicatorInsets = UIEdgeInsetsMake(
      kScrollIndicatorVerticalInsets, 0, kScrollIndicatorVerticalInsets, 0);
  self.tableView.rowHeight = UITableViewAutomaticDimension;
  self.tableView.sectionHeaderHeight = 0;
  self.tableView.separatorStyle = UITableViewCellSeparatorStyleNone;
  // Adding a tableHeaderView is needed to prevent a wide inset on top of the
  // collection.
  self.tableView.tableHeaderView = [[UIView alloc]
      initWithFrame:CGRectMake(0.0f, 0.0f, self.tableView.bounds.size.width,
                               0.01f)];
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

    if (section != items.count - 1) {
      // Add a footer for all sections except the last one.
      TableViewHeaderFooterItem* footer =
          [[PopupMenuFooterItem alloc] initWithType:kItemTypeEnumZero];
      [self.tableViewModel setFooter:footer
            forSectionWithIdentifier:sectionIdentifier];
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

- (CGFloat)tableView:(UITableView*)tableView
    heightForFooterInSection:(NSInteger)section {
  if (section == self.tableViewModel.numberOfSections - 1)
    return 0;
  return kFooterHeight;
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
      [self showNotImplementedAlert];
      break;
    case PopupMenuActionFindInPage:
      base::RecordAction(UserMetricsAction("MobileMenuFindInPage"));
      [self.dispatcher showFindInPage];
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
      [self showNotImplementedAlert];
      break;
    case PopupMenuActionReportIssue:
      base::RecordAction(UserMetricsAction("MobileMenuReportAnIssue"));
      [self.dispatcher
          showReportAnIssueFromViewController:self.baseViewController];
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
      [self.dispatcher showSettingsFromViewController:self.baseViewController];
      break;
  }

  // Close the tools menu.
  [self.dispatcher dismissPopupMenu];
}

#pragma mark - Private

// TODO(crbug.com/822703): Remove this.
- (void)showNotImplementedAlert {
  UIAlertController* alertController =
      [UIAlertController alertControllerWithTitle:@"Not implemented yet."
                                          message:nil
                                   preferredStyle:UIAlertControllerStyleAlert];
  UIAlertAction* alertAction =
      [UIAlertAction actionWithTitle:@"OK"
                               style:UIAlertActionStyleCancel
                             handler:nil];

  [alertController addAction:alertAction];
  [self.baseViewController presentViewController:alertController
                                        animated:YES
                                      completion:nil];
}

@end
