// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/modals/infobar_password_table_view_controller.h"

#include "base/logging.h"
#include "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/infobars/modals/infobar_modal_constants.h"
#import "ios/chrome/browser/ui/infobars/modals/infobar_password_modal_delegate.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_cells_constants.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_button_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_edit_item.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_styler.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Text color for the Cancel button.
const CGFloat kCancelButtonTextColorBlue = 0x1A73E8;
}  // namespace

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierContent = kSectionIdentifierEnumZero,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeURL = kItemTypeEnumZero,
  ItemTypeUsername,
  ItemTypePassword,
  ItemTypeSaveCredentials,
  ItemTypeCancel,
};

@interface InfobarPasswordTableViewController ()
// Item that holds the Username TextField information.
@property(nonatomic, strong) TableViewTextEditItem* usernameItem;
// Item that holds the Password TextField information.
@property(nonatomic, strong) TableViewTextEditItem* passwordItem;
// Item that holds the SaveCredentials Button information.
@property(nonatomic, strong) TableViewTextButtonItem* saveCredentialsItem;
// Item that holds the cancel Button for this Infobar. e.g. "Never Save for this
// site".
@property(nonatomic, strong) TableViewTextButtonItem* cancelInfobarItem;
@end

@implementation InfobarPasswordTableViewController

#pragma mark - ViewController Lifecycle

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.backgroundColor = [UIColor whiteColor];
  self.styler.cellBackgroundColor = [UIColor whiteColor];
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
  UIBarButtonItem* settingsButton = [[UIBarButtonItem alloc]
      initWithImage:settingsImage
              style:UIBarButtonItemStylePlain
             target:self.infobarModalDelegate
             action:@selector(presentPasswordSettings)];
  self.navigationItem.leftBarButtonItem = cancelButton;
  self.navigationItem.rightBarButtonItem = settingsButton;
  self.navigationController.navigationBar.prefersLargeTitles = NO;

  [self loadModel];
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];
  self.tableView.scrollEnabled =
      self.tableView.contentSize.height > self.view.frame.size.height;
}

#pragma mark - TableViewModel

- (void)loadModel {
  [super loadModel];

  TableViewModel* model = self.tableViewModel;
  [model addSectionWithIdentifier:SectionIdentifierContent];

  TableViewTextEditItem* URLItem =
      [[TableViewTextEditItem alloc] initWithType:ItemTypeURL];
  URLItem.textFieldName =
      l10n_util::GetNSString(IDS_IOS_SHOW_PASSWORD_VIEW_SITE);
  URLItem.textFieldValue = self.URL;
  [model addItem:URLItem toSectionWithIdentifier:SectionIdentifierContent];

  self.usernameItem =
      [[TableViewTextEditItem alloc] initWithType:ItemTypeUsername];
  self.usernameItem.textFieldName =
      l10n_util::GetNSString(IDS_IOS_SHOW_PASSWORD_VIEW_USERNAME);
  self.usernameItem.textFieldValue = self.username;
  self.usernameItem.textFieldEnabled = YES;
  self.usernameItem.autoCapitalizationType = UITextAutocapitalizationTypeNone;
  [model addItem:self.usernameItem
      toSectionWithIdentifier:SectionIdentifierContent];

  self.passwordItem =
      [[TableViewTextEditItem alloc] initWithType:ItemTypePassword];
  self.passwordItem.textFieldName =
      l10n_util::GetNSString(IDS_IOS_SHOW_PASSWORD_VIEW_PASSWORD);
  self.passwordItem.textFieldValue = self.maskedPassword;
  [model addItem:self.passwordItem
      toSectionWithIdentifier:SectionIdentifierContent];

  self.saveCredentialsItem =
      [[TableViewTextButtonItem alloc] initWithType:ItemTypeSaveCredentials];
  self.saveCredentialsItem.textAlignment = NSTextAlignmentNatural;
  self.saveCredentialsItem.text = self.detailsTextMessage;
  self.saveCredentialsItem.buttonText = self.saveButtonText;
  self.saveCredentialsItem.disableButtonIntrinsicWidth = YES;
  [model addItem:self.saveCredentialsItem
      toSectionWithIdentifier:SectionIdentifierContent];

  if ([self.cancelButtonText length]) {
    self.cancelInfobarItem =
        [[TableViewTextButtonItem alloc] initWithType:ItemTypeCancel];
    self.cancelInfobarItem.buttonText = self.cancelButtonText;
    self.cancelInfobarItem.buttonTextColor =
        UIColorFromRGB(kCancelButtonTextColorBlue);
    self.cancelInfobarItem.buttonBackgroundColor = [UIColor clearColor];
    [model addItem:self.cancelInfobarItem
        toSectionWithIdentifier:SectionIdentifierContent];
  }
}

#pragma mark - UITableViewDataSource

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];
  ItemType itemType = static_cast<ItemType>(
      [self.tableViewModel itemTypeForIndexPath:indexPath]);

  switch (itemType) {
    case ItemTypeSaveCredentials: {
      TableViewTextButtonCell* tableViewTextButtonCell =
          base::mac::ObjCCastStrict<TableViewTextButtonCell>(cell);
      [tableViewTextButtonCell.button
                 addTarget:self
                    action:@selector(saveCredentialsButtonWasPressed:)
          forControlEvents:UIControlEventTouchUpInside];
      break;
    }
    case ItemTypeCancel: {
      TableViewTextButtonCell* tableViewTextButtonCell =
          base::mac::ObjCCastStrict<TableViewTextButtonCell>(cell);
      [tableViewTextButtonCell.button
                 addTarget:self.infobarModalDelegate
                    action:@selector(neverSaveCredentialsForCurrentSite)
          forControlEvents:UIControlEventTouchUpInside];
      break;
    }
    case ItemTypeUsername:
    case ItemTypePassword: {
      TableViewTextEditCell* editCell =
          base::mac::ObjCCast<TableViewTextEditCell>(cell);
      [editCell.textField addTarget:self
                             action:@selector(updateSaveCredentialsButtonState)
                   forControlEvents:UIControlEventEditingChanged];
      break;
    }
    case ItemTypeURL:
      break;
  }

  return cell;
}

#pragma mark - Private Methods

- (void)updateSaveCredentialsButtonState {
  BOOL currentButtonState = [self.saveCredentialsItem isEnabled];
  BOOL newButtonState = [self.passwordItem.textFieldValue length] ? YES : NO;
  if (currentButtonState != newButtonState) {
    self.saveCredentialsItem.enabled = newButtonState;
    [self reconfigureCellsForItems:@[ self.saveCredentialsItem ]];
  }
}

- (void)dismissInfobarModal:(UIButton*)sender {
  [self.infobarModalDelegate dismissInfobarModal:sender
                                        animated:YES
                                      completion:nil];
}

- (void)saveCredentialsButtonWasPressed:(UIButton*)sender {
  [self.infobarModalDelegate
      updateCredentialsWithUsername:self.usernameItem.textFieldValue
                           password:self.unmaskedPassword];
}

@end
