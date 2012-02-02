// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/notifier/bridged_sync_notifier.h"

#include "chrome/browser/sync/notifier/chrome_sync_notification_bridge.h"

namespace sync_notifier {

BridgedSyncNotifier::BridgedSyncNotifier(
    ChromeSyncNotificationBridge* bridge, SyncNotifier* delegate)
    : bridge_(bridge), delegate_(delegate) {
  DCHECK(bridge_);
  DCHECK(delegate_.get());
}

BridgedSyncNotifier::~BridgedSyncNotifier() {
}

void BridgedSyncNotifier::AddObserver(SyncNotifierObserver* observer) {
  delegate_->AddObserver(observer);
  bridge_->AddObserver(observer);
}

void BridgedSyncNotifier::RemoveObserver(
    SyncNotifierObserver* observer) {
  bridge_->RemoveObserver(observer);
  delegate_->RemoveObserver(observer);
}

void BridgedSyncNotifier::SetUniqueId(const std::string& unique_id) {
  delegate_->SetUniqueId(unique_id);
}

void BridgedSyncNotifier::SetState(const std::string& state) {
  delegate_->SetState(state);
}

void BridgedSyncNotifier::UpdateCredentials(
    const std::string& email, const std::string& token) {
  delegate_->UpdateCredentials(email, token);
}

void BridgedSyncNotifier::UpdateEnabledTypes(
    syncable::ModelTypeSet enabled_types) {
  delegate_->UpdateEnabledTypes(enabled_types);
}

void BridgedSyncNotifier::SendNotification(
    syncable::ModelTypeSet changed_types) {
  delegate_->SendNotification(changed_types);
}

}  // namespace sync_notifier
