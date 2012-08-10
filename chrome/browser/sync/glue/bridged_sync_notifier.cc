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

void BridgedSyncNotifier::RegisterHandler(
    syncer::SyncNotifierObserver* handler) {
  if (delegate_.get())
    delegate_->RegisterHandler(handler);
  bridge_->RegisterHandler(handler);
}

void BridgedSyncNotifier::UpdateRegisteredIds(
    syncer::SyncNotifierObserver* handler,
    const syncer::ObjectIdSet& ids) {
  if (delegate_.get())
    delegate_->UpdateRegisteredIds(handler, ids);
  bridge_->UpdateRegisteredIds(handler, ids);
}

void BridgedSyncNotifier::UnregisterHandler(
    syncer::SyncNotifierObserver* handler) {
  if (delegate_.get())
    delegate_->UnregisterHandler(handler);
  bridge_->UnregisterHandler(handler);
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

void BridgedSyncNotifier::SendNotification(
    syncer::ModelTypeSet changed_types) {
  if (delegate_.get())
    delegate_->SendNotification(changed_types);
}

}  // namespace browser_sync
