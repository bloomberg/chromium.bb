// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/add_language_table_view_controller.h"

#include "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/list_model/list_item+Controller.h"
#import "ios/chrome/browser/ui/settings/cells/language_item.h"
#import "ios/chrome/browser/ui/settings/language_settings_data_source.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

NSString* const kAddLanguageTableViewAccessibilityIdentifier =
    @"add_language_table_view";

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierLanguages = kSectionIdentifierEnumZero,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeLanguage = kItemTypeEnumZero,  // This is a repeating type.
};

}  // namespace

@interface AddLanguageTableViewController ()

// The data source passed to this instance.
@property(nonatomic, strong) id<LanguageSettingsDataSource> dataSource;

// The delegate passed to this instance.
@property(nonatomic, weak) id<AddLanguageTableViewControllerDelegate> delegate;

@end

@implementation AddLanguageTableViewController

- (instancetype)initWithDataSource:(id<LanguageSettingsDataSource>)dataSource
                          delegate:(id<AddLanguageTableViewControllerDelegate>)
                                       delegate {
  DCHECK(dataSource);
  DCHECK(delegate);
  UITableViewStyle style = base::FeatureList::IsEnabled(kSettingsRefresh)
                               ? UITableViewStylePlain
                               : UITableViewStyleGrouped;
  self = [super initWithTableViewStyle:style
                           appBarStyle:ChromeTableViewControllerStyleNoAppBar];
  if (self) {
    _dataSource = dataSource;
    _delegate = delegate;
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  self.title =
      l10n_util::GetNSString(IDS_IOS_LANGUAGE_SETTINGS_ADD_LANGUAGE_TITLE);
  self.shouldHideDoneButton = YES;
  self.tableView.accessibilityIdentifier =
      kAddLanguageTableViewAccessibilityIdentifier;

  [self loadModel];
}

#pragma mark - ChromeTableViewController

- (void)loadModel {
  [super loadModel];

  [self.tableViewModel addSectionWithIdentifier:SectionIdentifierLanguages];
  [self populateLanguagesSection];
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  LanguageItem* languageItem = base::mac::ObjCCastStrict<LanguageItem>(
      [self.tableViewModel itemAtIndexPath:indexPath]);

  [self.delegate addLanguageTableViewController:self
                          didSelectLanguageCode:languageItem.languageCode];
}

#pragma mark - Public methods

- (void)supportedLanguagesListChanged {
  // Update the model and the table view.
  [self updateLanguagesSection];
}

#pragma mark - Helper methods

- (void)populateLanguagesSection {
  TableViewModel* model = self.tableViewModel;

  // Languages items.
  [[self.dataSource supportedLanguagesItems]
      enumerateObjectsUsingBlock:^(LanguageItem* item, NSUInteger index,
                                   BOOL* stop) {
        item.type = ItemTypeLanguage;
        [model addItem:item toSectionWithIdentifier:SectionIdentifierLanguages];
      }];
}

- (void)updateLanguagesSection {
  // Update the model.
  [self.tableViewModel
      deleteAllItemsFromSectionWithIdentifier:SectionIdentifierLanguages];
  [self populateLanguagesSection];

  // Update the table view.
  NSUInteger index = [self.tableViewModel
      sectionForSectionIdentifier:SectionIdentifierLanguages];
  [self.tableView reloadSections:[NSIndexSet indexSetWithIndex:index]
                withRowAnimation:UITableViewRowAnimationNone];
}

@end
