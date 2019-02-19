// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_mediator.h"

#include "base/auto_reset.h"
#include "base/logging.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/sync/sync_observer_bridge.h"
#include "ios/chrome/browser/sync/sync_setup_service.h"
#import "ios/chrome/browser/ui/list_model/list_model.h"
#import "ios/chrome/browser/ui/settings/cells/sync_switch_item.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using l10n_util::GetNSString;

namespace {

// List of sections.
typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SyncDataTypeSectionIdentifier = kSectionIdentifierEnumZero,
};

// List of items. For implementation details in
// ManageSyncSettingsTableViewController, two SyncSwitchItem items should not
// share the same type. The cell UISwitch tag is used to save the item type, and
// when the user taps on the switch, this tag is used to retreive the item based
// on the type.
typedef NS_ENUM(NSInteger, ItemType) {
  // SyncDataTypeSectionIdentifier section.
  SyncEverythingItemType = kItemTypeEnumZero,
  // kSyncAutofill.
  AutofillDataTypeItemType,
  // kSyncBookmarks.
  BookmarksDataTypeItemType,
  // kSyncOmniboxHistory.
  HistoryDataTypeItemType,
  // kSyncOpenTabs.
  OpenTabsDataTypeItemType,
  // kSyncPasswords
  PasswordsDataTypeItemType,
  // kSyncReadingList.
  ReadingListDataTypeItemType,
  // kSyncPreferences.
  SettingsDataTypeItemType,
};

}  // namespace

@interface ManageSyncSettingsMediator () <SyncObserverModelBridge> {
  // Sync observer.
  std::unique_ptr<SyncObserverBridge> _syncObserver;
  // Whether Sync State changes should be currently ignored.
  BOOL _ignoreSyncStateChanges;
}

// Model item for sync everything.
@property(nonatomic, strong) SyncSwitchItem* syncEverythingItem;
// Model item for each data types.
@property(nonatomic, strong) NSArray<SyncSwitchItem*>* syncSwitchItems;
// Returns whether the Sync settings should be disabled because of a Sync error.
@property(nonatomic, assign, readonly) BOOL disabledBecauseOfSyncError;

@end

@implementation ManageSyncSettingsMediator

- (instancetype)initWithSyncService:(syncer::SyncService*)syncService {
  self = [super init];
  if (self) {
    DCHECK(syncService);
    _syncObserver.reset(new SyncObserverBridge(self, syncService));
  }
  return self;
}

#pragma mark - Loads sync data type section

// Loads the sync data type section.
- (void)loadSyncDataTypeSection {
  TableViewModel* model = self.consumer.tableViewModel;
  [model addSectionWithIdentifier:SyncDataTypeSectionIdentifier];
  self.syncEverythingItem =
      [[SyncSwitchItem alloc] initWithType:SyncEverythingItemType];
  self.syncEverythingItem.text = GetNSString(IDS_IOS_SYNC_EVERYTHING_TITLE);
  [self updateSyncEverythingItemNotifyConsumer:NO];
  [model addItem:self.syncEverythingItem
      toSectionWithIdentifier:SyncDataTypeSectionIdentifier];
  self.syncSwitchItems = @[
    [self switchItemWithDataType:SyncSetupService::kSyncAutofill],
    [self switchItemWithDataType:SyncSetupService::kSyncBookmarks],
    [self switchItemWithDataType:SyncSetupService::kSyncOmniboxHistory],
    [self switchItemWithDataType:SyncSetupService::kSyncOpenTabs],
    [self switchItemWithDataType:SyncSetupService::kSyncPasswords],
    [self switchItemWithDataType:SyncSetupService::kSyncReadingList],
    [self switchItemWithDataType:SyncSetupService::kSyncPreferences]
  ];
  [self updateSyncDataItemsNotifyConsumer:NO];
  for (SyncSwitchItem* switchItem in self.syncSwitchItems) {
    [model addItem:switchItem
        toSectionWithIdentifier:SyncDataTypeSectionIdentifier];
  }
}

// Updates the sync everything item, and notify the consumer if |notifyConsumer|
// is set to YES.
- (void)updateSyncEverythingItemNotifyConsumer:(BOOL)notifyConsumer {
  BOOL shouldSyncEverythingBeEditable =
      self.syncSetupService->IsSyncEnabled() &&
      !self.disabledBecauseOfSyncError;
  BOOL shouldSyncEverythingItemBeOn =
      self.syncSetupService->IsSyncEnabled() &&
      self.syncSetupService->IsSyncingAllDataTypes();
  BOOL needsUpdate =
      (self.syncEverythingItem.on != shouldSyncEverythingItemBeOn) ||
      (self.syncEverythingItem.enabled != shouldSyncEverythingBeEditable);
  self.syncEverythingItem.on = shouldSyncEverythingItemBeOn;
  self.syncEverythingItem.enabled = shouldSyncEverythingBeEditable;
  if (needsUpdate && notifyConsumer) {
    [self.consumer reloadItem:self.syncEverythingItem];
  }
}

// Updates all the sync data type items, and notify the consumer if
// |notifyConsumer| is set to YES.
- (void)updateSyncDataItemsNotifyConsumer:(BOOL)notifyConsumer {
  BOOL isSyncDataTypeItemEnabled =
      (!self.syncSetupService->IsSyncingAllDataTypes() &&
       self.syncSetupService->IsSyncEnabled() &&
       !self.disabledBecauseOfSyncError);
  for (SyncSwitchItem* syncSwitchItem in self.syncSwitchItems) {
    SyncSetupService::SyncableDatatype dataType =
        static_cast<SyncSetupService::SyncableDatatype>(
            syncSwitchItem.dataType);
    syncer::ModelType modelType = self.syncSetupService->GetModelType(dataType);
    BOOL isDataTypeSynced =
        self.syncSetupService->IsDataTypePreferred(modelType);
    BOOL needsUpdate = (syncSwitchItem.on != isDataTypeSynced) ||
                       (syncSwitchItem.isEnabled != isSyncDataTypeItemEnabled);
    syncSwitchItem.on = isDataTypeSynced;
    syncSwitchItem.enabled = isSyncDataTypeItemEnabled;
    if (needsUpdate && notifyConsumer) {
      [self.consumer reloadItem:syncSwitchItem];
    }
  }
}

#pragma mark - Private

// Creates a SyncSwitchItem instance.
- (SyncSwitchItem*)switchItemWithDataType:
    (SyncSetupService::SyncableDatatype)dataType {
  NSInteger itemType = 0;
  int textStringID = 0;
  switch (dataType) {
    case SyncSetupService::kSyncBookmarks:
      itemType = BookmarksDataTypeItemType;
      textStringID = IDS_SYNC_DATATYPE_BOOKMARKS;
      break;
    case SyncSetupService::kSyncOmniboxHistory:
      itemType = HistoryDataTypeItemType;
      textStringID = IDS_SYNC_DATATYPE_TYPED_URLS;
      break;
    case SyncSetupService::kSyncPasswords:
      itemType = PasswordsDataTypeItemType;
      textStringID = IDS_SYNC_DATATYPE_PASSWORDS;
      break;
    case SyncSetupService::kSyncOpenTabs:
      itemType = OpenTabsDataTypeItemType;
      textStringID = IDS_SYNC_DATATYPE_TABS;
      break;
    case SyncSetupService::kSyncAutofill:
      itemType = AutofillDataTypeItemType;
      textStringID = IDS_SYNC_DATATYPE_AUTOFILL;
      break;
    case SyncSetupService::kSyncPreferences:
      itemType = SettingsDataTypeItemType;
      textStringID = IDS_SYNC_DATATYPE_PREFERENCES;
      break;
    case SyncSetupService::kSyncReadingList:
      itemType = ReadingListDataTypeItemType;
      textStringID = IDS_SYNC_DATATYPE_READING_LIST;
      break;
    case SyncSetupService::kNumberOfSyncableDatatypes:
      NOTREACHED();
      break;
  }
  DCHECK_NE(itemType, 0);
  DCHECK_NE(textStringID, 0);
  SyncSwitchItem* switchItem = [[SyncSwitchItem alloc] initWithType:itemType];
  switchItem.text = GetNSString(textStringID);
  switchItem.dataType = dataType;
  return switchItem;
}

#pragma mark - Properties

- (BOOL)disabledBecauseOfSyncError {
  SyncSetupService::SyncServiceState state =
      self.syncSetupService->GetSyncServiceState();
  return state != SyncSetupService::kNoSyncServiceError &&
         state != SyncSetupService::kSyncServiceNeedsPassphrase;
}

#pragma mark - ManageSyncSettingsTableViewControllerModelDelegate

- (void)manageSyncSettingsTableViewControllerLoadModel:
    (id<ManageSyncSettingsConsumer>)controller {
  DCHECK_EQ(self.consumer, controller);
  [self loadSyncDataTypeSection];
}

#pragma mark - SyncObserverModelBridge

- (void)onSyncStateChanged {
  if (_ignoreSyncStateChanges) {
    // The UI should not updated so the switch animations can run smoothly.
    return;
  }
  [self updateSyncEverythingItemNotifyConsumer:YES];
  [self updateSyncDataItemsNotifyConsumer:YES];
}

#pragma mark - ManageSyncSettingsServiceDelegate

- (void)toggleSwitchItem:(SyncSwitchItem*)switchItem withValue:(BOOL)value {
  {
    // The notifications should be ignored to get smooth switch animations.
    // Notifications are sent by SyncObserverModelBridge while changing
    // settings.
    base::AutoReset<BOOL> autoReset(&_ignoreSyncStateChanges, YES);
    switchItem.on = value;
    if (switchItem.type == SyncEverythingItemType) {
      self.syncSetupService->SetSyncingAllDataTypes(value);
    } else {
      SyncSetupService::SyncableDatatype dataType =
          static_cast<SyncSetupService::SyncableDatatype>(switchItem.dataType);
      syncer::ModelType modelType =
          self.syncSetupService->GetModelType(dataType);
      self.syncSetupService->SetDataTypeEnabled(modelType, value);
    }
  }
  [self updateSyncEverythingItemNotifyConsumer:YES];
  [self updateSyncDataItemsNotifyConsumer:YES];
}

@end
