// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/google_services/google_services_settings_mediator.h"

#include "base/auto_reset.h"
#include "base/mac/foundation_util.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/sync/driver/sync_service.h"
#include "components/unified_consent/pref_names.h"
#include "ios/chrome/browser/pref_names.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#include "ios/chrome/browser/signin/chrome_identity_service_observer_bridge.h"
#include "ios/chrome/browser/sync/sync_observer_bridge.h"
#import "ios/chrome/browser/ui/authentication/cells/table_view_account_item.h"
#import "ios/chrome/browser/ui/authentication/resized_avatar_cache.h"
#import "ios/chrome/browser/ui/settings/cells/account_sign_in_item.h"
#import "ios/chrome/browser/ui/settings/cells/settings_image_detail_text_item.h"
#import "ios/chrome/browser/ui/settings/cells/settings_multiline_detail_item.h"
#import "ios/chrome/browser/ui/settings/cells/sync_switch_item.h"
#import "ios/chrome/browser/ui/settings/google_services/google_services_settings_command_handler.h"
#import "ios/chrome/browser/ui/settings/sync/utils/sync_util.h"
#import "ios/chrome/browser/ui/settings/utils/observable_boolean.h"
#import "ios/chrome/browser/ui/settings/utils/pref_backed_boolean.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity.h"
#import "services/identity/public/objc/identity_manager_observer_bridge.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using l10n_util::GetNSString;

typedef NSArray<TableViewItem*>* ItemArray;

namespace {

// List of sections.
typedef NS_ENUM(NSInteger, SectionIdentifier) {
  IdentitySectionIdentifier = kSectionIdentifierEnumZero,
  SyncSectionIdentifier,
  NonPersonalizedSectionIdentifier,
};

// List of items.
typedef NS_ENUM(NSInteger, ItemType) {
  // IdentitySectionIdentifier section.
  IdentityItemType = kItemTypeEnumZero,
  // SyncSectionIdentifier section.
  SignInItemType,
  RestartAuthenticationFlowErrorItemType,
  ReauthDialogAsSyncIsInAuthErrorItemType,
  ShowPassphraseDialogErrorItemType,
  SyncChromeDataItemType,
  ManageSyncItemType,
  // NonPersonalizedSectionIdentifier section.
  AutocompleteSearchesAndURLsItemType,
  PreloadPagesItemType,
  ImproveChromeItemType,
  BetterSearchAndBrowsingItemType,
};

}  // namespace

@interface GoogleServicesSettingsMediator () <
    BooleanObserver,
    ChromeIdentityServiceObserver,
    IdentityManagerObserverBridgeDelegate,
    SyncObserverModelBridge> {
  // Sync observer.
  std::unique_ptr<SyncObserverBridge> _syncObserver;
  // Identity manager observer.
  std::unique_ptr<identity::IdentityManagerObserverBridge>
      _identityManagerObserverBridge;
  // Chrome identity observer.
  std::unique_ptr<ChromeIdentityServiceObserverBridge> _identityServiceObserver;
}

// Returns YES if the user is authenticated.
@property(nonatomic, assign, readonly) BOOL isAuthenticated;
// Sync setup service.
@property(nonatomic, assign, readonly) SyncSetupService* syncSetupService;
// ** Identity section.
// Avatar cache.
@property(nonatomic, strong) ResizedAvatarCache* resizedAvatarCache;
// Account item.
@property(nonatomic, strong) TableViewAccountItem* accountItem;
// ** Sync section.
// YES if the impression of the Signin cell has already been recorded.
@property(nonatomic, assign) BOOL hasRecordedSigninImpression;
// Sync error item (in the sync section).
@property(nonatomic, strong) TableViewItem* syncErrorItem;
// Sync your Chrome data switch item.
@property(nonatomic, strong) SyncSwitchItem* syncChromeDataSwitchItem;
// ** Non personalized section.
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
// All the items for the non-personalized section.
@property(nonatomic, strong, readonly) ItemArray nonPersonalizedItems;

@end

@implementation GoogleServicesSettingsMediator

@synthesize nonPersonalizedItems = _nonPersonalizedItems;

- (instancetype)initWithUserPrefService:(PrefService*)userPrefService
                       localPrefService:(PrefService*)localPrefService
                       syncSetupService:(SyncSetupService*)syncSetupService {
  self = [super init];
  if (self) {
    DCHECK(userPrefService);
    DCHECK(localPrefService);
    DCHECK(syncSetupService);
    _syncSetupService = syncSetupService;
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
    _resizedAvatarCache = [[ResizedAvatarCache alloc] init];
  }
  return self;
}

#pragma mark - Loads identity section

// Loads the identity section.
- (void)loadIdentitySection {
  self.accountItem = nil;
  if (!self.isAuthenticated)
    return;
  [self createIdentitySection];
  [self configureIdentityAccountItem];
}

// Creates the identity sections.
- (void)createIdentitySection {
  TableViewModel* model = self.consumer.tableViewModel;
  [model insertSectionWithIdentifier:IdentitySectionIdentifier atIndex:0];
  DCHECK(!self.accountItem);
  self.accountItem =
      [[TableViewAccountItem alloc] initWithType:IdentityItemType];
  [model addItem:self.accountItem
      toSectionWithIdentifier:IdentitySectionIdentifier];
}

// Creates, removes or updates the identity section as needed. And notifies the
// consumer.
- (void)updateIdentitySectionAndNotifyConsumer {
  TableViewModel* model = self.consumer.tableViewModel;
  BOOL hasIdentitySection =
      [model hasSectionForSectionIdentifier:IdentitySectionIdentifier];
  if (!self.isAuthenticated) {
    if (!hasIdentitySection) {
      DCHECK(!self.accountItem);
      return;
    }
    self.accountItem = nil;
    NSInteger sectionIndex =
        [model sectionForSectionIdentifier:IdentitySectionIdentifier];
    [model removeSectionWithIdentifier:IdentitySectionIdentifier];
    NSIndexSet* indexSet = [NSIndexSet indexSetWithIndex:sectionIndex];
    [self.consumer deleteSections:indexSet];
    return;
  }
  if (!hasIdentitySection) {
    [self createIdentitySection];
    NSInteger sectionIndex =
        [model sectionForSectionIdentifier:IdentitySectionIdentifier];
    NSIndexSet* indexSet = [NSIndexSet indexSetWithIndex:sectionIndex];
    [self.consumer insertSections:indexSet];
  }
  [self configureIdentityAccountItem];
  [self.consumer reloadItem:self.accountItem];
}

// Configures the identity account item.
- (void)configureIdentityAccountItem {
  DCHECK(self.accountItem);
  ChromeIdentity* identity = self.authService->GetAuthenticatedIdentity();
  DCHECK(identity);
  self.accountItem.image =
      [self.resizedAvatarCache resizedAvatarForIdentity:identity];
  self.accountItem.text = identity.userFullName;
  if (self.syncSetupService->HasFinishedInitialSetup()) {
    self.accountItem.detailText = identity.userEmail;
  } else {
    self.accountItem.detailText = GetNSString(IDS_IOS_SYNC_SETUP_IN_PROGRESS);
  }
}

#pragma mark - Loads sync section

// Loads the sync section.
- (void)loadSyncSection {
  self.syncErrorItem = nil;
  self.syncChromeDataSwitchItem = nil;
  TableViewModel* model = self.consumer.tableViewModel;
  [model addSectionWithIdentifier:SyncSectionIdentifier];
  [self updateSyncSection:NO];
}

// Updates the sync section. If |notifyConsumer| is YES, the consumer is
// notified about model changes.
- (void)updateSyncSection:(BOOL)notifyConsumer {
  BOOL needsAccountSigninItemUpdate = [self updateAccountSignInItem];
  BOOL needsSyncErrorItemsUpdate = [self updateSyncErrorItems];
  BOOL needsSyncChromeDataItemUpdate = [self updateSyncChromeDataItem];
  BOOL needsManageSyncItemUpdate = [self updateManageSyncItem];
  if (notifyConsumer &&
      (needsAccountSigninItemUpdate || needsSyncErrorItemsUpdate ||
       needsSyncChromeDataItemUpdate || needsManageSyncItemUpdate)) {
    TableViewModel* model = self.consumer.tableViewModel;
    NSUInteger sectionIndex =
        [model sectionForSectionIdentifier:SyncSectionIdentifier];
    NSIndexSet* indexSet = [NSIndexSet indexSetWithIndex:sectionIndex];
    [self.consumer reloadSections:indexSet];
  }
}

// Adds, removes and updates the account sign-in item in the model as needed.
// Returns YES if the consumer should be notified.
- (BOOL)updateAccountSignInItem {
  TableViewModel* model = self.consumer.tableViewModel;
  BOOL hasAccountSignInItem = [model hasItemForItemType:SignInItemType
                                      sectionIdentifier:SyncSectionIdentifier];

  if (self.isAuthenticated) {
    self.hasRecordedSigninImpression = NO;
    if (!hasAccountSignInItem)
      return NO;
    [model removeItemWithType:SignInItemType
        fromSectionWithIdentifier:SyncSectionIdentifier];
    return YES;
  }

  if (hasAccountSignInItem)
    return NO;
  AccountSignInItem* accountSignInItem =
      [[AccountSignInItem alloc] initWithType:SignInItemType];
  accountSignInItem.detailText =
      GetNSString(IDS_IOS_GOOGLE_SERVICES_SETTINGS_SIGN_IN_DETAIL_TEXT);
  [model addItem:accountSignInItem
      toSectionWithIdentifier:SyncSectionIdentifier];

  if (!self.hasRecordedSigninImpression) {
    // Once the Settings are open, this button impression will at most be
    // recorded once per dialog displayed and per sign-in.
    signin_metrics::RecordSigninImpressionUserActionForAccessPoint(
        signin_metrics::AccessPoint::ACCESS_POINT_GOOGLE_SERVICES_SETTINGS);
    self.hasRecordedSigninImpression = YES;
  }
  return YES;
}

// Adds, removes and updates the sync error item in the model as needed. Returns
// YES if the consumer should be notified.
- (BOOL)updateSyncErrorItems {
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

  if ((!hasError && !self.syncErrorItem) ||
      (hasError && self.syncErrorItem && type == self.syncErrorItem.type)) {
    // Nothing to update.
    return NO;
  }

  if (self.syncErrorItem) {
    // Remove the previous sync error item, since it is either the wrong error
    // (if hasError is YES), or there is no error anymore.
    [model removeItemWithType:self.syncErrorItem.type
        fromSectionWithIdentifier:SyncSectionIdentifier];
    self.syncErrorItem = nil;
    if (!hasError)
      return YES;
  }
  // Add the sync error item and its section.
  self.syncErrorItem = [self createSyncErrorItemWithItemType:type];
  [model insertItem:self.syncErrorItem
      inSectionWithIdentifier:SyncSectionIdentifier
                      atIndex:0];
  return YES;
}

// Reloads the manage sync item, and returns YES if the section should be
// reloaded.
- (BOOL)updateManageSyncItem {
  TableViewModel* model = self.consumer.tableViewModel;
  BOOL hasManageSyncItem = [model hasItemForItemType:ManageSyncItemType
                                   sectionIdentifier:SyncSectionIdentifier];
  if (self.isAuthenticated) {
    if (hasManageSyncItem)
      return NO;
    SettingsMultilineDetailItem* item =
        [[SettingsMultilineDetailItem alloc] initWithType:ManageSyncItemType];
    item.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
    item.text = GetNSString(IDS_IOS_MANAGE_SYNC_SETTINGS_TITLE);
    [model addItem:item toSectionWithIdentifier:SyncSectionIdentifier];
    return YES;
  }
  if (!hasManageSyncItem)
    return NO;
  [model removeItemWithType:ManageSyncItemType
      fromSectionWithIdentifier:SyncSectionIdentifier];
  return YES;
}

- (BOOL)updateSyncChromeDataItem {
  TableViewModel* model = self.consumer.tableViewModel;
  if (self.isAuthenticated) {
    if (self.syncChromeDataSwitchItem) {
      BOOL needsUpdate = self.syncChromeDataSwitchItem.on !=
                         self.syncSetupService->IsSyncingAllDataTypes();
      self.syncChromeDataSwitchItem.on =
          self.syncSetupService->IsSyncingAllDataTypes();
      return needsUpdate;
    }
    self.syncChromeDataSwitchItem = [self
        switchItemWithItemType:SyncChromeDataItemType
                  textStringID:IDS_IOS_GOOGLE_SERVICES_SETTINGS_SYNC_CHROME_DATA
                detailStringID:0
                      dataType:0];
    self.syncChromeDataSwitchItem.on = self.syncSetupService->IsSyncEnabled();
    [model addItem:self.syncChromeDataSwitchItem
        toSectionWithIdentifier:SyncSectionIdentifier];
    return YES;
  }
  if (!self.syncChromeDataSwitchItem)
    return NO;
  self.syncChromeDataSwitchItem = nil;
  [model removeItemWithType:SyncChromeDataItemType
      fromSectionWithIdentifier:SyncSectionIdentifier];
  return YES;
}

#pragma mark - Load non personalized section

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
      case IdentityItemType:
      case SignInItemType:
      case RestartAuthenticationFlowErrorItemType:
      case ReauthDialogAsSyncIsInAuthErrorItemType:
      case ShowPassphraseDialogErrorItemType:
      case SyncChromeDataItemType:
      case ManageSyncItemType:
        NOTREACHED();
        break;
    }
  }
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
  syncErrorItem.text = GetNSString(IDS_IOS_SYNC_ERROR_TITLE);
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

#pragma mark - GoogleServicesSettingsViewControllerModelDelegate

- (void)googleServicesSettingsViewControllerLoadModel:
    (GoogleServicesSettingsViewController*)controller {
  DCHECK_EQ(self.consumer, controller);
  [self loadIdentitySection];
  [self loadSyncSection];
  [self loadNonPersonalizedSection];
  _identityManagerObserverBridge.reset(
      new identity::IdentityManagerObserverBridge(self.identityManager, self));
  DCHECK(self.syncService);
  _syncObserver.reset(new SyncObserverBridge(self, self.syncService));
  _identityServiceObserver.reset(new ChromeIdentityServiceObserverBridge(self));
}

#pragma mark - GoogleServicesSettingsServiceDelegate

- (void)toggleSwitchItem:(SyncSwitchItem*)switchItem withValue:(BOOL)value {
  ItemType type = static_cast<ItemType>(switchItem.type);
  switchItem.on = value;
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
    case SyncChromeDataItemType:
      self.syncSetupService->SetSyncEnabled(value);
      break;
    case IdentityItemType:
    case SignInItemType:
    case RestartAuthenticationFlowErrorItemType:
    case ReauthDialogAsSyncIsInAuthErrorItemType:
    case ShowPassphraseDialogErrorItemType:
    case ManageSyncItemType:
      NOTREACHED();
      break;
  }
}

- (void)didSelectItem:(TableViewItem*)item {
  ItemType type = static_cast<ItemType>(item.type);
  switch (type) {
    case IdentityItemType:
      [self.commandHandler openAccountSettings];
      break;
    case SignInItemType:
      [self.commandHandler showSignIn];
      break;
    case RestartAuthenticationFlowErrorItemType:
      [self.commandHandler restartAuthenticationFlow];
      break;
    case ReauthDialogAsSyncIsInAuthErrorItemType:
      [self.commandHandler openReauthDialogAsSyncIsInAuthError];
      break;
    case ShowPassphraseDialogErrorItemType:
      [self.commandHandler openPassphraseDialog];
      break;
    case ManageSyncItemType:
      [self.commandHandler openManageSyncSettings];
      break;
    case AutocompleteSearchesAndURLsItemType:
    case PreloadPagesItemType:
    case ImproveChromeItemType:
    case BetterSearchAndBrowsingItemType:
    case SyncChromeDataItemType:
      break;
  }
}

#pragma mark - SyncObserverModelBridge

- (void)onSyncStateChanged {
  [self updateSyncSection:YES];
  if (self.accountItem) {
    [self configureIdentityAccountItem];
    [self.consumer reloadItem:self.accountItem];
  }
}
#pragma mark - IdentityManagerObserverBridgeDelegate

- (void)onPrimaryAccountSet:(const AccountInfo&)primaryAccountInfo {
  [self updateSyncSection:YES];
  [self updateIdentitySectionAndNotifyConsumer];
}

- (void)onPrimaryAccountCleared:(const AccountInfo&)previousPrimaryAccountInfo {
  [self updateSyncSection:YES];
  [self updateIdentitySectionAndNotifyConsumer];
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

#pragma mark - ChromeIdentityServiceObserver

- (void)profileUpdate:(ChromeIdentity*)identity {
  [self updateIdentitySectionAndNotifyConsumer];
}

- (void)chromeIdentityServiceWillBeDestroyed {
  _identityServiceObserver.reset();
}

@end
