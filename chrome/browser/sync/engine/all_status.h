// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The AllStatus object watches various sync engine components and aggregates
// the status of all of them into one place.

#ifndef CHROME_BROWSER_SYNC_ENGINE_ALL_STATUS_H_
#define CHROME_BROWSER_SYNC_ENGINE_ALL_STATUS_H_
#pragma once

#include <map>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "chrome/browser/sync/engine/syncer_types.h"
#include "chrome/browser/sync/internal_api/sync_manager.h"
#include "chrome/browser/sync/syncable/model_type.h"

namespace browser_sync {

class ScopedStatusLock;
struct AuthWatcherEvent;
struct ServerConnectionEvent;

class AllStatus : public SyncEngineEventListener {
  friend class ScopedStatusLock;
 public:
  AllStatus();
  virtual ~AllStatus();

  void HandleServerConnectionEvent(const ServerConnectionEvent& event);

  void HandleAuthWatcherEvent(const AuthWatcherEvent& event);

  virtual void OnSyncEngineEvent(const SyncEngineEvent& event) OVERRIDE;

  sync_api::SyncManager::Status status() const;

  void SetNotificationsEnabled(bool notifications_enabled);

  void IncrementNotifiableCommits();

  void IncrementNotificationsReceived();

  void SetEncryptedTypes(const syncable::ModelTypeSet& types);
  void SetCryptographerReady(bool ready);
  void SetCryptoHasPendingKeys(bool has_pending_keys);

  void SetUniqueId(const std::string& guid);

 protected:
  // Examines syncer to calculate syncing and the unsynced count,
  // and returns a Status with new values.
  sync_api::SyncManager::Status CalcSyncing(const SyncEngineEvent& event) const;
  sync_api::SyncManager::Status CreateBlankStatus() const;

  // Examines status to see what has changed, updates old_status in place.
  void CalcStatusChanges();

  sync_api::SyncManager::Status status_;

  mutable base::Lock mutex_;  // Protects all data members.
  DISALLOW_COPY_AND_ASSIGN(AllStatus);
};

class ScopedStatusLock {
 public:
  explicit ScopedStatusLock(AllStatus* allstatus);
  ~ScopedStatusLock();
 protected:
  AllStatus* allstatus_;
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_ENGINE_ALL_STATUS_H_
