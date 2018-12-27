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
#import "ios/chrome/browser/ui/settings/cells/legacy/legacy_settings_image_detail_text_item.h"
#import "ios/chrome/browser/ui/settings/cells/legacy/legacy_sync_switch_item.h"
#import "ios/chrome/browser/ui/settings/sync_utils/sync_util.h"
#import "ios/chrome/browser/ui/settings/utils/observable_boolean.h"
#import "ios/chrome/browser/ui/settings/utils/pref_backed_boolean.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/third_party/material_components_ios/src/components/Palettes/src/MaterialPalettes.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using l10n_util::GetNSString;

typedef NSArray<CollectionViewItem*>* ItemArray;

namespace {

// List of sections.
typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SyncFeedbackSectionIdentifier = kSectionIdentifierEnumZero,
  NonPersonalizedSectionIdentifier,
};

// List of items.
typedef NS_ENUM(NSInteger, ItemType) {
  // SyncErrorSectionIdentifier,
  SyncErrorItemType = kItemTypeEnumZero,
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
// Item to display the sync error.
@property(nonatomic, strong) LegacySettingsImageDetailTextItem* syncErrorItem;
// All the items for the personalized section.
@property(nonatomic, strong, readonly) ItemArray personalizedItems;
// Item for the autocomplete wallet feature.
@property(nonatomic, strong, readonly)
    LegacySyncSwitchItem* autocompleteWalletItem;
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
  CollectionViewModel* model = self.consumer.collectionViewModel;
  [model addSectionWithIdentifier:NonPersonalizedSectionIdentifier];
  for (CollectionViewItem* item in self.nonPersonalizedItems) {
    [model addItem:item
        toSectionWithIdentifier:NonPersonalizedSectionIdentifier];
  }
  [self updateNonPersonalizedSection];
}

#pragma mark - Properties

- (BOOL)isAuthenticated {
  return self.authService->IsAuthenticated();
}

- (LegacySettingsImageDetailTextItem*)syncErrorItem {
  if (!_syncErrorItem) {
    _syncErrorItem = [[LegacySettingsImageDetailTextItem alloc]
        initWithType:SyncErrorItemType];
    {
      // TODO(crbug.com/889470): Needs asset for the sync error.
      CGSize size = CGSizeMake(40, 40);
      UIGraphicsBeginImageContextWithOptions(size, YES, 0);
      [[UIColor grayColor] setFill];
      UIRectFill(CGRectMake(0, 0, size.width, size.height));
      _syncErrorItem.image = UIGraphicsGetImageFromCurrentImageContext();
      UIGraphicsEndImageContext();
    }
  }
  return _syncErrorItem;
}

- (ItemArray)nonPersonalizedItems {
  if (!_nonPersonalizedItems) {
    LegacySyncSwitchItem* autocompleteSearchesAndURLsItem = [self
        switchItemWithItemType:AutocompleteSearchesAndURLsItemType
                  textStringID:
                      IDS_IOS_GOOGLE_SERVICES_SETTINGS_AUTOCOMPLETE_SEARCHES_AND_URLS_TEXT
                detailStringID:
                    IDS_IOS_GOOGLE_SERVICES_SETTINGS_AUTOCOMPLETE_SEARCHES_AND_URLS_DETAIL
                     commandID:
                         GoogleServicesSettingsCommandIDToggleAutocompleteSearchesService
                      dataType:0];
    LegacySyncSwitchItem* preloadPagesItem = [self
        switchItemWithItemType:PreloadPagesItemType
                  textStringID:
                      IDS_IOS_GOOGLE_SERVICES_SETTINGS_PRELOAD_PAGES_TEXT
                detailStringID:
                    IDS_IOS_GOOGLE_SERVICES_SETTINGS_PRELOAD_PAGES_DETAIL
                     commandID:
                         GoogleServicesSettingsCommandIDTogglePreloadPagesService
                      dataType:0];
    LegacySyncSwitchItem* improveChromeItem = [self
        switchItemWithItemType:ImproveChromeItemType
                  textStringID:
                      IDS_IOS_GOOGLE_SERVICES_SETTINGS_IMPROVE_CHROME_TEXT
                detailStringID:
                    IDS_IOS_GOOGLE_SERVICES_SETTINGS_IMPROVE_CHROME_DETAIL
                     commandID:
                         GoogleServicesSettingsCommandIDToggleImproveChromeService
                      dataType:0];
    LegacySyncSwitchItem* betterSearchAndBrowsingItemType = [self
        switchItemWithItemType:BetterSearchAndBrowsingItemType
                  textStringID:
                      IDS_IOS_GOOGLE_SERVICES_SETTINGS_BETTER_SEARCH_AND_BROWSING_TEXT
                detailStringID:
                    IDS_IOS_GOOGLE_SERVICES_SETTINGS_BETTER_SEARCH_AND_BROWSING_DETAIL
                     commandID:
                         GoogleServicesSettingsCommandIDToggleBetterSearchAndBrowsingService
                      dataType:0];
    _nonPersonalizedItems = @[
      autocompleteSearchesAndURLsItem, preloadPagesItem, improveChromeItem,
      betterSearchAndBrowsingItemType
    ];
  }
  return _nonPersonalizedItems;
}

#pragma mark - Private

// Creates a LegacySyncSwitchItem instance.
- (LegacySyncSwitchItem*)switchItemWithItemType:(NSInteger)itemType
                                   textStringID:(int)textStringID
                                 detailStringID:(int)detailStringID
                                      commandID:(NSInteger)commandID
                                       dataType:(NSInteger)dataType {
  LegacySyncSwitchItem* switchItem =
      [[LegacySyncSwitchItem alloc] initWithType:itemType];
  switchItem.text = GetNSString(textStringID);
  if (detailStringID)
    switchItem.detailText = GetNSString(detailStringID);
  switchItem.commandID = commandID;
  switchItem.dataType = dataType;
  return switchItem;
}

// Reloads the sync feedback section. If |notifyConsummer| is YES, the consomer
// is notified to add or remove the sync error section.
- (void)updateSyncErrorSectionAndNotifyConsumer:(BOOL)notifyConsummer {
  CollectionViewModel* model = self.consumer.collectionViewModel;
  GoogleServicesSettingsCommandID commandID =
      GoogleServicesSettingsCommandIDNoOp;
  if (self.isAuthenticated) {
    switch (self.syncSetupService->GetSyncServiceState()) {
      case SyncSetupService::kSyncServiceUnrecoverableError:
        commandID = GoogleServicesSettingsCommandIDRestartAuthenticationFlow;
        break;
      case SyncSetupService::kSyncServiceSignInNeedsUpdate:
        commandID = GoogleServicesSettingsReauthDialogAsSyncIsInAuthError;
        break;
      case SyncSetupService::kSyncServiceNeedsPassphrase:
        commandID = GoogleServicesSettingsCommandIDShowPassphraseDialog;
        break;
      case SyncSetupService::kNoSyncServiceError:
      case SyncSetupService::kSyncServiceCouldNotConnect:
      case SyncSetupService::kSyncServiceServiceUnavailable:
        break;
    }
  }
  if (commandID == GoogleServicesSettingsCommandIDNoOp) {
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
  if (![model hasSectionForSectionIdentifier:SyncFeedbackSectionIdentifier]) {
    // Adding the sync error item and its section.
    [model insertSectionWithIdentifier:SyncFeedbackSectionIdentifier atIndex:0];
    [model addItem:self.syncErrorItem
        toSectionWithIdentifier:SyncFeedbackSectionIdentifier];
    sectionAdded = YES;
  }
  NSUInteger sectionIndex =
      [model sectionForSectionIdentifier:SyncFeedbackSectionIdentifier];
  self.syncErrorItem.text = l10n_util::GetNSString(IDS_IOS_SYNC_ERROR_TITLE);
  self.syncErrorItem.detailText =
      GetSyncErrorDescriptionForSyncSetupService(self.syncSetupService);
  self.syncErrorItem.commandID = commandID;
  if (notifyConsummer) {
    if (sectionAdded) {
      NSIndexSet* indexSet = [NSIndexSet indexSetWithIndex:sectionIndex];
      [self.consumer insertSections:indexSet];
    } else {
      [self.consumer reloadItem:self.syncErrorItem];
    }
  }
}

// Updates the non-personalized section according to the user consent.
- (void)updateNonPersonalizedSection {
  for (CollectionViewItem* item in self.nonPersonalizedItems) {
    if ([item isKindOfClass:[LegacySyncSwitchItem class]]) {
      LegacySyncSwitchItem* switchItem =
          base::mac::ObjCCast<LegacySyncSwitchItem>(item);
      switch (switchItem.commandID) {
        case GoogleServicesSettingsCommandIDToggleAutocompleteSearchesService:
          switchItem.on = self.autocompleteSearchPreference.value;
          break;
        case GoogleServicesSettingsCommandIDTogglePreloadPagesService:
          switchItem.on = self.preloadPagesPreference.value;
          break;
        case GoogleServicesSettingsCommandIDToggleImproveChromeService:
          switchItem.on = self.sendDataUsagePreference.value;
          break;
        case GoogleServicesSettingsCommandIDToggleBetterSearchAndBrowsingService:
          switchItem.on = self.anonymizedDataCollectionPreference.value;
          break;
      }
    } else {
      NOTREACHED();
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

- (void)toggleAutocompleteWalletServiceWithValue:(BOOL)value {
  self.autocompleteWalletPreference.value = value;
}

- (void)toggleAutocompleteSearchesServiceWithValue:(BOOL)value {
  self.autocompleteSearchPreference.value = value;
}

- (void)togglePreloadPagesServiceWithValue:(BOOL)value {
  self.preloadPagesPreference.value = value;
  if (value) {
    // Should be wifi only, until https://crbug.com/872101 is fixed.
    self.preloadPagesWifiOnlyPreference.value = YES;
  }
}

- (void)toggleImproveChromeServiceWithValue:(BOOL)value {
  self.sendDataUsagePreference.value = value;
  if (value) {
    // Should be wifi only, until https://crbug.com/872101 is fixed.
    self.sendDataUsageWifiOnlyPreference.value = YES;
  }
}

- (void)toggleBetterSearchAndBrowsingServiceWithValue:(BOOL)value {
  self.anonymizedDataCollectionPreference.value = value;
}

#pragma mark - BooleanObserver

- (void)booleanDidChange:(id<ObservableBoolean>)observableBoolean {
  [self updateNonPersonalizedSection];
  CollectionViewModel* model = self.consumer.collectionViewModel;
  NSUInteger index =
      [model sectionForSectionIdentifier:NonPersonalizedSectionIdentifier];
  NSIndexSet* sectionIndexToReload = [NSIndexSet indexSetWithIndex:index];
  [self.consumer reloadSections:sectionIndexToReload];
}

@end
