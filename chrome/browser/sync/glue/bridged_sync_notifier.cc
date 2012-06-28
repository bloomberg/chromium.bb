// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/bridged_sync_notifier.h"

#include "chrome/browser/sync/glue/chrome_sync_notification_bridge.h"

namespace browser_sync {

BridgedSyncNotifier::BridgedSyncNotifier(
    ChromeSyncNotificationBridge* bridge,
    syncer::SyncNotifier* delegate)
    : bridge_(bridge), delegate_(delegate) {
  DCHECK(bridge_);
}

BridgedSyncNotifier::~BridgedSyncNotifier() {
}

void BridgedSyncNotifier::AddObserver(
    syncer::SyncNotifierObserver* observer) {
  if (delegate_.get())
   delegate_->AddObserver(observer);
  bridge_->AddObserver(observer);
}

void BridgedSyncNotifier::RemoveObserver(
    syncer::SyncNotifierObserver* observer) {
  bridge_->RemoveObserver(observer);
  if (delegate_.get())
    delegate_->RemoveObserver(observer);
}

void BridgedSyncNotifier::SetUniqueId(const std::string& unique_id) {
  if (delegate_.get())
    delegate_->SetUniqueId(unique_id);
}

void BridgedSyncNotifier::SetStateDeprecated(const std::string& state) {
  if (delegate_.get())
    delegate_->SetStateDeprecated(state);
}

void BridgedSyncNotifier::UpdateCredentials(
    const std::string& email, const std::string& token) {
  if (delegate_.get())
    delegate_->UpdateCredentials(email, token);
}

void BridgedSyncNotifier::UpdateEnabledTypes(
    syncable::ModelTypeSet enabled_types) {
  if (delegate_.get())
    delegate_->UpdateEnabledTypes(enabled_types);
}

void BridgedSyncNotifier::SendNotification(
    syncable::ModelTypeSet changed_types) {
  if (delegate_.get())
    delegate_->SendNotification(changed_types);
}

}  // namespace browser_sync
