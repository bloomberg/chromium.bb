// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/bridged_invalidator.h"

#include "chrome/browser/sync/glue/chrome_sync_notification_bridge.h"

namespace browser_sync {

BridgedInvalidator::BridgedInvalidator(
    ChromeSyncNotificationBridge* bridge,
    syncer::Invalidator* delegate)
    : bridge_(bridge), delegate_(delegate) {
  DCHECK(bridge_);
}

BridgedInvalidator::~BridgedInvalidator() {
}

void BridgedInvalidator::RegisterHandler(
    syncer::InvalidationHandler* handler) {
  if (delegate_.get())
    delegate_->RegisterHandler(handler);
  bridge_->RegisterHandler(handler);
}

void BridgedInvalidator::UpdateRegisteredIds(
    syncer::InvalidationHandler* handler,
    const syncer::ObjectIdSet& ids) {
  if (delegate_.get())
    delegate_->UpdateRegisteredIds(handler, ids);
  bridge_->UpdateRegisteredIds(handler, ids);
}

void BridgedInvalidator::UnregisterHandler(
    syncer::InvalidationHandler* handler) {
  if (delegate_.get())
    delegate_->UnregisterHandler(handler);
  bridge_->UnregisterHandler(handler);
}

void BridgedInvalidator::SetUniqueId(const std::string& unique_id) {
  if (delegate_.get())
    delegate_->SetUniqueId(unique_id);
}

void BridgedInvalidator::SetStateDeprecated(const std::string& state) {
  if (delegate_.get())
    delegate_->SetStateDeprecated(state);
}

void BridgedInvalidator::UpdateCredentials(
    const std::string& email, const std::string& token) {
  if (delegate_.get())
    delegate_->UpdateCredentials(email, token);
}

void BridgedInvalidator::SendNotification(
    const syncer::ObjectIdStateMap& id_state_map) {
  if (delegate_.get())
    delegate_->SendNotification(id_state_map);
}

}  // namespace browser_sync
