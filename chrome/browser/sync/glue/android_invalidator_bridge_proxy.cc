// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/android_invalidator_bridge_proxy.h"

#include "chrome/browser/sync/glue/android_invalidator_bridge.h"

using std::string;
using syncer::InvalidatorState;

namespace browser_sync {

AndroidInvalidatorBridgeProxy::AndroidInvalidatorBridgeProxy(
    AndroidInvalidatorBridge* bridge)
    : bridge_(bridge) {
  DCHECK(bridge_);
}

AndroidInvalidatorBridgeProxy::~AndroidInvalidatorBridgeProxy() {
}

void AndroidInvalidatorBridgeProxy::RegisterHandler(
    syncer::InvalidationHandler* handler) {
  bridge_->RegisterHandler(handler);
}

void AndroidInvalidatorBridgeProxy::UpdateRegisteredIds(
    syncer::InvalidationHandler* handler,
    const syncer::ObjectIdSet& ids) {
  bridge_->UpdateRegisteredIds(handler, ids);
}

InvalidatorState AndroidInvalidatorBridgeProxy::GetInvalidatorState() const {
  return bridge_->GetInvalidatorState();
}

void AndroidInvalidatorBridgeProxy::UnregisterHandler(
    syncer::InvalidationHandler* handler) {
  bridge_->UnregisterHandler(handler);
}

void AndroidInvalidatorBridgeProxy::Acknowledge(
    const invalidation::ObjectId& id,
    const syncer::AckHandle& ack_handle) {
  bridge_->Acknowledge(id, ack_handle);
}

void AndroidInvalidatorBridgeProxy::UpdateCredentials(
    const string& email, const string& token) {
  bridge_->UpdateCredentials(email, token);
}

void AndroidInvalidatorBridgeProxy::SendInvalidation(
    const syncer::ObjectIdInvalidationMap& invalidation_map) {
  bridge_->SendInvalidation(invalidation_map);
}

}  // namespace browser_sync
