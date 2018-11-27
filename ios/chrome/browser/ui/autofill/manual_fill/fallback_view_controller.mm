// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/manual_fill/fallback_view_controller.h"

#include "base/ios/ios_util.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/action_cell.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_styler.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  ItemsSectionIdentifier = kSectionIdentifierEnumZero,
  ActionsSectionIdentifier,
};

namespace {

// This is the width used for |self.preferredContentSize|.
constexpr float PopoverPreferredWidth = 320;

// This is the maximum height used for |self.preferredContentSize|.
constexpr float PopoverMaxHeight = 250;

}  // namespace

@implementation FallbackViewController

- (instancetype)init {
  self = [super initWithTableViewStyle:UITableViewStylePlain
                           appBarStyle:ChromeTableViewControllerStyleNoAppBar];
  if (self) {
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(handleKeyboardWillShow:)
               name:UIKeyboardWillShowNotification
             object:nil];
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(handleKeyboardDidHide:)
               name:UIKeyboardDidHideNotification
             object:nil];
  }
  return self;
}

- (void)viewDidLoad {
  // Super's |viewDidLoad| uses |styler.tableViewBackgroundColor| so it needs to
  // be set before.
  self.styler.tableViewBackgroundColor = [UIColor whiteColor];

  [super viewDidLoad];

  self.tableView.separatorStyle = UITableViewCellSeparatorStyleNone;
  self.tableView.sectionHeaderHeight = 0;
  self.tableView.sectionFooterHeight = 20.0;
  self.tableView.estimatedRowHeight = 200;
  self.tableView.separatorInset = UIEdgeInsetsMake(0, 0, 0, 0);
  self.tableView.allowsSelection = NO;
  self.definesPresentationContext = YES;
}

- (void)presentDataItems:(NSArray<TableViewItem*>*)items {
  if (!self.tableViewModel) {
    [self loadModel];
  }
  BOOL sectionExist = [self.tableViewModel
      hasSectionForSectionIdentifier:ItemsSectionIdentifier];
  // If there are no passed items, remove section if exist.
  if (!items.count && sectionExist) {
    [self.tableViewModel removeSectionWithIdentifier:ItemsSectionIdentifier];
  } else if (items.count && !sectionExist) {
    [self.tableViewModel insertSectionWithIdentifier:ItemsSectionIdentifier
                                             atIndex:0];
  }
  [self presentFallbackItems:items inSection:ItemsSectionIdentifier];
}

- (void)presentActionItems:(NSArray<TableViewItem*>*)actions {
  if (!self.tableViewModel) {
    [self loadModel];
  }
  BOOL sectionExist = [self.tableViewModel
      hasSectionForSectionIdentifier:ActionsSectionIdentifier];
  // If there are no passed items, remove section if exist.
  if (!actions.count && sectionExist) {
    [self.tableViewModel removeSectionWithIdentifier:ActionsSectionIdentifier];
  } else if (actions.count && !sectionExist) {
    [self.tableViewModel addSectionWithIdentifier:ActionsSectionIdentifier];
  }
  [self presentFallbackItems:actions inSection:ActionsSectionIdentifier];
}

#pragma mark - Private

- (void)handleKeyboardDidHide:(NSNotification*)notification {
  if (self.contentInsetsAlwaysEqualToSafeArea && !IsIPadIdiom()) {
    // Resets the table view content inssets to be equal to the safe area
    // insets.
    self.tableView.contentInset = self.view.safeAreaInsets;
  }
}

- (void)handleKeyboardWillShow:(NSNotification*)notification {
  if (self.contentInsetsAlwaysEqualToSafeArea && !IsIPadIdiom()) {
    // Sets the bottom inset to be equal to the height of the keyboard to
    // override the behaviour in UITableViewController. Which adjust the scroll
    // view insets to accommodate for the keyboard.
    CGRect keyboardFrame =
        [notification.userInfo[UIKeyboardFrameEndUserInfoKey] CGRectValue];
    CGFloat keyboardHeight = keyboardFrame.size.height;
    UIEdgeInsets safeInsets = self.view.safeAreaInsets;
    self.tableView.contentInset =
        UIEdgeInsetsMake(safeInsets.top, safeInsets.left,
                         safeInsets.bottom - keyboardHeight, safeInsets.right);
  }
}

// Presents |items| in the respective section. Handles creating or deleting the
// section accordingly.
- (void)presentFallbackItems:(NSArray<TableViewItem*>*)items
                   inSection:(SectionIdentifier)sectionIdentifier {
  // If there are no passed items, remove section if exist.
  if (items.count) {
    [self.tableViewModel
        deleteAllItemsFromSectionWithIdentifier:sectionIdentifier];
    for (TableViewItem* item in items) {
      [self.tableViewModel addItem:item
           toSectionWithIdentifier:sectionIdentifier];
    }
  }
  [self.tableView reloadData];
  if (IsIPadIdiom()) {
    // Update the preffered content size on iPad so the popover shows the right
    // size.
    [self.tableView layoutIfNeeded];
    CGSize systemLayoutSize = self.tableView.contentSize;
    CGFloat preferredHeight = MIN(systemLayoutSize.height, PopoverMaxHeight);
    self.preferredContentSize =
        CGSizeMake(PopoverPreferredWidth, preferredHeight);
  }
}

@end
