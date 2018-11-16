// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/settings_root_table_view_controller.h"

#import "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/material_components/utils.h"
#import "ios/chrome/browser/ui/settings/cells/settings_cells_constants.h"
#import "ios/chrome/browser/ui/settings/settings_navigation_controller.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_styler.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface SettingsRootTableViewController ()

// Delete button for the toolbar.
@property(nonatomic, strong) UIBarButtonItem* deleteButton;

@end

@implementation SettingsRootTableViewController

@synthesize deleteButton = _deleteButton;
@synthesize dispatcher = _dispatcher;

#pragma mark - Public

- (void)updateEditButton {
  if (self.tableView.editing) {
    self.navigationItem.rightBarButtonItem = [self createEditModeDoneButton];
    return;
  }

  [self.navigationController setToolbarHidden:YES animated:YES];
  if (self.shouldShowEditButton) {
    self.navigationItem.rightBarButtonItem = [self createEditButton];
  } else {
    self.navigationItem.rightBarButtonItem = [self doneButtonIfNeeded];
  }
}

#pragma mark - Property

- (UIBarButtonItem*)deleteButton {
  if (!_deleteButton) {
    _deleteButton = [[UIBarButtonItem alloc]
        initWithTitle:l10n_util::GetNSString(IDS_IOS_SETTINGS_TOOLBAR_DELETE)
                style:UIBarButtonItemStylePlain
               target:self
               action:@selector(deleteButtonCallback)];
    _deleteButton.tintColor = [UIColor redColor];
  }
  return _deleteButton;
}

#pragma mark - UIViewController

- (NSArray<UIBarButtonItem*>*)toolbarItems {
  UIBarButtonItem* flexibleSpace = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemFlexibleSpace
                           target:nil
                           action:nil];
  return @[ flexibleSpace, self.deleteButton, flexibleSpace ];
}

- (void)viewDidLoad {
  self.styler.tableViewBackgroundColor =
      [UIColor groupTableViewBackgroundColor];
  self.styler.tableViewSectionHeaderBlurEffect = nil;
  [super viewDidLoad];
  self.styler.cellBackgroundColor = [UIColor whiteColor];
  self.styler.cellTitleColor = [UIColor blackColor];
  self.tableView.estimatedRowHeight = kSettingsCellDefaultHeight;
  // Do not set the estimated height of the footer/header as if there is no
  // header/footer, there is an empty space.
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  UIBarButtonItem* doneButton = [self doneButtonIfNeeded];
  if (!self.navigationItem.rightBarButtonItem && doneButton) {
    self.navigationItem.rightBarButtonItem = doneButton;
  }
}

- (void)viewWillDisappear:(BOOL)animated {
  [super viewWillDisappear:animated];
  [self.navigationController setToolbarHidden:YES animated:YES];
}

- (void)setEditing:(BOOL)editing animated:(BOOL)animated {
  [super setEditing:editing animated:animated];
  if (!editing)
    [self.navigationController setToolbarHidden:YES animated:YES];
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  if (!self.tableView.editing)
    return;

  if (self.navigationController.toolbarHidden)
    [self.navigationController setToolbarHidden:NO animated:YES];
}

- (void)tableView:(UITableView*)tableView
    didDeselectRowAtIndexPath:(NSIndexPath*)indexPath {
  if (!self.tableView.editing)
    return;

  if (self.tableView.indexPathsForSelectedRows.count == 0)
    [self.navigationController setToolbarHidden:YES animated:YES];
}

#pragma mark - TableViewLinkHeaderFooterItemDelegate

- (void)view:(TableViewLinkHeaderFooterView*)view didTapLinkURL:(GURL)URL {
  // Subclass must have a valid dispatcher assigned.
  DCHECK(self.dispatcher);
  OpenNewTabCommand* command = [OpenNewTabCommand commandWithURLFromChrome:URL];
  [self.dispatcher closeSettingsUIAndOpenURL:command];
}

#pragma mark - Private

- (void)deleteButtonCallback {
  [self deleteItems:self.tableView.indexPathsForSelectedRows];
}

- (UIBarButtonItem*)doneButtonIfNeeded {
  if (self.shouldHideDoneButton) {
    return nil;
  }
  SettingsNavigationController* navigationController =
      base::mac::ObjCCast<SettingsNavigationController>(
          self.navigationController);
  return [navigationController doneButton];
}

- (UIBarButtonItem*)createEditButton {
  // Create a custom Edit bar button item, as Material Navigation Bar does not
  // handle a system UIBarButtonSystemItemEdit item.
  UIBarButtonItem* button = [[UIBarButtonItem alloc]
      initWithTitle:l10n_util::GetNSString(IDS_IOS_NAVIGATION_BAR_EDIT_BUTTON)
              style:UIBarButtonItemStyleDone
             target:self
             action:@selector(editButtonPressed)];
  [button setEnabled:[self editButtonEnabled]];
  return button;
}

- (UIBarButtonItem*)createEditModeDoneButton {
  // Create a custom Done bar button item, as Material Navigation Bar does not
  // handle a system UIBarButtonSystemItemDone item.
  return [[UIBarButtonItem alloc]
      initWithTitle:l10n_util::GetNSString(IDS_IOS_NAVIGATION_BAR_DONE_BUTTON)
              style:UIBarButtonItemStyleDone
             target:self
             action:@selector(editButtonPressed)];
}

#pragma mark - Subclassing

- (BOOL)shouldShowEditButton {
  return NO;
}

- (BOOL)editButtonEnabled {
  return NO;
}

- (void)editButtonPressed {
  [self setEditing:!self.tableView.editing animated:YES];
  [self updateEditButton];
}

- (void)deleteItems:(NSArray<NSIndexPath*>*)indexPaths {
  [self.tableView performBatchUpdates:^{
    [self removeFromModelItemAtIndexPaths:indexPaths];
    [self.tableView deleteRowsAtIndexPaths:indexPaths
                          withRowAnimation:UITableViewRowAnimationAutomatic];
  }
                           completion:nil];
}

@end
