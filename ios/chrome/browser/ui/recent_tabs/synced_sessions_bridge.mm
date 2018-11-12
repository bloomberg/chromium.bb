// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/recent_tabs/synced_sessions_bridge.h"

#include "components/browser_sync/profile_sync_service.h"
#include "components/sync_sessions/session_sync_service.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/signin/identity_manager_factory.h"
#include "ios/chrome/browser/sync/profile_sync_service_factory.h"
#include "ios/chrome/browser/sync/session_sync_service_factory.h"
#include "ios/chrome/browser/sync/sync_setup_service.h"
#include "ios/chrome/browser/sync/sync_setup_service_factory.h"
#include "services/identity/public/cpp/identity_manager.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace synced_sessions {

#pragma mark - SyncedSessionsObserverBridge

SyncedSessionsObserverBridge::SyncedSessionsObserverBridge(
    id<SyncedSessionsObserver> owner,
    ios::ChromeBrowserState* browserState)
    : SyncObserverBridge(
          owner,
          ProfileSyncServiceFactory::GetForBrowserState(browserState)),
      owner_(owner),
      identity_manager_(
          IdentityManagerFactory::GetForBrowserState(browserState)),
      browser_state_(browserState),
      identity_manager_observer_(this) {
  identity_manager_observer_.Add(identity_manager_);

  // base::Unretained() is safe below because the subscription itself is a class
  // member field and handles destruction well.
  foreign_session_updated_subscription_ =
      SessionSyncServiceFactory::GetForBrowserState(browserState)
          ->SubscribeToForeignSessionsChanged(base::BindRepeating(
              &SyncedSessionsObserverBridge::OnForeignSessionChanged,
              base::Unretained(this)));
}

SyncedSessionsObserverBridge::~SyncedSessionsObserverBridge() {}

#pragma mark - SyncObserverBridge

void SyncedSessionsObserverBridge::OnSyncConfigurationCompleted(
    syncer::SyncService* sync) {
  // TODO(crbug.com/895455): This notification seems redundant because
  // OnForeignSessionChanged() should be called when the initial sync is
  // completed.
  [owner_ reloadSessions];
}

bool SyncedSessionsObserverBridge::IsFirstSyncCycleCompleted() {
  // TODO(crbug.com/895455): This could probably be implemented via
  // SessionSyncService directly and possibly remove all dependencies to
  // SyncService/SyncSetupService.
  return SyncSetupServiceFactory::GetForBrowserState(browser_state_)
      ->IsDataTypeActive(syncer::SESSIONS);
}

#pragma mark - identity::IdentityManager::Observer

void SyncedSessionsObserverBridge::OnPrimaryAccountCleared(
    const AccountInfo& previous_primary_account_info) {
  [owner_ reloadSessions];
}

#pragma mark - Signin and syncing status

bool SyncedSessionsObserverBridge::IsSignedIn() {
  return identity_manager_->HasPrimaryAccount();
}

void SyncedSessionsObserverBridge::OnForeignSessionChanged() {
  [owner_ reloadSessions];
}

}  // namespace synced_sessions
