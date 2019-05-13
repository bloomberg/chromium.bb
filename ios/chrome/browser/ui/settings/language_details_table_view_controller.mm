// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/language_details_table_view_controller.h"

#include "base/feature_list.h"
#import "ios/chrome/browser/ui/settings/cells/settings_cells_constants.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_cells_constants.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_item.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

NSString* const kLanguageDetailsTableViewAccessibilityIdentifier =
    @"language_details_table_view";

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierOptions = kSectionIdentifierEnumZero,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeNeverTranslate = kItemTypeEnumZero,
  ItemTypeOfferTranslate,
};

}  // namespace

@interface LanguageDetailsTableViewController ()

// The language code for the given language.
@property(nonatomic, copy) NSString* languageCode;

// The name of the given language.
@property(nonatomic, copy) NSString* languageName;

// Whether the language is Translate-blocked.
@property(nonatomic, getter=isBlocked) BOOL blocked;

// Whether Translate can be offered for the language, i.e., it can be unblocked.
@property(nonatomic) BOOL canOfferTranslate;

@end

@implementation LanguageDetailsTableViewController

- (instancetype)initWithLanguageCode:(NSString*)languageCode
                        languageName:(NSString*)languageName
                             blocked:(BOOL)blocked
                   canOfferTranslate:(BOOL)canOfferTranslate {
  UITableViewStyle style = base::FeatureList::IsEnabled(kSettingsRefresh)
                               ? UITableViewStylePlain
                               : UITableViewStyleGrouped;
  self = [super initWithTableViewStyle:style
                           appBarStyle:ChromeTableViewControllerStyleNoAppBar];
  if (self) {
    _languageCode = [languageCode copy];
    _languageName = [languageName copy];
    _blocked = blocked;
    _canOfferTranslate = canOfferTranslate;
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  self.title = self.languageName;
  self.shouldHideDoneButton = YES;
  self.tableView.accessibilityIdentifier =
      kLanguageDetailsTableViewAccessibilityIdentifier;

  [self loadModel];
}

#pragma mark - ChromeTableViewController

- (void)loadModel {
  [super loadModel];

  TableViewModel* model = self.tableViewModel;
  [model addSectionWithIdentifier:SectionIdentifierOptions];

  // Never translate item.
  TableViewTextItem* neverTranslateItem =
      [[TableViewTextItem alloc] initWithType:ItemTypeNeverTranslate];
  neverTranslateItem.text =
      l10n_util::GetNSString(IDS_IOS_LANGUAGE_SETTINGS_NEVER_TRANSLATE_TITLE);
  neverTranslateItem.accessibilityTraits |= UIAccessibilityTraitButton;
  neverTranslateItem.accessoryType = self.isBlocked
                                         ? UITableViewCellAccessoryCheckmark
                                         : UITableViewCellAccessoryNone;
  [model addItem:neverTranslateItem
      toSectionWithIdentifier:SectionIdentifierOptions];

  // Offer to translate item.
  TableViewTextItem* offerTranslateItem =
      [[TableViewTextItem alloc] initWithType:ItemTypeOfferTranslate];
  offerTranslateItem.text = l10n_util::GetNSString(
      IDS_IOS_LANGUAGE_SETTINGS_OFFER_TO_TRANSLATE_TITLE);
  offerTranslateItem.accessibilityTraits |= UIAccessibilityTraitButton;
  offerTranslateItem.accessoryType = self.isBlocked
                                         ? UITableViewCellAccessoryNone
                                         : UITableViewCellAccessoryCheckmark;
  if (!self.canOfferTranslate) {
    offerTranslateItem.enabled = NO;
    offerTranslateItem.textColor =
        UIColorFromRGB(kSettingsCellsDetailTextColor);
  }
  [model addItem:offerTranslateItem
      toSectionWithIdentifier:SectionIdentifierOptions];
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  NSInteger type = [self.tableViewModel itemTypeForIndexPath:indexPath];
  [self.delegate
      languageDetailsTableViewController:self
                 didSelectOfferTranslate:(type == ItemTypeOfferTranslate)
                            languageCode:self.languageCode];
}

@end
