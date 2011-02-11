// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_JS_SYNC_MANAGER_OBSERVER_H_
#define CHROME_BROWSER_SYNC_JS_SYNC_MANAGER_OBSERVER_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "chrome/browser/sync/engine/syncapi.h"

namespace browser_sync {

class JsEventRouter;

// Routes SyncManager events to a JsEventRouter.
class JsSyncManagerObserver : public sync_api::SyncManager::Observer {
 public:
  // |parent_router| must be non-NULL and must outlive this object.
  explicit JsSyncManagerObserver(JsEventRouter* parent_router);

  virtual ~JsSyncManagerObserver();

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
  virtual void OnPassphraseRequired(bool for_decryption);
  virtual void OnPassphraseAccepted(const std::string& bootstrap_token);
  virtual void OnInitializationComplete();
  virtual void OnPaused();
  virtual void OnResumed();
  virtual void OnStopSyncingPermanently();
  virtual void OnClearServerDataSucceeded();
  virtual void OnClearServerDataFailed();

 private:
  JsEventRouter* parent_router_;

  DISALLOW_COPY_AND_ASSIGN(JsSyncManagerObserver);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_JS_SYNC_MANAGER_OBSERVER_H_
