// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/google_services_settings_mediator.h"

#include "base/auto_reset.h"
#include "base/mac/foundation_util.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/unified_consent/pref_names.h"
#include "ios/chrome/browser/pref_names.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/ui/settings/cells/settings_image_detail_text_item.h"
#import "ios/chrome/browser/ui/settings/cells/sync_switch_item.h"
#import "ios/chrome/browser/ui/settings/google_services_settings_command_handler.h"
#import "ios/chrome/browser/ui/settings/sync_utils/sync_util.h"
#import "ios/chrome/browser/ui/settings/utils/observable_boolean.h"
#import "ios/chrome/browser/ui/settings/utils/pref_backed_boolean.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using l10n_util::GetNSString;

typedef NSArray<TableViewItem*>* ItemArray;

namespace {

// List of sections.
typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SyncFeedbackSectionIdentifier = kSectionIdentifierEnumZero,
  NonPersonalizedSectionIdentifier,
};

// List of items.
typedef NS_ENUM(NSInteger, ItemType) {
  // SyncErrorSectionIdentifier,
  RestartAuthenticationFlowErrorItemType = kItemTypeEnumZero,
  ReauthDialogAsSyncIsInAuthErrorItemType,
  ShowPassphraseDialogErrorItemType,
  // NonPersonalizedSectionIdentifier section.
  AutocompleteSearchesAndURLsItemType,
  PreloadPagesItemType,
  ImproveChromeItemType,
  BetterSearchAndBrowsingItemType,
};

}  // namespace

@interface GoogleServicesSettingsMediator () <BooleanObserver>

// Unified consent service.
@property(nonatomic, assign)
    unified_consent::UnifiedConsentService* unifiedConsentService;
// Returns YES if the user is authenticated.
@property(nonatomic, assign, readonly) BOOL isAuthenticated;
// Sync setup service.
@property(nonatomic, assign, readonly) SyncSetupService* syncSetupService;
// Preference value for the autocomplete wallet feature.
@property(nonatomic, strong, readonly)
    PrefBackedBoolean* autocompleteWalletPreference;
// Preference value for the "Autocomplete searches and URLs" feature.
@property(nonatomic, strong, readonly)
    PrefBackedBoolean* autocompleteSearchPreference;
// Preference value for the "Preload pages for faster browsing" feature.
@property(nonatomic, strong, readonly)
    PrefBackedBoolean* preloadPagesPreference;
// Preference value for the "Preload pages for faster browsing" for Wifi-Only.
// TODO(crbug.com/872101): Needs to create the UI to change from Wifi-Only to
// always
@property(nonatomic, strong, readonly)
    PrefBackedBoolean* preloadPagesWifiOnlyPreference;
// Preference value for the "Help improve Chromium's features" feature.
@property(nonatomic, strong, readonly)
    PrefBackedBoolean* sendDataUsagePreference;
// Preference value for the "Help improve Chromium's features" for Wifi-Only.
// TODO(crbug.com/872101): Needs to create the UI to change from Wifi-Only to
// always
@property(nonatomic, strong, readonly)
    PrefBackedBoolean* sendDataUsageWifiOnlyPreference;
// Preference value for the "Make searches and browsing better" feature.
@property(nonatomic, strong, readonly)
    PrefBackedBoolean* anonymizedDataCollectionPreference;

// YES if at least one switch in the personalized section is currently animating
// from one state to another.
@property(nonatomic, assign) BOOL personalizedSectionBeingAnimated;
// All the items for the personalized section.
@property(nonatomic, strong, readonly) ItemArray personalizedItems;
// Item for the autocomplete wallet feature.
@property(nonatomic, strong, readonly) SyncSwitchItem* autocompleteWalletItem;
// All the items for the non-personalized section.
@property(nonatomic, strong, readonly) ItemArray nonPersonalizedItems;

@end

@implementation GoogleServicesSettingsMediator

@synthesize nonPersonalizedItems = _nonPersonalizedItems;

#pragma mark - Load model

- (instancetype)
initWithUserPrefService:(PrefService*)userPrefService
       localPrefService:(PrefService*)localPrefService
       syncSetupService:(SyncSetupService*)syncSetupService
  unifiedConsentService:
      (unified_consent::UnifiedConsentService*)unifiedConsentService {
  self = [super init];
  if (self) {
    DCHECK(userPrefService);
    DCHECK(localPrefService);
    DCHECK(syncSetupService);
    DCHECK(unifiedConsentService);
    _syncSetupService = syncSetupService;
    _unifiedConsentService = unifiedConsentService;
    _autocompleteWalletPreference = [[PrefBackedBoolean alloc]
        initWithPrefService:userPrefService
                   prefName:autofill::prefs::kAutofillWalletImportEnabled];
    _autocompleteWalletPreference.observer = self;
    _autocompleteSearchPreference = [[PrefBackedBoolean alloc]
        initWithPrefService:userPrefService
                   prefName:prefs::kSearchSuggestEnabled];
    _autocompleteSearchPreference.observer = self;
    _preloadPagesPreference = [[PrefBackedBoolean alloc]
        initWithPrefService:userPrefService
                   prefName:prefs::kNetworkPredictionEnabled];
    _preloadPagesPreference.observer = self;
    _preloadPagesWifiOnlyPreference = [[PrefBackedBoolean alloc]
        initWithPrefService:userPrefService
                   prefName:prefs::kNetworkPredictionWifiOnly];
    _sendDataUsagePreference = [[PrefBackedBoolean alloc]
        initWithPrefService:localPrefService
                   prefName:metrics::prefs::kMetricsReportingEnabled];
    _sendDataUsagePreference.observer = self;
    _sendDataUsageWifiOnlyPreference = [[PrefBackedBoolean alloc]
        initWithPrefService:localPrefService
                   prefName:prefs::kMetricsReportingWifiOnly];
    _anonymizedDataCollectionPreference = [[PrefBackedBoolean alloc]
        initWithPrefService:userPrefService
                   prefName:unified_consent::prefs::
                                kUrlKeyedAnonymizedDataCollectionEnabled];
    _anonymizedDataCollectionPreference.observer = self;
  }
  return self;
}

// Loads NonPersonalizedSectionIdentifier section.
- (void)loadNonPersonalizedSection {
  TableViewModel* model = self.consumer.tableViewModel;
  [model addSectionWithIdentifier:NonPersonalizedSectionIdentifier];
  for (TableViewItem* item in self.nonPersonalizedItems) {
    [model addItem:item
        toSectionWithIdentifier:NonPersonalizedSectionIdentifier];
  }
  [self updateNonPersonalizedSection];
}

#pragma mark - Properties

- (BOOL)isAuthenticated {
  return self.authService->IsAuthenticated();
}

- (ItemArray)nonPersonalizedItems {
  if (!_nonPersonalizedItems) {
    SyncSwitchItem* autocompleteSearchesAndURLsItem = [self
        switchItemWithItemType:AutocompleteSearchesAndURLsItemType
                  textStringID:
                      IDS_IOS_GOOGLE_SERVICES_SETTINGS_AUTOCOMPLETE_SEARCHES_AND_URLS_TEXT
                detailStringID:
                    IDS_IOS_GOOGLE_SERVICES_SETTINGS_AUTOCOMPLETE_SEARCHES_AND_URLS_DETAIL
                      dataType:0];
    SyncSwitchItem* preloadPagesItem =
        [self switchItemWithItemType:PreloadPagesItemType
                        textStringID:
                            IDS_IOS_GOOGLE_SERVICES_SETTINGS_PRELOAD_PAGES_TEXT
                      detailStringID:
                          IDS_IOS_GOOGLE_SERVICES_SETTINGS_PRELOAD_PAGES_DETAIL
                            dataType:0];
    SyncSwitchItem* improveChromeItem =
        [self switchItemWithItemType:ImproveChromeItemType
                        textStringID:
                            IDS_IOS_GOOGLE_SERVICES_SETTINGS_IMPROVE_CHROME_TEXT
                      detailStringID:
                          IDS_IOS_GOOGLE_SERVICES_SETTINGS_IMPROVE_CHROME_DETAIL
                            dataType:0];
    SyncSwitchItem* betterSearchAndBrowsingItemType = [self
        switchItemWithItemType:BetterSearchAndBrowsingItemType
                  textStringID:
                      IDS_IOS_GOOGLE_SERVICES_SETTINGS_BETTER_SEARCH_AND_BROWSING_TEXT
                detailStringID:
                    IDS_IOS_GOOGLE_SERVICES_SETTINGS_BETTER_SEARCH_AND_BROWSING_DETAIL
                      dataType:0];
    _nonPersonalizedItems = @[
      autocompleteSearchesAndURLsItem, preloadPagesItem, improveChromeItem,
      betterSearchAndBrowsingItemType
    ];
  }
  return _nonPersonalizedItems;
}

#pragma mark - Private

// Creates a SyncSwitchItem instance.
- (SyncSwitchItem*)switchItemWithItemType:(NSInteger)itemType
                             textStringID:(int)textStringID
                           detailStringID:(int)detailStringID
                                 dataType:(NSInteger)dataType {
  SyncSwitchItem* switchItem = [[SyncSwitchItem alloc] initWithType:itemType];
  switchItem.text = GetNSString(textStringID);
  if (detailStringID)
    switchItem.detailText = GetNSString(detailStringID);
  switchItem.dataType = dataType;
  return switchItem;
}

// Creates a item to display the sync error.
- (SettingsImageDetailTextItem*)createSyncErrorItemWithItemType:
    (NSInteger)itemType {
  SettingsImageDetailTextItem* syncErrorItem =
      [[SettingsImageDetailTextItem alloc] initWithType:itemType];
  syncErrorItem.text = l10n_util::GetNSString(IDS_IOS_SYNC_ERROR_TITLE);
  syncErrorItem.detailText =
      GetSyncErrorDescriptionForSyncSetupService(self.syncSetupService);
  {
    // TODO(crbug.com/889470): Needs asset for the sync error.
    CGSize size = CGSizeMake(40, 40);
    UIGraphicsBeginImageContextWithOptions(size, YES, 0);
    [[UIColor grayColor] setFill];
    UIRectFill(CGRectMake(0, 0, size.width, size.height));
    syncErrorItem.image = UIGraphicsGetImageFromCurrentImageContext();
    UIGraphicsEndImageContext();
  }
  return syncErrorItem;
}

// Reloads the sync feedback section. If |notifyConsummer| is YES, the consomer
// is notified to add or remove the sync error section.
- (void)updateSyncErrorSectionAndNotifyConsumer:(BOOL)notifyConsummer {
  TableViewModel* model = self.consumer.tableViewModel;
  BOOL hasError = NO;
  ItemType type;
  if (self.isAuthenticated) {
    switch (self.syncSetupService->GetSyncServiceState()) {
      case SyncSetupService::kSyncServiceUnrecoverableError:
        type = RestartAuthenticationFlowErrorItemType;
        hasError = YES;
        break;
      case SyncSetupService::kSyncServiceSignInNeedsUpdate:
        type = ReauthDialogAsSyncIsInAuthErrorItemType;
        hasError = YES;
        break;
      case SyncSetupService::kSyncServiceNeedsPassphrase:
        type = ShowPassphraseDialogErrorItemType;
        hasError = YES;
        break;
      case SyncSetupService::kNoSyncServiceError:
      case SyncSetupService::kSyncServiceCouldNotConnect:
      case SyncSetupService::kSyncServiceServiceUnavailable:
        break;
    }
  }
  if (!hasError) {
    // No action to do, therefore the sync error section should not be visibled.
    if ([model hasSectionForSectionIdentifier:SyncFeedbackSectionIdentifier]) {
      // Remove the sync error item if it exists.
      NSUInteger sectionIndex =
          [model sectionForSectionIdentifier:SyncFeedbackSectionIdentifier];
      [model removeSectionWithIdentifier:SyncFeedbackSectionIdentifier];
      if (notifyConsummer) {
        NSIndexSet* indexSet = [NSIndexSet indexSetWithIndex:sectionIndex];
        [self.consumer deleteSections:indexSet];
      }
    }
    return;
  }
  // Add the sync error item and its section (if it doesn't already exist) and
  // reload them.
  BOOL sectionAdded = NO;
  SettingsImageDetailTextItem* syncErrorItem =
      [self createSyncErrorItemWithItemType:type];
  if (![model hasSectionForSectionIdentifier:SyncFeedbackSectionIdentifier]) {
    // Adding the sync error item and its section.
    [model insertSectionWithIdentifier:SyncFeedbackSectionIdentifier atIndex:0];
    [model addItem:syncErrorItem
        toSectionWithIdentifier:SyncFeedbackSectionIdentifier];
    sectionAdded = YES;
  } else {
    [model
        deleteAllItemsFromSectionWithIdentifier:SyncFeedbackSectionIdentifier];
    [model addItem:syncErrorItem
        toSectionWithIdentifier:SyncFeedbackSectionIdentifier];
  }
  if (notifyConsummer) {
    NSUInteger sectionIndex =
        [model sectionForSectionIdentifier:SyncFeedbackSectionIdentifier];
    NSIndexSet* indexSet = [NSIndexSet indexSetWithIndex:sectionIndex];
    if (sectionAdded) {
      [self.consumer insertSections:indexSet];
    } else {
      [self.consumer reloadSections:indexSet];
    }
  }
}

// Updates the non-personalized section according to the user consent.
- (void)updateNonPersonalizedSection {
  for (TableViewItem* item in self.nonPersonalizedItems) {
    ItemType type = static_cast<ItemType>(item.type);
    SyncSwitchItem* switchItem =
        base::mac::ObjCCastStrict<SyncSwitchItem>(item);
    switch (type) {
      case AutocompleteSearchesAndURLsItemType:
        switchItem.on = self.autocompleteSearchPreference.value;
        break;
      case PreloadPagesItemType:
        switchItem.on = self.preloadPagesPreference.value;
        break;
      case ImproveChromeItemType:
        switchItem.on = self.sendDataUsagePreference.value;
        break;
      case BetterSearchAndBrowsingItemType:
        switchItem.on = self.anonymizedDataCollectionPreference.value;
        break;
      case RestartAuthenticationFlowErrorItemType:
      case ReauthDialogAsSyncIsInAuthErrorItemType:
      case ShowPassphraseDialogErrorItemType:
        NOTREACHED();
        break;
    }
  }
}

#pragma mark - GoogleServicesSettingsViewControllerModelDelegate

- (void)googleServicesSettingsViewControllerLoadModel:
    (GoogleServicesSettingsViewController*)controller {
  DCHECK_EQ(self.consumer, controller);
  [self loadNonPersonalizedSection];
  [self updateSyncErrorSectionAndNotifyConsumer:NO];
}

#pragma mark - GoogleServicesSettingsServiceDelegate

- (void)toggleSwitchItem:(SyncSwitchItem*)switchItem withValue:(BOOL)value {
  ItemType type = static_cast<ItemType>(switchItem.type);
  switch (type) {
    case AutocompleteSearchesAndURLsItemType:
      self.autocompleteSearchPreference.value = value;
      break;
    case PreloadPagesItemType:
      self.preloadPagesPreference.value = value;
      if (value) {
        // Should be wifi only, until https://crbug.com/872101 is fixed.
        self.preloadPagesWifiOnlyPreference.value = YES;
      }
      break;
    case ImproveChromeItemType:
      self.sendDataUsagePreference.value = value;
      if (value) {
        // Should be wifi only, until https://crbug.com/872101 is fixed.
        self.sendDataUsageWifiOnlyPreference.value = YES;
      }
      break;
    case BetterSearchAndBrowsingItemType:
      self.anonymizedDataCollectionPreference.value = value;
      break;
    case RestartAuthenticationFlowErrorItemType:
    case ReauthDialogAsSyncIsInAuthErrorItemType:
    case ShowPassphraseDialogErrorItemType:
      NOTREACHED();
      break;
  }
}

- (void)didSelectItem:(TableViewItem*)item {
  ItemType type = static_cast<ItemType>(item.type);
  switch (type) {
    case RestartAuthenticationFlowErrorItemType:
      [self.commandHandler restartAuthenticationFlow];
      break;
    case ReauthDialogAsSyncIsInAuthErrorItemType:
      [self.commandHandler openReauthDialogAsSyncIsInAuthError];
      break;
    case ShowPassphraseDialogErrorItemType:
      [self.commandHandler openPassphraseDialog];
      break;
    case AutocompleteSearchesAndURLsItemType:
    case PreloadPagesItemType:
    case ImproveChromeItemType:
    case BetterSearchAndBrowsingItemType:
      break;
  }
}

#pragma mark - BooleanObserver

- (void)booleanDidChange:(id<ObservableBoolean>)observableBoolean {
  [self updateNonPersonalizedSection];
  TableViewModel* model = self.consumer.tableViewModel;
  NSUInteger index =
      [model sectionForSectionIdentifier:NonPersonalizedSectionIdentifier];
  NSIndexSet* sectionIndexToReload = [NSIndexSet indexSetWithIndex:index];
  [self.consumer reloadSections:sectionIndexToReload];
}

@end
