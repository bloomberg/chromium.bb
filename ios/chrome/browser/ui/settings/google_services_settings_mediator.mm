// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/google_services_settings_mediator.h"

#include "components/prefs/pref_service.h"
#include "components/unified_consent/pref_names.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_text_item.h"
#import "ios/chrome/browser/ui/settings/cells/settings_collapsible_item.h"
#import "ios/chrome/browser/ui/settings/cells/sync_switch_item.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/third_party/material_components_ios/src/components/Palettes/src/MaterialPalettes.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using l10n_util::GetNSString;
using unified_consent::prefs::kUnifiedConsentGiven;

namespace {

// List of sections.
typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SyncEverythingSectionIdentifier = kSectionIdentifierEnumZero,
  PersonalizedSectionIdentifier,
  NonPersonalizedSectionIdentifier,
};

// Keys for ListModel to save collapse/expanded prefences, for each section.
NSString* const kGoogleServicesSettingsPersonalizedSectionKey =
    @"GoogleServicesSettingsPersonalizedSection";
NSString* const kGoogleServicesSettingsNonPersonalizedSectionKey =
    @"GoogleServicesSettingsNonPersonalizedSection";

// List of items.
typedef NS_ENUM(NSInteger, ItemType) {
  // SyncEverythingSectionIdentifier section.
  SyncEverythingItemType = kItemTypeEnumZero,
  // PersonalizedSectionIdentifier section.
  SyncPersonalizationItemType,
  SyncBookmarksItemType,
  SyncHistoryItemType,
  SyncPasswordsItemType,
  SyncOpenTabsItemType,
  SyncAutofillItemType,
  SyncSettingsItemType,
  SyncReadingListItemType,
  SyncActivityAndInteractionsItemType,
  SyncGoogleActivityControlsItemType,
  EncryptionItemType,
  ManageSyncedDataItemType,
  // NonPersonalizedSectionIdentifier section.
  NonPersonalizedServicesItemType,
  AutocompleteSearchesAndURLsItemType,
  PreloadPagesItemType,
  ImproveChromeItemType,
  BetterSearchAndBrowsingItemType,
};

}  // namespace

@interface GoogleServicesSettingsMediator ()

// Returns YES if the user is authenticated.
@property(nonatomic, readonly) BOOL isAuthenticated;
// Preference service.
@property(nonatomic, readonly) PrefService* prefService;

@end

@implementation GoogleServicesSettingsMediator

@synthesize consumer = _consumer;
@synthesize authService = _authService;
@synthesize prefService = _prefService;

#pragma mark - Load model

- (instancetype)initWithPrefService:(PrefService*)prefService {
  self = [super init];
  if (self) {
    DCHECK(prefService);
    _prefService = prefService;
  }
  return self;
}

// Loads SyncEverythingSectionIdentifier section.
- (void)loadSyncEverythingSection {
  CollectionViewModel* model = self.consumer.collectionViewModel;
  [model addSectionWithIdentifier:SyncEverythingSectionIdentifier];
  [model addItem:[self syncEverythingItem]
      toSectionWithIdentifier:SyncEverythingSectionIdentifier];
}

// Creates SyncEverythingItemType item.
- (CollectionViewItem*)syncEverythingItem {
  SyncSwitchItem* item =
      [[SyncSwitchItem alloc] initWithType:SyncEverythingItemType];
  item.text = GetNSString(IDS_IOS_GOOGLE_SERVICES_SETTINGS_SYNC_EVERYTHING);
  item.enabled = YES;
  item.on = [self isConsentGiven];
  return item;
}

// Loads PersonalizedSectionIdentifier section.
- (void)loadPersonalizedSection {
  CollectionViewModel* model = self.consumer.collectionViewModel;
  [model addSectionWithIdentifier:PersonalizedSectionIdentifier];
  [model setSectionIdentifier:PersonalizedSectionIdentifier
                 collapsedKey:kGoogleServicesSettingsPersonalizedSectionKey];
  SettingsCollapsibleItem* syncPersonalizationItem =
      [self syncPersonalizationItem];
  [model addItem:syncPersonalizationItem
      toSectionWithIdentifier:PersonalizedSectionIdentifier];
  BOOL collapsed = self.isAuthenticated ? [self isConsentGiven] : YES;
  syncPersonalizationItem.collapsed = collapsed;
  [model setSection:PersonalizedSectionIdentifier collapsed:collapsed];
  [model addItem:[self syncBookmarksItem]
      toSectionWithIdentifier:PersonalizedSectionIdentifier];
  [model addItem:[self syncHistoryItem]
      toSectionWithIdentifier:PersonalizedSectionIdentifier];
  [model addItem:[self syncPasswordsItem]
      toSectionWithIdentifier:PersonalizedSectionIdentifier];
  [model addItem:[self syncOpenTabsItem]
      toSectionWithIdentifier:PersonalizedSectionIdentifier];
  [model addItem:[self syncAutofillItem]
      toSectionWithIdentifier:PersonalizedSectionIdentifier];
  [model addItem:[self syncReadingListItem]
      toSectionWithIdentifier:PersonalizedSectionIdentifier];
  [model addItem:[self syncActivityAndInteractionsItem]
      toSectionWithIdentifier:PersonalizedSectionIdentifier];
  [model addItem:[self syncGoogleActivityControlsItem]
      toSectionWithIdentifier:PersonalizedSectionIdentifier];
  [model addItem:[self encryptionItem]
      toSectionWithIdentifier:PersonalizedSectionIdentifier];
  [model addItem:[self manageSyncedDataItem]
      toSectionWithIdentifier:PersonalizedSectionIdentifier];
}

// Creates SyncPersonalizationItemType item.
- (SettingsCollapsibleItem*)syncPersonalizationItem {
  SettingsCollapsibleItem* item = [[SettingsCollapsibleItem alloc]
      initWithType:SyncPersonalizationItemType];
  item.text =
      GetNSString(IDS_IOS_GOOGLE_SERVICES_SETTINGS_SYNC_PERSONALIZATION_TEXT);
  item.numberOfTextLines = 0;
  if (!self.isAuthenticated)
    item.textColor = [[MDCPalette greyPalette] tint500];
  item.detailText =
      GetNSString(IDS_IOS_GOOGLE_SERVICES_SETTINGS_SYNC_PERSONALIZATION_DETAIL);
  item.numberOfDetailTextLines = 0;
  return item;
}

// Creates SyncBookmarksItemType item.
- (CollectionViewItem*)syncBookmarksItem {
  SyncSwitchItem* item =
      [[SyncSwitchItem alloc] initWithType:SyncBookmarksItemType];
  item.text = GetNSString(IDS_IOS_GOOGLE_SERVICES_SETTINGS_BOOKMARKS_TEXT);
  item.enabled = self.isAuthenticated;
  return item;
}

// Creates SyncHistoryItemType item.
- (CollectionViewItem*)syncHistoryItem {
  SyncSwitchItem* item =
      [[SyncSwitchItem alloc] initWithType:SyncHistoryItemType];
  item.text = GetNSString(IDS_IOS_GOOGLE_SERVICES_SETTINGS_HISTORY_TEXT);
  item.enabled = self.isAuthenticated;
  return item;
}

// Creates SyncPasswordsItemType item.
- (CollectionViewItem*)syncPasswordsItem {
  SyncSwitchItem* item =
      [[SyncSwitchItem alloc] initWithType:SyncPasswordsItemType];
  item.text = GetNSString(IDS_IOS_GOOGLE_SERVICES_SETTINGS_PASSWORD_TEXT);
  item.enabled = self.isAuthenticated;
  return item;
}

// Creates SyncOpenTabsItemType item.
- (CollectionViewItem*)syncOpenTabsItem {
  SyncSwitchItem* item =
      [[SyncSwitchItem alloc] initWithType:SyncOpenTabsItemType];
  item.text = GetNSString(IDS_IOS_GOOGLE_SERVICES_SETTINGS_OPENTABS_TEXT);
  item.enabled = self.isAuthenticated;
  return item;
}

// Creates SyncAutofillItemType item.
- (CollectionViewItem*)syncAutofillItem {
  SyncSwitchItem* item =
      [[SyncSwitchItem alloc] initWithType:SyncAutofillItemType];
  item.text = GetNSString(IDS_IOS_GOOGLE_SERVICES_SETTINGS_AUTOFILL_TEXT);
  item.enabled = self.isAuthenticated;
  return item;
}

// Creates SyncReadingListItemType item.
- (CollectionViewItem*)syncReadingListItem {
  SyncSwitchItem* item =
      [[SyncSwitchItem alloc] initWithType:SyncReadingListItemType];
  item.text = GetNSString(IDS_IOS_GOOGLE_SERVICES_SETTINGS_READING_LIST_TEXT);
  item.enabled = self.isAuthenticated;
  return item;
}

// Creates SyncActivityAndInteractionsItemType item.
- (CollectionViewItem*)syncActivityAndInteractionsItem {
  SyncSwitchItem* item =
      [[SyncSwitchItem alloc] initWithType:SyncActivityAndInteractionsItemType];
  item.text = GetNSString(
      IDS_IOS_GOOGLE_SERVICES_SETTINGS_ACTIVITY_AND_INTERACTIONS_TEXT);
  item.detailText = GetNSString(
      IDS_IOS_GOOGLE_SERVICES_SETTINGS_ACTIVITY_AND_INTERACTIONS_DETAIL);
  item.enabled = self.isAuthenticated;
  return item;
}

// Creates SyncGoogleActivityControlsItemType item.
- (CollectionViewItem*)syncGoogleActivityControlsItem {
  CollectionViewTextItem* item = [[CollectionViewTextItem alloc]
      initWithType:SyncGoogleActivityControlsItemType];
  item.text = GetNSString(
      IDS_IOS_GOOGLE_SERVICES_SETTINGS_GOOGLE_ACTIVITY_CONTROL_TEXT);
  item.numberOfTextLines = 0;
  if (!self.isAuthenticated)
    item.textColor = [[MDCPalette greyPalette] tint500];
  item.detailText = GetNSString(
      IDS_IOS_GOOGLE_SERVICES_SETTINGS_GOOGLE_ACTIVITY_CONTROL_DETAIL);
  item.numberOfDetailTextLines = 0;
  item.accessoryType = MDCCollectionViewCellAccessoryDisclosureIndicator;
  return item;
}

// Creates EncryptionItemType item.
- (CollectionViewItem*)encryptionItem {
  CollectionViewTextItem* item =
      [[CollectionViewTextItem alloc] initWithType:EncryptionItemType];
  item.text = GetNSString(IDS_IOS_GOOGLE_SERVICES_SETTINGS_ENCRYPTION_TEXT);
  item.numberOfTextLines = 0;
  if (!self.isAuthenticated)
    item.textColor = [[MDCPalette greyPalette] tint500];
  item.numberOfDetailTextLines = 0;
  item.accessoryType = MDCCollectionViewCellAccessoryDisclosureIndicator;
  return item;
}

// Creates ManageSyncedDataItemType item.
- (CollectionViewItem*)manageSyncedDataItem {
  CollectionViewTextItem* item =
      [[CollectionViewTextItem alloc] initWithType:ManageSyncedDataItemType];
  item.text =
      GetNSString(IDS_IOS_GOOGLE_SERVICES_SETTINGS_MANAGED_SYNC_DATA_TEXT);
  item.numberOfTextLines = 0;
  if (!self.isAuthenticated)
    item.textColor = [[MDCPalette greyPalette] tint500];
  return item;
}

// Loads NonPersonalizedSectionIdentifier section.
- (void)loadNonPersonalizedSection {
  CollectionViewModel* model = self.consumer.collectionViewModel;
  [model addSectionWithIdentifier:NonPersonalizedSectionIdentifier];
  [model setSectionIdentifier:NonPersonalizedSectionIdentifier
                 collapsedKey:kGoogleServicesSettingsNonPersonalizedSectionKey];
  SettingsCollapsibleItem* nonPersonalizedServicesItem =
      [self nonPersonalizedServicesItem];
  [model addItem:nonPersonalizedServicesItem
      toSectionWithIdentifier:NonPersonalizedSectionIdentifier];
  BOOL collapsed = self.isAuthenticated ? [self isConsentGiven] : NO;
  nonPersonalizedServicesItem.collapsed = collapsed;
  [model setSection:NonPersonalizedSectionIdentifier collapsed:collapsed];
  [model addItem:[self autocompleteSearchesAndURLsItem]
      toSectionWithIdentifier:NonPersonalizedSectionIdentifier];
  [model addItem:[self preloadPagesItem]
      toSectionWithIdentifier:NonPersonalizedSectionIdentifier];
  [model addItem:[self improveChromeItem]
      toSectionWithIdentifier:NonPersonalizedSectionIdentifier];
  [model addItem:[self betterSearchAndBrowsingItemType]
      toSectionWithIdentifier:NonPersonalizedSectionIdentifier];
}

// Creates NonPersonalizedServicesItemType item.
- (SettingsCollapsibleItem*)nonPersonalizedServicesItem {
  SettingsCollapsibleItem* item = [[SettingsCollapsibleItem alloc]
      initWithType:NonPersonalizedServicesItemType];
  item.text = GetNSString(
      IDS_IOS_GOOGLE_SERVICES_SETTINGS_NON_PERSONALIZED_SERVICES_TEXT);
  item.numberOfTextLines = 0;
  item.detailText = GetNSString(
      IDS_IOS_GOOGLE_SERVICES_SETTINGS_NON_PERSONALIZED_SERVICES_DETAIL);
  item.numberOfDetailTextLines = 0;
  return item;
}

// Creates AutocompleteSearchesAndURLsItemType item.
- (CollectionViewItem*)autocompleteSearchesAndURLsItem {
  SyncSwitchItem* item =
      [[SyncSwitchItem alloc] initWithType:AutocompleteSearchesAndURLsItemType];
  item.text = GetNSString(
      IDS_IOS_GOOGLE_SERVICES_SETTINGS_AUTOCOMPLETE_SEARCHES_AND_URLS_TEXT);
  item.detailText = GetNSString(
      IDS_IOS_GOOGLE_SERVICES_SETTINGS_AUTOCOMPLETE_SEARCHES_AND_URLS_DETAIL);
  item.enabled = YES;
  return item;
}

// Creates PreloadPagesItemType item.
- (CollectionViewItem*)preloadPagesItem {
  SyncSwitchItem* item =
      [[SyncSwitchItem alloc] initWithType:PreloadPagesItemType];
  item.text = GetNSString(IDS_IOS_GOOGLE_SERVICES_SETTINGS_PRELOAD_PAGES_TEXT);
  item.detailText =
      GetNSString(IDS_IOS_GOOGLE_SERVICES_SETTINGS_PRELOAD_PAGES_DETAIL);
  item.enabled = YES;
  return item;
}

// Creates ImproveChromeItemType item.
- (CollectionViewItem*)improveChromeItem {
  SyncSwitchItem* item =
      [[SyncSwitchItem alloc] initWithType:ImproveChromeItemType];
  item.text = GetNSString(IDS_IOS_GOOGLE_SERVICES_SETTINGS_IMPROVE_CHROME_TEXT);
  item.detailText =
      GetNSString(IDS_IOS_GOOGLE_SERVICES_SETTINGS_IMPROVE_CHROME_DETAIL);
  item.enabled = YES;
  return item;
}

// Creates BetterSearchAndBrowsingItemType item.
- (CollectionViewItem*)betterSearchAndBrowsingItemType {
  SyncSwitchItem* item =
      [[SyncSwitchItem alloc] initWithType:BetterSearchAndBrowsingItemType];
  item.text = GetNSString(
      IDS_IOS_GOOGLE_SERVICES_SETTINGS_BETTER_SEARCH_AND_BROWSING_TEXT);
  item.detailText = GetNSString(
      IDS_IOS_GOOGLE_SERVICES_SETTINGS_BETTER_SEARCH_AND_BROWSING_DETAIL);
  item.enabled = YES;
  return item;
}

#pragma mark - Private

- (BOOL)isAuthenticated {
  return self.authService->IsAuthenticated();
}

- (BOOL)isConsentGiven {
  return self.prefService->GetBoolean(kUnifiedConsentGiven);
}

#pragma mark - GoogleServicesSettingsViewControllerModelDelegate

- (void)googleServicesSettingsViewControllerLoadModel:
    (GoogleServicesSettingsViewController*)controller {
  DCHECK_EQ(self.consumer, controller);
  self.consumer.collectionViewModel.collapsableMode =
      ListModelCollapsableModeFirstCell;
  if (self.isAuthenticated)
    [self loadSyncEverythingSection];
  [self loadPersonalizedSection];
  [self loadNonPersonalizedSection];
}

@end
