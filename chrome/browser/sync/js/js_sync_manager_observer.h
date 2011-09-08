// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_JS_JS_SYNC_MANAGER_OBSERVER_H_
#define CHROME_BROWSER_SYNC_JS_JS_SYNC_MANAGER_OBSERVER_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "chrome/browser/sync/internal_api/sync_manager.h"
#include "chrome/browser/sync/protocol/sync_protocol_error.h"
#include "chrome/browser/sync/weak_handle.h"

namespace tracked_objects {
class Location;
}  // namespace tracked_objects

namespace browser_sync {

class JsEventDetails;
class JsEventHandler;

// Routes SyncManager events to a JsEventHandler.
class JsSyncManagerObserver : public sync_api::SyncManager::Observer {
 public:
  JsSyncManagerObserver();
  virtual ~JsSyncManagerObserver();

  void SetJsEventHandler(const WeakHandle<JsEventHandler>& event_handler);

  // sync_api::SyncManager::Observer implementation.
  virtual void OnChangesApplied(
      syncable::ModelType model_type,
      const sync_api::BaseTransaction* trans,
      const sync_api::SyncManager::ChangeRecord* changes,
      int change_count);
  virtual void OnChangesComplete(syncable::ModelType model_type);
  virtual void OnSyncCycleCompleted(
      const sessions::SyncSessionSnapshot* snapshot);
  virtual void OnAuthError(const GoogleServiceAuthError& auth_error);
  virtual void OnUpdatedToken(const std::string& token);
  virtual void OnPassphraseRequired(sync_api::PassphraseRequiredReason reason);
  virtual void OnPassphraseAccepted(const std::string& bootstrap_token);
  virtual void OnEncryptionComplete(
      const syncable::ModelTypeSet& encrypted_types);
  virtual void OnInitializationComplete(
      const WeakHandle<JsBackend>& js_backend,
      bool success);
  virtual void OnStopSyncingPermanently();
  virtual void OnClearServerDataSucceeded();
  virtual void OnClearServerDataFailed();
  virtual void OnMigrationNeededForTypes(const syncable::ModelTypeSet& types);
  virtual void OnActionableError(
      const browser_sync::SyncProtocolError& sync_protocol_error);

 private:
  void HandleJsEvent(const tracked_objects::Location& from_here,
                    const std::string& name, const JsEventDetails& details);

  WeakHandle<JsEventHandler> event_handler_;

  DISALLOW_COPY_AND_ASSIGN(JsSyncManagerObserver);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_JS_JS_SYNC_MANAGER_OBSERVER_H_
