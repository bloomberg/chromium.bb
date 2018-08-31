// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/manual_fill/password_view_controller.h"

#include "base/ios/ios_util.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/action_cell.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_password_cell.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_styler.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  CredentialsSectionIdentifier = kSectionIdentifierEnumZero,
  ActionsSectionIdentifier,
};

}  // namespace
@implementation PasswordViewController

- (instancetype)init {
  return [super initWithTableViewStyle:UITableViewStylePlain
                           appBarStyle:ChromeTableViewControllerStyleNoAppBar];
}

- (void)viewDidLoad {
  self.styler.tableViewBackgroundColor = [UIColor whiteColor];

  [super viewDidLoad];

  self.tableView.separatorStyle = UITableViewCellSeparatorStyleNone;
  self.tableView.sectionHeaderHeight = 0;
  self.tableView.sectionFooterHeight = 0;
  self.tableView.estimatedRowHeight = 200;
  self.tableView.separatorInset = UIEdgeInsetsMake(0, 0, 0, 0);

  NSString* titleString =
      l10n_util::GetNSString(IDS_IOS_MANUAL_FALLBACK_USE_OTHER_PASSWORD);
  self.title = titleString;
  self.navigationItem.rightBarButtonItem = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                           target:self
                           action:@selector(handleDoneNavigationItemTap)];

  if (!base::ios::IsRunningOnIOS11OrLater()) {
    // On iOS 11 this is not needed since the cell constrains are updated by the
    // system.
    [[NSNotificationCenter defaultCenter]
        addObserver:self.tableView
           selector:@selector(reloadData)
               name:UIContentSizeCategoryDidChangeNotification
             object:nil];
  }
}

#pragma mark - ManualFillPasswordConsumer

- (void)presentCredentials:(NSArray<ManualFillCredentialItem*>*)credentials {
  [self presentItems:credentials inSection:CredentialsSectionIdentifier];
}

- (void)presentActions:(NSArray<ManualFillActionItem*>*)actions {
  [self presentItems:actions inSection:ActionsSectionIdentifier];
}

#pragma mark - Private

// Callback for the "Done" navigation item.
- (void)handleDoneNavigationItemTap {
  [self dismissViewControllerAnimated:YES completion:nil];
}

// Presents |items| in the respective section. Handles creating or deleting the
// section accordingly.
- (void)presentItems:(NSArray<TableViewItem*>*)items
           inSection:(SectionIdentifier)sectionIdentifier {
  if (!self.tableViewModel) {
    [self loadModel];
  }
  BOOL doesSectionExist =
      [self.tableViewModel hasSectionForSectionIdentifier:sectionIdentifier];
  // If there are no passed credentials, remove section if exist and return.
  if (!items.count) {
    if (doesSectionExist) {
      NSInteger tableViewSection =
          [self.tableViewModel sectionForSectionIdentifier:sectionIdentifier];
      NSIndexSet* sectionIndexSet =
          [NSIndexSet indexSetWithIndex:tableViewSection];
      [self.tableViewModel removeSectionWithIdentifier:sectionIdentifier];
      [self.tableView deleteSections:sectionIndexSet
                    withRowAnimation:UITableViewRowAnimationNone];
    }
    return;
  }

  if (!doesSectionExist) {
    if (sectionIdentifier == CredentialsSectionIdentifier) {
      [self.tableViewModel
          insertSectionWithIdentifier:CredentialsSectionIdentifier
                              atIndex:0];
    } else {
      [self.tableViewModel addSectionWithIdentifier:sectionIdentifier];
    }
  }

  for (TableViewItem* item in items) {
    [self.tableViewModel addItem:item
         toSectionWithIdentifier:sectionIdentifier];
  }
  NSInteger tableViewSection =
      [self.tableViewModel sectionForSectionIdentifier:sectionIdentifier];
  NSIndexSet* sectionIndexSet = [NSIndexSet indexSetWithIndex:tableViewSection];
  if (doesSectionExist) {
    [self.tableView reloadSections:sectionIndexSet
                  withRowAnimation:UITableViewRowAnimationNone];
  } else {
    [self.tableView insertSections:sectionIndexSet
                  withRowAnimation:UITableViewRowAnimationNone];
  }
}

@end
