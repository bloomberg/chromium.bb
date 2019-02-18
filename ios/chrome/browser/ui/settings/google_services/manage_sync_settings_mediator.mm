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
}

// Updates the sync everything item, and notify the consumer if |notifyConsumer|
// is set to YES.
- (void)updateSyncEverythingItemNotifyConsumer:(BOOL)notifyConsumer {
  SyncSetupService::SyncServiceState state =
      self.syncSetupService->GetSyncServiceState();
  BOOL shouldSyncEverythingBeEditable =
      self.syncSetupService->IsSyncEnabled() &&
      (state == SyncSetupService::kNoSyncServiceError ||
       state == SyncSetupService::kSyncServiceNeedsPassphrase);
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
}

#pragma mark - ManageSyncSettingsServiceDelegate

- (void)toggleSwitchItem:(SyncSwitchItem*)switchItem withValue:(BOOL)value {
  {
    // The notifications should be ignored to get smooth switch animations.
    // Notifications are sent by SyncObserverModelBridge while changing
    // settings.
    base::AutoReset<BOOL> autoReset(&_ignoreSyncStateChanges, YES);
    switchItem.on = value;
    DCHECK(switchItem.type == SyncEverythingItemType);
    self.syncSetupService->SetSyncingAllDataTypes(value);
  }
  [self updateSyncEverythingItemNotifyConsumer:YES];
}

@end
