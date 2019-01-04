// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/table_cell_catalog_view_controller.h"

#import "ios/chrome/browser/ui/authentication/cells/account_control_item.h"
#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view_configurator.h"
#import "ios/chrome/browser/ui/authentication/cells/table_view_account_item.h"
#import "ios/chrome/browser/ui/authentication/cells/table_view_signin_promo_item.h"
#import "ios/chrome/browser/ui/autofill/cells/autofill_edit_item.h"
#import "ios/chrome/browser/ui/icons/chrome_icon.h"
#import "ios/chrome/browser/ui/settings/cells/account_sign_in_item.h"
#import "ios/chrome/browser/ui/settings/cells/autofill_data_item.h"
#import "ios/chrome/browser/ui/settings/cells/copied_to_chrome_item.h"
#import "ios/chrome/browser/ui/settings/cells/encryption_item.h"
#import "ios/chrome/browser/ui/settings/cells/settings_detail_item.h"
#import "ios/chrome/browser/ui/settings/cells/settings_image_detail_text_item.h"
#import "ios/chrome/browser/ui/settings/cells/settings_switch_item.h"
#import "ios/chrome/browser/ui/settings/cells/sync_switch_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_detail_text_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_image_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_button_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_url_item.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_styler.h"
#import "ios/chrome/browser/ui/table_view/table_view_model.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierText = kSectionIdentifierEnumZero,
  SectionIdentifierSettings,
  SectionIdentifierAutofill,
  SectionIdentifierAccount,
  SectionIdentifierURL,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeText = kItemTypeEnumZero,
  ItemTypeTextHeader,
  ItemTypeTextFooter,
  ItemTypeTextButton,
  ItemTypeURLNoMetadata,
  ItemTypeTextAccessoryImage,
  ItemTypeTextAccessoryNoImage,
  ItemTypeURLWithTimestamp,
  ItemTypeURLWithSize,
  ItemTypeURLWithSupplementalText,
  ItemTypeURLWithBadgeImage,
  ItemTypeTextSettingsDetail,
  ItemTypeEncryption,
  ItemTypeLinkFooter,
  ItemTypeDetailText,
  ItemTypeAccountSignInItem,
  ItemTypeSettingsSwitch1,
  ItemTypeSettingsSwitch2,
  ItemTypeSyncSwitch,
  ItemTypeSettingsSyncError,
  ItemTypeAutofillEditItem,
  ItemTypeAutofillData,
  ItemTypeAccount,
};
}

@implementation TableCellCatalogViewController

- (instancetype)init {
  if ((self = [super
           initWithTableViewStyle:UITableViewStyleGrouped
                      appBarStyle:ChromeTableViewControllerStyleNoAppBar])) {
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.title = @"Table Cell Catalog";
  self.tableView.rowHeight = UITableViewAutomaticDimension;
  self.tableView.estimatedRowHeight = 56.0;
  self.tableView.estimatedSectionHeaderHeight = 56.0;
  self.tableView.estimatedSectionFooterHeight = 56.0;

  [self loadModel];
}

- (void)loadModel {
  [super loadModel];

  TableViewModel* model = self.tableViewModel;
  [model addSectionWithIdentifier:SectionIdentifierText];
  [model addSectionWithIdentifier:SectionIdentifierSettings];
  [model addSectionWithIdentifier:SectionIdentifierAutofill];
  [model addSectionWithIdentifier:SectionIdentifierAccount];
  [model addSectionWithIdentifier:SectionIdentifierURL];

  // SectionIdentifierText.
  TableViewTextHeaderFooterItem* textHeaderFooterItem =
      [[TableViewTextHeaderFooterItem alloc] initWithType:ItemTypeTextHeader];
  textHeaderFooterItem.text = @"Simple Text Header";
  [model setHeader:textHeaderFooterItem
      forSectionWithIdentifier:SectionIdentifierText];

  TableViewTextItem* textItem =
      [[TableViewTextItem alloc] initWithType:ItemTypeText];
  textItem.text = @"Simple Text Cell";
  textItem.textAlignment = NSTextAlignmentCenter;
  textItem.textColor = [UIColor blackColor];
  [model addItem:textItem toSectionWithIdentifier:SectionIdentifierText];

  textItem = [[TableViewTextItem alloc] initWithType:ItemTypeText];
  textItem.text = @"1234";
  textItem.masked = YES;
  [model addItem:textItem toSectionWithIdentifier:SectionIdentifierText];

  TableViewImageItem* textImageItem =
      [[TableViewImageItem alloc] initWithType:ItemTypeTextAccessoryImage];
  textImageItem.title = @"Image Item with History Image";
  textImageItem.image = [UIImage imageNamed:@"show_history"];
  [model addItem:textImageItem toSectionWithIdentifier:SectionIdentifierText];

  textImageItem =
      [[TableViewImageItem alloc] initWithType:ItemTypeTextAccessoryNoImage];
  textImageItem.title = @"Image Item with No Image";
  [model addItem:textImageItem toSectionWithIdentifier:SectionIdentifierText];

  TableViewTextItem* textItemDefault =
      [[TableViewTextItem alloc] initWithType:ItemTypeText];
  textItemDefault.text = @"Simple Text Cell with Defaults";
  [model addItem:textItemDefault toSectionWithIdentifier:SectionIdentifierText];

  textHeaderFooterItem =
      [[TableViewTextHeaderFooterItem alloc] initWithType:ItemTypeTextFooter];
  textHeaderFooterItem.text = @"Simple Text Footer";
  [model setFooter:textHeaderFooterItem
      forSectionWithIdentifier:SectionIdentifierText];

  TableViewTextButtonItem* textActionButtonItem =
      [[TableViewTextButtonItem alloc] initWithType:ItemTypeTextButton];
  textActionButtonItem.text = @"Hello, you should do something.";
  textActionButtonItem.buttonText = @"Do something";
  [model addItem:textActionButtonItem
      toSectionWithIdentifier:SectionIdentifierText];

  TableViewDetailTextItem* detailTextItem =
      [[TableViewDetailTextItem alloc] initWithType:ItemTypeDetailText];
  detailTextItem.text = @"Item with two labels";
  detailTextItem.detailText =
      @"The second label is optional and is mostly displayed on one line";
  [model addItem:detailTextItem toSectionWithIdentifier:SectionIdentifierText];

  TableViewDetailTextItem* noDetailTextItem =
      [[TableViewDetailTextItem alloc] initWithType:ItemTypeDetailText];
  noDetailTextItem.text = @"Detail item on one line.";
  [model addItem:noDetailTextItem
      toSectionWithIdentifier:SectionIdentifierText];

  // SectionIdentifierSettings.
  TableViewTextHeaderFooterItem* settingsHeader =
      [[TableViewTextHeaderFooterItem alloc] initWithType:ItemTypeTextHeader];
  settingsHeader.text = @"Settings";
  [model setHeader:settingsHeader
      forSectionWithIdentifier:SectionIdentifierSettings];

  SettingsDetailItem* settingsDetailItem =
      [[SettingsDetailItem alloc] initWithType:ItemTypeTextSettingsDetail];
  settingsDetailItem.text = @"Settings cells";
  settingsDetailItem.detailText = @"Short";
  [model addItem:settingsDetailItem
      toSectionWithIdentifier:SectionIdentifierSettings];

  SettingsDetailItem* settingsDetailItemLong =
      [[SettingsDetailItem alloc] initWithType:ItemTypeTextSettingsDetail];
  settingsDetailItemLong.text = @"Very long text eating the other detail label";
  settingsDetailItemLong.detailText = @"A bit less short";
  [model addItem:settingsDetailItemLong
      toSectionWithIdentifier:SectionIdentifierSettings];

  AccountSignInItem* accountSignInItem =
      [[AccountSignInItem alloc] initWithType:ItemTypeAccountSignInItem];
  accountSignInItem.detailText = @"Get cool stuff on all your devices";
  [model addItem:accountSignInItem
      toSectionWithIdentifier:SectionIdentifierSettings];

  SettingsSwitchItem* settingsSwitchItem =
      [[SettingsSwitchItem alloc] initWithType:ItemTypeSettingsSwitch1];
  settingsSwitchItem.text = @"This is a settings switch item";
  [model addItem:settingsSwitchItem
      toSectionWithIdentifier:SectionIdentifierSettings];

  SettingsSwitchItem* settingsSwitchItem2 =
      [[SettingsSwitchItem alloc] initWithType:ItemTypeSettingsSwitch2];
  settingsSwitchItem2.text = @"This is a disabled settings switch item";
  settingsSwitchItem2.detailText = @"This is a switch item with detail text";
  settingsSwitchItem2.on = YES;
  settingsSwitchItem2.enabled = NO;
  [model addItem:settingsSwitchItem2
      toSectionWithIdentifier:SectionIdentifierSettings];

  SyncSwitchItem* syncSwitchItem =
      [[SyncSwitchItem alloc] initWithType:ItemTypeSyncSwitch];
  syncSwitchItem.text = @"This is a sync switch item";
  syncSwitchItem.detailText = @"This is a sync switch item with detail text";
  syncSwitchItem.on = YES;
  syncSwitchItem.enabled = NO;
  [model addItem:syncSwitchItem
      toSectionWithIdentifier:SectionIdentifierSettings];

  SettingsImageDetailTextItem* imageDetailTextItem =
      [[SettingsImageDetailTextItem alloc]
          initWithType:ItemTypeSettingsSyncError];
  imageDetailTextItem.text = @"This is an error description about sync";
  imageDetailTextItem.detailText =
      @"This is more detail about the sync error description";
  imageDetailTextItem.image = [ChromeIcon infoIcon];
  [model addItem:imageDetailTextItem
      toSectionWithIdentifier:SectionIdentifierSettings];

  EncryptionItem* encryptionChecked =
      [[EncryptionItem alloc] initWithType:ItemTypeEncryption];
  encryptionChecked.text =
      @"These two cells have exactly the same text, but one has a checkmark "
      @"and the other does not.  They should lay out identically, and the "
      @"presence of the checkmark should not cause the text to reflow.";
  encryptionChecked.accessoryType = UITableViewCellAccessoryCheckmark;
  [model addItem:encryptionChecked
      toSectionWithIdentifier:SectionIdentifierSettings];

  EncryptionItem* encryptionUnchecked =
      [[EncryptionItem alloc] initWithType:ItemTypeEncryption];
  encryptionUnchecked.text =
      @"These two cells have exactly the same text, but one has a checkmark "
      @"and the other does not.  They should lay out identically, and the "
      @"presence of the checkmark should not cause the text to reflow.";
  [model addItem:encryptionUnchecked
      toSectionWithIdentifier:SectionIdentifierSettings];

  TableViewLinkHeaderFooterItem* linkFooter =
      [[TableViewLinkHeaderFooterItem alloc] initWithType:ItemTypeLinkFooter];
  linkFooter.text =
      @"This is a footer text view with a BEGIN_LINKlinkEND_LINK in the middle";
  [model setFooter:linkFooter
      forSectionWithIdentifier:SectionIdentifierSettings];

  // SectionIdentifierAutofill.
  TableViewTextHeaderFooterItem* autofillHeader =
      [[TableViewTextHeaderFooterItem alloc] initWithType:ItemTypeTextHeader];
  autofillHeader.text = @"Autofill";
  [model setHeader:autofillHeader
      forSectionWithIdentifier:SectionIdentifierAutofill];

  AutofillEditItem* autofillEditItem =
      [[AutofillEditItem alloc] initWithType:ItemTypeAutofillEditItem];
  autofillEditItem.textFieldName = @"Autofill field";
  autofillEditItem.textFieldValue = @" with a value";
  autofillEditItem.identifyingIcon =
      [UIImage imageNamed:@"table_view_cell_check_mark"];
  [model addItem:autofillEditItem
      toSectionWithIdentifier:SectionIdentifierAutofill];

  AutofillDataItem* autofillItemWithMainLeading =
      [[AutofillDataItem alloc] initWithType:ItemTypeAutofillData];
  autofillItemWithMainLeading.text = @"Main Text";
  autofillItemWithMainLeading.trailingDetailText = @"Trailing Detail Text";
  [model addItem:autofillItemWithMainLeading
      toSectionWithIdentifier:SectionIdentifierAutofill];

  AutofillDataItem* autofillItemWithLeading =
      [[AutofillDataItem alloc] initWithType:ItemTypeAutofillData];
  autofillItemWithLeading.text = @"Main Text";
  autofillItemWithLeading.leadingDetailText = @"Leading Detail Text";
  autofillItemWithLeading.accessoryType =
      UITableViewCellAccessoryDisclosureIndicator;
  [model addItem:autofillItemWithLeading
      toSectionWithIdentifier:SectionIdentifierAutofill];

  AutofillDataItem* autofillItemWithAllTexts =
      [[AutofillDataItem alloc] initWithType:ItemTypeAutofillData];
  autofillItemWithAllTexts.text = @"Main Text";
  autofillItemWithAllTexts.leadingDetailText = @"Leading Detail Text";
  autofillItemWithAllTexts.trailingDetailText = @"Trailing Detail Text";
  autofillItemWithAllTexts.accessoryType =
      UITableViewCellAccessoryDisclosureIndicator;
  [model addItem:autofillItemWithAllTexts
      toSectionWithIdentifier:SectionIdentifierAutofill];

  CopiedToChromeItem* copiedToChrome =
      [[CopiedToChromeItem alloc] initWithType:ItemTypeAutofillData];
  [model addItem:copiedToChrome
      toSectionWithIdentifier:SectionIdentifierAutofill];

  // SectionIdentifierAccount.
  TableViewSigninPromoItem* signinPromo =
      [[TableViewSigninPromoItem alloc] initWithType:ItemTypeAccount];
  signinPromo.configurator = [[SigninPromoViewConfigurator alloc]
      initWithUserEmail:@"jonhdoe@example.com"
           userFullName:@"John Doe"
              userImage:nil
         hasCloseButton:NO];
  signinPromo.text = @"Signin promo text example";
  [model addItem:signinPromo toSectionWithIdentifier:SectionIdentifierAccount];

  TableViewAccountItem* accountItemDetailWithError =
      [[TableViewAccountItem alloc] initWithType:ItemTypeAccount];
  // TODO(crbug.com/754032): ios_default_avatar image is from a downstream iOS
  // internal repository. It should be used through a provider API instead.
  accountItemDetailWithError.image = [UIImage imageNamed:@"ios_default_avatar"];
  accountItemDetailWithError.text = @"Account User Name";
  accountItemDetailWithError.detailText =
      @"Syncing to AccountUserNameAccount@example.com";
  accountItemDetailWithError.accessoryType =
      UITableViewCellAccessoryDisclosureIndicator;
  accountItemDetailWithError.shouldDisplayError = YES;
  [model addItem:accountItemDetailWithError
      toSectionWithIdentifier:SectionIdentifierAccount];

  TableViewAccountItem* accountItemCheckMark =
      [[TableViewAccountItem alloc] initWithType:ItemTypeAccount];
  // TODO(crbug.com/754032): ios_default_avatar image is from a downstream iOS
  // internal repository. It should be used through a provider API instead.
  accountItemCheckMark.image = [UIImage imageNamed:@"ios_default_avatar"];
  accountItemCheckMark.text = @"Lorem ipsum dolor sit amet, consectetur "
                              @"adipiscing elit, sed do eiusmod tempor "
                              @"incididunt ut labore et dolore magna aliqua.";
  accountItemCheckMark.detailText =
      @"Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do "
      @"eiusmod tempor incididunt ut labore et dolore magna aliqua.";
  accountItemCheckMark.accessoryType = UITableViewCellAccessoryCheckmark;
  [model addItem:accountItemCheckMark
      toSectionWithIdentifier:SectionIdentifierAccount];

  AccountControlItem* accountControlItem =
      [[AccountControlItem alloc] initWithType:ItemTypeAccount];
  accountControlItem.image = [UIImage imageNamed:@"settings_sync"];
  accountControlItem.text = @"Account Sync Settings";
  accountControlItem.detailText = @"Detail text";
  accountControlItem.accessoryType =
      UITableViewCellAccessoryDisclosureIndicator;
  [model addItem:accountControlItem
      toSectionWithIdentifier:SectionIdentifierAccount];

  AccountControlItem* accountControlItemWithExtraLongText =
      [[AccountControlItem alloc] initWithType:ItemTypeAccount];
  accountControlItemWithExtraLongText.image = [ChromeIcon infoIcon];
  accountControlItemWithExtraLongText.text =
      @"Account Control Settings - long title";
  accountControlItemWithExtraLongText.detailText =
      @"Detail text detail text detail text detail text detail text.";
  accountControlItemWithExtraLongText.accessoryType =
      UITableViewCellAccessoryDisclosureIndicator;
  [model addItem:accountControlItemWithExtraLongText
      toSectionWithIdentifier:SectionIdentifierAccount];

  // SectionIdentifierURL.
  TableViewURLItem* item =
      [[TableViewURLItem alloc] initWithType:ItemTypeURLNoMetadata];
  item.title = @"Google Design";
  item.URL = GURL("https://design.google.com");
  [model addItem:item toSectionWithIdentifier:SectionIdentifierURL];

  item = [[TableViewURLItem alloc] initWithType:ItemTypeURLNoMetadata];
  item.URL = GURL("https://notitle.google.com");
  [model addItem:item toSectionWithIdentifier:SectionIdentifierURL];

  item = [[TableViewURLItem alloc] initWithType:ItemTypeURLWithTimestamp];
  item.title = @"Google";
  item.URL = GURL("https://www.google.com");
  item.metadata = @"3:42 PM";
  [model addItem:item toSectionWithIdentifier:SectionIdentifierURL];

  item = [[TableViewURLItem alloc] initWithType:ItemTypeURLWithSize];
  item.title = @"World Series 2017: Houston Astros Defeat Someone Else";
  item.URL = GURL("https://m.bbc.com");
  item.metadata = @"176 KB";
  [model addItem:item toSectionWithIdentifier:SectionIdentifierURL];

  item =
      [[TableViewURLItem alloc] initWithType:ItemTypeURLWithSupplementalText];
  item.title = @"Chrome | Google Blog";
  item.URL = GURL("https://blog.google/products/chrome/");
  item.supplementalURLText = @"Read 4 days ago";
  [model addItem:item toSectionWithIdentifier:SectionIdentifierURL];

  item = [[TableViewURLItem alloc] initWithType:ItemTypeURLWithBadgeImage];
  item.title = @"Photos - Google Photos";
  item.URL = GURL("https://photos.google.com/");
  item.badgeImage = [UIImage imageNamed:@"table_view_cell_check_mark"];
  [model addItem:item toSectionWithIdentifier:SectionIdentifierURL];
}

@end
