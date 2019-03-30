// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/modals/infobar_password_table_view_controller.h"

#include "base/logging.h"
#include "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/infobars/modals/infobar_modal_constants.h"
#import "ios/chrome/browser/ui/infobars/modals/infobar_modal_delegate.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_cells_constants.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_detail_icon_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_button_item.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_styler.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierContent = kSectionIdentifierEnumZero,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeURL = kItemTypeEnumZero,
  ItemTypeUsername,
  ItemTypePassword,
  ItemTypeSavePassword,
};

@implementation InfobarPasswordTableViewController

#pragma mark - ViewController Lifecycle

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.backgroundColor = [UIColor whiteColor];
  self.styler.cellBackgroundColor = [UIColor whiteColor];
  self.tableView.scrollEnabled = NO;
  self.tableView.sectionHeaderHeight = 0;
  [self.tableView
      setSeparatorInset:UIEdgeInsetsMake(0, kTableViewHorizontalSpacing, 0, 0)];

  // Configure the NavigationBar.
  UIBarButtonItem* cancelButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemCancel
                           target:self
                           action:@selector(dismissInfobarModal:)];
  cancelButton.accessibilityIdentifier = kInfobarModalCancelButton;
  UIImage* settingsImage = [[UIImage imageNamed:@"infobar_settings_icon"]
      imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
  UIBarButtonItem* settingsButton =
      [[UIBarButtonItem alloc] initWithImage:settingsImage
                                       style:UIBarButtonItemStylePlain
                                      target:self
                                      action:nil];
  self.navigationItem.leftBarButtonItem = cancelButton;
  self.navigationItem.rightBarButtonItem = settingsButton;
  self.navigationController.navigationBar.prefersLargeTitles = NO;

  [self loadModel];
}

#pragma mark - TableViewModel

- (void)loadModel {
  [super loadModel];

  TableViewModel* model = self.tableViewModel;
  [model addSectionWithIdentifier:SectionIdentifierContent];

  TableViewDetailIconItem* URLDetailItem =
      [[TableViewDetailIconItem alloc] initWithType:ItemTypeURL];
  URLDetailItem.text = l10n_util::GetNSString(IDS_IOS_SHOW_PASSWORD_VIEW_SITE);
  URLDetailItem.detailText = self.URL;
  [model addItem:URLDetailItem
      toSectionWithIdentifier:SectionIdentifierContent];

  TableViewDetailIconItem* usernameDetailItem =
      [[TableViewDetailIconItem alloc] initWithType:ItemTypeUsername];
  usernameDetailItem.text =
      l10n_util::GetNSString(IDS_IOS_SHOW_PASSWORD_VIEW_USERNAME);
  usernameDetailItem.detailText = self.username;
  [model addItem:usernameDetailItem
      toSectionWithIdentifier:SectionIdentifierContent];

  TableViewDetailIconItem* passwordDetailItem =
      [[TableViewDetailIconItem alloc] initWithType:ItemTypePassword];
  passwordDetailItem.text =
      l10n_util::GetNSString(IDS_IOS_SHOW_PASSWORD_VIEW_PASSWORD);
  // TODO(crbug.com/927064): Set the number of dots depending on Password
  // length?
  passwordDetailItem.detailText = @"•••••••••";
  [model addItem:passwordDetailItem
      toSectionWithIdentifier:SectionIdentifierContent];

  TableViewTextButtonItem* savePasswordButtonItem =
      [[TableViewTextButtonItem alloc] initWithType:ItemTypeSavePassword];
  // TODO(crbug.com/927064): Create IDS String for this once we're sure about
  // the exact text.
  savePasswordButtonItem.buttonText = @"Save Password";
  [model addItem:savePasswordButtonItem
      toSectionWithIdentifier:SectionIdentifierContent];
}

#pragma mark - UITableViewDataSource

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];
  NSInteger itemTypeSelected =
      [self.tableViewModel itemTypeForIndexPath:indexPath];

  if (itemTypeSelected == ItemTypeSavePassword) {
    TableViewTextButtonCell* tableViewTextButtonCell =
        base::mac::ObjCCastStrict<TableViewTextButtonCell>(cell);
    [tableViewTextButtonCell.button
               addTarget:self.infobarModalDelegate
                  action:@selector(modalInfobarButtonWasPressed:)
        forControlEvents:UIControlEventTouchUpInside];
  }

  return cell;
}

#pragma mark - Private Methods

- (void)dismissInfobarModal:(UIButton*)sender {
  [self.infobarModalDelegate dismissInfobarModal:sender completion:nil];
}

@end
