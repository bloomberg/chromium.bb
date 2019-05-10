// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/language_settings_table_view_controller.h"

#include <map>
#include <memory>
#include <vector>

#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "components/language/core/browser/language_model_manager.h"
#include "components/language/core/browser/pref_names.h"
#include "components/language/core/common/language_util.h"
#include "components/prefs/ios/pref_observer_bridge.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/translate/core/browser/translate_pref_names.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/language/language_model_manager_factory.h"
#include "ios/chrome/browser/translate/chrome_ios_translate_client.h"
#include "ios/chrome/browser/translate/translate_service_ios.h"
#import "ios/chrome/browser/ui/list_model/list_model.h"
#import "ios/chrome/browser/ui/settings/cells/language_item.h"
#import "ios/chrome/browser/ui/settings/cells/settings_cells_constants.h"
#import "ios/chrome/browser/ui/settings/cells/settings_switch_cell.h"
#import "ios/chrome/browser/ui/settings/cells/settings_switch_item.h"
#import "ios/chrome/browser/ui/settings/utils/pref_backed_boolean.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_cells_constants.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_item.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

NSString* const kLanguageSettingsTableViewAccessibilityIdentifier =
    @"language_settings_table_view";
NSString* const kAddLanguageButtonAccessibilityIdentifier =
    @"add_language_button";
NSString* const kTranslateSwitchAccessibilityIdentifier = @"translate_switch";

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierLanguages = kSectionIdentifierEnumZero,
  SectionIdentifierTranslate,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeHeader = kItemTypeEnumZero,
  ItemTypeLanguage,  // This is a repeating type.
  ItemTypeAddLanguage,
  ItemTypeTranslateSwitch
};

}  // namespace

@interface LanguageSettingsTableViewController () <BooleanObserver,
                                                   PrefObserverDelegate> {
  // Registrar for pref change notifications.
  std::unique_ptr<PrefChangeRegistrar> _prefChangeRegistrar;

  // Pref observer to track changes to language::prefs::kAcceptLanguages.
  std::unique_ptr<PrefObserverBridge> _acceptLanguagesPrefObserverBridge;

  // Pref observer to track changes to language::prefs::kFluentLanguages.
  std::unique_ptr<PrefObserverBridge> _fluentLanguagesPrefObserverBridge;

  // Translate wrapper for the PrefService.
  std::unique_ptr<translate::TranslatePrefs> _translatePrefs;
}

// The BrowserState passed to this instance.
@property(nonatomic, assign) ios::ChromeBrowserState* browserState;

// Whether or not prefs::kOfferTranslateEnabled pref is enabled.
@property(nonatomic, strong) PrefBackedBoolean* translateEnabled;

// A reference to the Add language item for quick access.
@property(nonatomic, weak) TableViewTextItem* addLanguageItem;

// A reference to the Translate switch item for quick access.
@property(nonatomic, weak) SettingsSwitchItem* translateSwitchItem;

@end

@implementation LanguageSettingsTableViewController

- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState {
  DCHECK(browserState);
  UITableViewStyle style = base::FeatureList::IsEnabled(kSettingsRefresh)
                               ? UITableViewStylePlain
                               : UITableViewStyleGrouped;
  self = [super initWithTableViewStyle:style
                           appBarStyle:ChromeTableViewControllerStyleNoAppBar];
  if (self) {
    _browserState = browserState;

    _translateEnabled = [[PrefBackedBoolean alloc]
        initWithPrefService:_browserState->GetPrefs()
                   prefName:prefs::kOfferTranslateEnabled];
    [_translateEnabled setObserver:self];

    _prefChangeRegistrar = std::make_unique<PrefChangeRegistrar>();
    _prefChangeRegistrar->Init(_browserState->GetPrefs());
    _acceptLanguagesPrefObserverBridge =
        std::make_unique<PrefObserverBridge>(self);
    _acceptLanguagesPrefObserverBridge->ObserveChangesForPreference(
        language::prefs::kAcceptLanguages, _prefChangeRegistrar.get());
    _fluentLanguagesPrefObserverBridge =
        std::make_unique<PrefObserverBridge>(self);
    _fluentLanguagesPrefObserverBridge->ObserveChangesForPreference(
        language::prefs::kFluentLanguages, _prefChangeRegistrar.get());

    _translatePrefs = ChromeIOSTranslateClient::CreateTranslatePrefs(
        _browserState->GetPrefs());
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  self.title = l10n_util::GetNSString(IDS_IOS_LANGUAGE_SETTINGS_TITLE);
  self.shouldHideDoneButton = YES;
  self.tableView.accessibilityIdentifier =
      kLanguageSettingsTableViewAccessibilityIdentifier;

  [self loadModel];
  [self updateUIForEditState];
}

#pragma mark - ChromeTableViewController

- (void)loadModel {
  [super loadModel];

  TableViewModel* model = self.tableViewModel;
  [model addSectionWithIdentifier:SectionIdentifierLanguages];
  [self populateLanguagesSection];

  // Translate switch item.
  [model addSectionWithIdentifier:SectionIdentifierTranslate];
  SettingsSwitchItem* translateSwitchItem =
      [[SettingsSwitchItem alloc] initWithType:ItemTypeTranslateSwitch];
  self.translateSwitchItem = translateSwitchItem;
  translateSwitchItem.accessibilityIdentifier =
      kTranslateSwitchAccessibilityIdentifier;
  translateSwitchItem.text =
      l10n_util::GetNSString(IDS_IOS_LANGUAGE_SETTINGS_TRANSLATE_SWITCH_TITLE);
  translateSwitchItem.detailText = l10n_util::GetNSString(
      IDS_IOS_LANGUAGE_SETTINGS_TRANSLATE_SWITCH_SUBTITLE);
  translateSwitchItem.on = self.translateEnabled.value;
  [model addItem:translateSwitchItem
      toSectionWithIdentifier:SectionIdentifierTranslate];
}

#pragma mark - SettingsRootTableViewController

- (BOOL)shouldShowEditButton {
  return YES;
}

- (BOOL)editButtonEnabled {
  return [self.tableViewModel hasItemForItemType:ItemTypeLanguage
                               sectionIdentifier:SectionIdentifierLanguages];
}

- (void)updateUIForEditState {
  [super updateUIForEditState];

  [self setAddLanguageItemEnabled:!self.isEditing];
  [self setTranslateSwitchItemEnabled:!self.isEditing];
}

#pragma mark - UITableViewDelegate

- (UITableViewCellEditingStyle)tableView:(UITableView*)tableView
           editingStyleForRowAtIndexPath:(NSIndexPath*)indexPath {
  TableViewModel* model = self.tableViewModel;

  // Only language items are removable.
  TableViewItem* item = [model itemAtIndexPath:indexPath];
  if (item.type != ItemTypeLanguage)
    return UITableViewCellEditingStyleNone;

  // The last Translate-blocked language cannot be deleted.
  LanguageItem* languageItem = base::mac::ObjCCastStrict<LanguageItem>(item);
  return (languageItem.isBlocked && [self numberOfBlockedLanguages] <= 1)
             ? UITableViewCellEditingStyleNone
             : UITableViewCellEditingStyleDelete;
}

- (NSIndexPath*)tableView:(UITableView*)tableView
    willSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  if (![super tableView:tableView willSelectRowAtIndexPath:indexPath])
    return nil;

  // Ignore selection of language items when Translate is disabled.
  NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:indexPath];
  return (itemType != ItemTypeLanguage || self.translateEnabled.value)
             ? indexPath
             : nil;
}

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  [super tableView:tableView didSelectRowAtIndexPath:indexPath];

  // TODO(crbug.com/957688): Handle selections.
}

- (NSIndexPath*)tableView:(UITableView*)tableView
    targetIndexPathForMoveFromRowAtIndexPath:(NSIndexPath*)sourceIndexPath
                         toProposedIndexPath:
                             (NSIndexPath*)proposedDestinationIndexPath {
  // Allow language items to move in their own section. Also prevent moving to
  // the last row of the section which is reserved for the add language item.
  NSInteger lastRowIndex =
      [self.tableViewModel numberOfItemsInSection:sourceIndexPath.section] - 1;
  NSInteger lastValidRowIndex = lastRowIndex - 1;
  if (sourceIndexPath.section != proposedDestinationIndexPath.section) {
    NSUInteger rowInSourceSection =
        (sourceIndexPath.section > proposedDestinationIndexPath.section)
            ? 0
            : lastValidRowIndex;
    return [NSIndexPath indexPathForRow:rowInSourceSection
                              inSection:sourceIndexPath.section];
  } else if (proposedDestinationIndexPath.row >= lastRowIndex) {
    return [NSIndexPath indexPathForRow:lastValidRowIndex
                              inSection:sourceIndexPath.section];
  }
  // Allow the proposed destination.
  return proposedDestinationIndexPath;
}

#pragma mark - UITableViewDataSource

- (BOOL)tableView:(UITableView*)tableView
    canEditRowAtIndexPath:(NSIndexPath*)indexPath {
  // Only language items are editable.
  NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:indexPath];
  return itemType == ItemTypeLanguage;
}

- (BOOL)tableView:(UITableView*)tableView
    shouldIndentWhileEditingRowAtIndexPath:(NSIndexPath*)indexPath {
  return NO;
}

- (void)tableView:(UITableView*)tableView
    commitEditingStyle:(UITableViewCellEditingStyle)editingStyle
     forRowAtIndexPath:(NSIndexPath*)indexPath {
  DCHECK_EQ(editingStyle, UITableViewCellEditingStyleDelete);

  LanguageItem* languageItem = base::mac::ObjCCastStrict<LanguageItem>(
      [self.tableViewModel itemAtIndexPath:indexPath]);

  // Update the model and the table view.
  [self deleteItems:[NSArray arrayWithObject:indexPath]];

  // Update the pref.
  _translatePrefs->RemoveFromLanguageList(languageItem.languageCode);
}

- (BOOL)tableView:(UITableView*)tableView
    canMoveRowAtIndexPath:(NSIndexPath*)indexPath {
  // Only language items can be reordered.
  NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:indexPath];
  return itemType == ItemTypeLanguage;
}

- (void)tableView:(UITableView*)tableView
    moveRowAtIndexPath:(NSIndexPath*)sourceIndexPath
           toIndexPath:(NSIndexPath*)destinationIndexPath {
  if (sourceIndexPath.row == destinationIndexPath.row) {
    return;
  }

  // Update the model.
  TableViewModel* model = self.tableViewModel;
  LanguageItem* languageItem = base::mac::ObjCCastStrict<LanguageItem>(
      [model itemAtIndexPath:sourceIndexPath]);
  [model removeItemWithType:ItemTypeLanguage
      fromSectionWithIdentifier:SectionIdentifierLanguages
                        atIndex:sourceIndexPath.row];
  [model insertItem:languageItem
      inSectionWithIdentifier:SectionIdentifierLanguages
                      atIndex:destinationIndexPath.row];

  // Update the pref.
  translate::TranslatePrefs::RearrangeSpecifier where =
      sourceIndexPath.row < destinationIndexPath.row
          ? translate::TranslatePrefs::kDown
          : translate::TranslatePrefs::kUp;
  const int offset = abs(sourceIndexPath.row - destinationIndexPath.row);
  std::vector<std::string> languageCodes;
  _translatePrefs->GetLanguageList(&languageCodes);
  _translatePrefs->RearrangeLanguage(languageItem.languageCode, where, offset,
                                     languageCodes);
}

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];
  switch ([self.tableViewModel itemTypeForIndexPath:indexPath]) {
    case ItemTypeTranslateSwitch: {
      SettingsSwitchCell* switchCell =
          base::mac::ObjCCastStrict<SettingsSwitchCell>(cell);
      [switchCell.switchView addTarget:self
                                action:@selector(translateSwitchChanged:)
                      forControlEvents:UIControlEventValueChanged];
      break;
    }
  }
  return cell;
}

#pragma mark - BooleanObserver

// Called when the value of prefs::kOfferTranslateEnabled changes.
- (void)booleanDidChange:(id<ObservableBoolean>)observableBoolean {
  // Ingnore pref changes while in edit mode.
  if (self.isEditing)
    return;

  DCHECK_EQ(self.translateEnabled, observableBoolean);

  // Update the model and the table view.
  [self setTranslateSwitchItemOn:self.translateEnabled.value];
  [self updateLanguagesSection];
}

#pragma mark - PrefObserverDelegate

// Called when the values of language::prefs::kAcceptLanguages or
// language::prefs::kFluentLanguages change.
- (void)onPreferenceChanged:(const std::string&)preferenceName {
  // Ingnore pref changes while in edit mode.
  if (self.isEditing)
    return;

  DCHECK(preferenceName == language::prefs::kAcceptLanguages ||
         preferenceName == language::prefs::kFluentLanguages);

  // Update the model and the table view.
  [self updateLanguagesSection];
}

#pragma mark - Helper methods

- (void)populateLanguagesSection {
  TableViewModel* model = self.tableViewModel;

  // Header item.
  TableViewLinkHeaderFooterItem* headerItem =
      [[TableViewLinkHeaderFooterItem alloc] initWithType:ItemTypeHeader];
  headerItem.text = l10n_util::GetNSString(IDS_IOS_LANGUAGE_SETTINGS_HEADER);
  [model setHeader:headerItem
      forSectionWithIdentifier:SectionIdentifierLanguages];

  // Populate the supported languages map.
  std::map<std::string, translate::TranslateLanguageInfo> supportedLanguagesMap;
  std::vector<translate::TranslateLanguageInfo> supportedLanguages;
  translate::TranslatePrefs::GetLanguageInfoList(
      GetApplicationContext()->GetApplicationLocale(),
      _translatePrefs->IsTranslateAllowedByPolicy(), &supportedLanguages);
  for (const auto& entry : supportedLanguages) {
    supportedLanguagesMap[entry.code] = entry;
  }

  // Languages items.
  std::vector<std::string> languageCodes;
  _translatePrefs->GetLanguageList(&languageCodes);
  for (const auto& languageCode : languageCodes) {
    // Ignore unsupported languages.
    auto it = supportedLanguagesMap.find(languageCode);
    if (it == supportedLanguagesMap.end())
      continue;

    const translate::TranslateLanguageInfo& language = it->second;
    LanguageItem* languageItem =
        [[LanguageItem alloc] initWithType:ItemTypeLanguage];
    languageItem.languageCode = language.code;
    languageItem.text = base::SysUTF8ToNSString(language.display_name);
    languageItem.leadingDetailText =
        base::SysUTF8ToNSString(language.native_display_name);
    languageItem.blocked = _translatePrefs->IsBlockedLanguage(language.code);
    if (self.translateEnabled.value) {
      // Show a disclosure indicator to suggest Translate options are available
      // as well as a label indicating if the language is Translate-blocked.
      languageItem.accessibilityTraits |= UIAccessibilityTraitButton;
      languageItem.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
      languageItem.trailingDetailText =
          languageItem.isBlocked
              ? l10n_util::GetNSString(
                    IDS_IOS_LANGUAGE_SETTINGS_NEVER_TRANSLATE_TITLE)
              : l10n_util::GetNSString(
                    IDS_IOS_LANGUAGE_SETTINGS_OFFER_TO_TRANSLATE_TITLE);
    }
    [model addItem:languageItem
        toSectionWithIdentifier:SectionIdentifierLanguages];
  }

  // Add language item.
  TableViewTextItem* addLanguageItem =
      [[TableViewTextItem alloc] initWithType:ItemTypeAddLanguage];
  self.addLanguageItem = addLanguageItem;
  addLanguageItem.accessibilityIdentifier =
      kAddLanguageButtonAccessibilityIdentifier;
  addLanguageItem.text = l10n_util::GetNSString(
      IDS_IOS_LANGUAGE_SETTINGS_ADD_LANGUAGE_BUTTON_TITLE);
  addLanguageItem.textColor = UIColorFromRGB(kTableViewTextLabelColorBlue);
  addLanguageItem.accessibilityTraits |= UIAccessibilityTraitButton;
  [self.tableViewModel addItem:addLanguageItem
       toSectionWithIdentifier:SectionIdentifierLanguages];
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

- (void)setAddLanguageItemEnabled:(BOOL)enabled {
  // Update the model.
  self.addLanguageItem.enabled = enabled;
  self.addLanguageItem.textColor =
      self.isEditing ? UIColorFromRGB(kSettingsCellsDetailTextColor)
                     : UIColorFromRGB(kTableViewTextLabelColorBlue);

  // Update the table view.
  [self reconfigureCellsForItems:@[ self.addLanguageItem ]];
}

- (void)setTranslateSwitchItemEnabled:(BOOL)enabled {
  // Update the model.
  self.translateSwitchItem.enabled = enabled;

  // Update the table view.
  [self reconfigureCellsForItems:@[ self.translateSwitchItem ]];
}

- (void)setTranslateSwitchItemOn:(BOOL)on {
  // Update the model.
  self.translateSwitchItem.on = on;

  // Update the table view.
  [self reconfigureCellsForItems:@[ self.translateSwitchItem ]];
}

- (BOOL)canOfferToTranslateLanguage:(const std::string&)languageCode
                            blocked:(BOOL)blocked {
  // Cannot offer Translate for the last Translate-blocked language.
  if (blocked && [self numberOfBlockedLanguages] <= 1) {
    return NO;
  }

  // Cannot offer Translate for the Translate target language.
  // Note the language codes used in the language settings have the Chrome
  // internal format while the Translate target language has the Translate
  // server format. To convert the former to the latter the utilily function
  // ToTranslateLanguageSynonym() must be used.
  const std::string& targetLanguageCode =
      TranslateServiceIOS::GetTargetLanguage(
          self.browserState->GetPrefs(),
          LanguageModelManagerFactory::GetForBrowserState(self.browserState)
              ->GetPrimaryModel());
  std::string canonicalLanguageCode = languageCode;
  language::ToTranslateLanguageSynonym(&canonicalLanguageCode);
  return targetLanguageCode != canonicalLanguageCode;
}

// Returns the number of Translate-blocked languages currently in the model.
- (NSUInteger)numberOfBlockedLanguages {
  __block NSUInteger numberOfBlockedLanguages = 0;
  NSArray<TableViewItem*>* languageItems = [self.tableViewModel
      itemsInSectionWithIdentifier:SectionIdentifierLanguages];
  [languageItems enumerateObjectsUsingBlock:^(TableViewItem* item,
                                              NSUInteger idx, BOOL* stop) {
    if (item.type != ItemTypeLanguage)
      return;
    LanguageItem* languageItem = base::mac::ObjCCastStrict<LanguageItem>(item);
    if (languageItem.isBlocked)
      numberOfBlockedLanguages++;
  }];
  return numberOfBlockedLanguages;
}

#pragma mark - Actions

- (void)translateSwitchChanged:(UISwitch*)switchView {
  // Update the pref.
  [self.translateEnabled setValue:switchView.isOn];

  // Update the model and the table view.
  [self setTranslateSwitchItemOn:switchView.isOn];
  [self updateLanguagesSection];
}

@end
