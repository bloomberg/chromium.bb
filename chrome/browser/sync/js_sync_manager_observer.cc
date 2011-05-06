// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/js_sync_manager_observer.h"

#include <cstddef>

#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/sync/js_arg_list.h"
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
  ListValue return_args;
  return_args.Append(Value::CreateStringValue(
      syncable::ModelTypeToString(model_type)));
  ListValue* change_values = new ListValue();
  return_args.Append(change_values);
  for (int i = 0; i < change_count; ++i) {
    change_values->Append(changes[i].ToValue(trans));
  }
  parent_router_->RouteJsEvent("onChangesApplied", JsArgList(&return_args));
}

void JsSyncManagerObserver::OnChangesComplete(
    syncable::ModelType model_type) {
  ListValue return_args;
  return_args.Append(Value::CreateStringValue(
      syncable::ModelTypeToString(model_type)));
  parent_router_->RouteJsEvent("onChangesComplete", JsArgList(&return_args));
}

void JsSyncManagerObserver::OnSyncCycleCompleted(
    const sessions::SyncSessionSnapshot* snapshot) {
  ListValue return_args;
  return_args.Append(snapshot->ToValue());
  parent_router_->RouteJsEvent("onSyncCycleCompleted",
                               JsArgList(&return_args));
}

void JsSyncManagerObserver::OnAuthError(
    const GoogleServiceAuthError& auth_error) {
  ListValue return_args;
  return_args.Append(auth_error.ToValue());
  parent_router_->RouteJsEvent("onAuthError", JsArgList(&return_args));
}

void JsSyncManagerObserver::OnUpdatedToken(const std::string& token) {
  ListValue return_args;
  return_args.Append(Value::CreateStringValue("<redacted>"));
  parent_router_->RouteJsEvent("onUpdatedToken", JsArgList(&return_args));
}

void JsSyncManagerObserver::OnPassphraseRequired(
    sync_api::PassphraseRequiredReason reason) {
  ListValue return_args;

  return_args.Append(Value::CreateStringValue(
      sync_api::PassphraseRequiredReasonToString(reason)));
  parent_router_->RouteJsEvent("onPassphraseRequired",
                               JsArgList(&return_args));
}

void JsSyncManagerObserver::OnPassphraseAccepted(
    const std::string& bootstrap_token) {
  ListValue return_args;
  return_args.Append(Value::CreateStringValue("<redacted>"));
  parent_router_->RouteJsEvent("onPassphraseAccepted",
                               JsArgList(&return_args));
}

void JsSyncManagerObserver::OnEncryptionComplete(
    const syncable::ModelTypeSet& encrypted_types) {
  ListValue return_args;
  return_args.Append(syncable::ModelTypeSetToValue(encrypted_types));
  parent_router_->RouteJsEvent("onEncryptionComplete",
                               JsArgList(&return_args));
}

void JsSyncManagerObserver::OnMigrationNeededForTypes(
    const syncable::ModelTypeSet& types) {
  ListValue return_args;
  return_args.Append(syncable::ModelTypeSetToValue(types));
  parent_router_->RouteJsEvent("onMigrationNeededForTypes",
                               JsArgList(&return_args));
}

void JsSyncManagerObserver::OnInitializationComplete() {
  parent_router_->RouteJsEvent("onInitializationComplete", JsArgList());
}

void JsSyncManagerObserver::OnStopSyncingPermanently() {
  parent_router_->RouteJsEvent("onStopSyncingPermanently", JsArgList());
}

void JsSyncManagerObserver::OnClearServerDataSucceeded() {
  parent_router_->RouteJsEvent("onClearServerDataSucceeded", JsArgList());
}

void JsSyncManagerObserver::OnClearServerDataFailed() {
  parent_router_->RouteJsEvent("onClearServerDataFailed", JsArgList());
}

}  // namespace browser_sync
