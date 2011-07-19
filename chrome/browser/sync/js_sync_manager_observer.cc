// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/js_sync_manager_observer.h"

#include <cstddef>

#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/sync/js_arg_list.h"
#include "chrome/browser/sync/js_event_details.h"
#include "chrome/browser/sync/js_event_router.h"
#include "chrome/browser/sync/sessions/session_state.h"
#include "chrome/browser/sync/syncable/model_type.h"

namespace browser_sync {

JsSyncManagerObserver::JsSyncManagerObserver(JsEventRouter* parent_router)
    : parent_router_(parent_router) {
  DCHECK(parent_router_);
}

JsSyncManagerObserver::~JsSyncManagerObserver() {}

void JsSyncManagerObserver::OnChangesApplied(
    syncable::ModelType model_type,
    const sync_api::BaseTransaction* trans,
    const sync_api::SyncManager::ChangeRecord* changes,
    int change_count) {
  DictionaryValue details;
  details.SetString("modelType", syncable::ModelTypeToString(model_type));
  ListValue* change_values = new ListValue();
  details.Set("changes", change_values);
  for (int i = 0; i < change_count; ++i) {
    change_values->Append(changes[i].ToValue(trans));
  }
  parent_router_->RouteJsEvent("onChangesApplied", JsEventDetails(&details));
}

void JsSyncManagerObserver::OnChangesComplete(
    syncable::ModelType model_type) {
  DictionaryValue details;
  details.SetString("modelType", syncable::ModelTypeToString(model_type));
  parent_router_->RouteJsEvent("onChangesComplete", JsEventDetails(&details));
}

void JsSyncManagerObserver::OnSyncCycleCompleted(
    const sessions::SyncSessionSnapshot* snapshot) {
  DictionaryValue details;
  details.Set("snapshot", snapshot->ToValue());
  parent_router_->RouteJsEvent("onSyncCycleCompleted",
                               JsEventDetails(&details));
}

void JsSyncManagerObserver::OnAuthError(
    const GoogleServiceAuthError& auth_error) {
  DictionaryValue details;
  details.Set("authError", auth_error.ToValue());
  parent_router_->RouteJsEvent("onAuthError", JsEventDetails(&details));
}

void JsSyncManagerObserver::OnUpdatedToken(const std::string& token) {
  DictionaryValue details;
  details.SetString("token", "<redacted>");
  parent_router_->RouteJsEvent("onUpdatedToken", JsEventDetails(&details));
}

void JsSyncManagerObserver::OnPassphraseRequired(
    sync_api::PassphraseRequiredReason reason) {
  DictionaryValue details;
  details.SetString("reason",
                     sync_api::PassphraseRequiredReasonToString(reason));
  parent_router_->RouteJsEvent("onPassphraseRequired",
                               JsEventDetails(&details));
}

void JsSyncManagerObserver::OnPassphraseAccepted(
    const std::string& bootstrap_token) {
  DictionaryValue details;
  details.SetString("bootstrapToken", "<redacted>");
  parent_router_->RouteJsEvent("onPassphraseAccepted",
                               JsEventDetails(&details));
}

void JsSyncManagerObserver::OnEncryptionComplete(
    const syncable::ModelTypeSet& encrypted_types) {
  DictionaryValue details;
  details.Set("encryptedTypes",
               syncable::ModelTypeSetToValue(encrypted_types));
  parent_router_->RouteJsEvent("onEncryptionComplete",
                               JsEventDetails(&details));
}

void JsSyncManagerObserver::OnMigrationNeededForTypes(
    const syncable::ModelTypeSet& types) {
  DictionaryValue details;
  details.Set("types", syncable::ModelTypeSetToValue(types));
  parent_router_->RouteJsEvent("onMigrationNeededForTypes",
                               JsEventDetails(&details));
}

void JsSyncManagerObserver::OnInitializationComplete() {
  parent_router_->RouteJsEvent("onInitializationComplete", JsEventDetails());
}

void JsSyncManagerObserver::OnStopSyncingPermanently() {
  parent_router_->RouteJsEvent("onStopSyncingPermanently", JsEventDetails());
}

void JsSyncManagerObserver::OnClearServerDataSucceeded() {
  parent_router_->RouteJsEvent("onClearServerDataSucceeded", JsEventDetails());
}

void JsSyncManagerObserver::OnClearServerDataFailed() {
  parent_router_->RouteJsEvent("onClearServerDataFailed", JsEventDetails());
}

}  // namespace browser_sync
