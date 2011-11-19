// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_INTERNAL_API_DEBUG_INFO_EVENT_LISTENER_H_
#define CHROME_BROWSER_SYNC_INTERNAL_API_DEBUG_INFO_EVENT_LISTENER_H_

#include <queue>
#include <string>

#include "chrome/browser/sync/internal_api/sync_manager.h"
#include "chrome/browser/sync/js/js_backend.h"
#include "chrome/browser/sync/protocol/sync.pb.h"
#include "chrome/browser/sync/sessions/debug_info_getter.h"
#include "chrome/browser/sync/sessions/session_state.h"
#include "chrome/browser/sync/util/weak_handle.h"
#include "chrome/common/net/gaia/google_service_auth_error.h"

namespace sync_api {

const unsigned int kMaxEntries = 6;

// Listens to events and records them in a queue. And passes the events to
// syncer when requested.
class DebugInfoEventListener : public sync_api::SyncManager::Observer,
                               public browser_sync::sessions::DebugInfoGetter {
 public:
  DebugInfoEventListener();
  virtual ~DebugInfoEventListener();

  // SyncManager::Observer implementation.
  virtual void OnSyncCycleCompleted(
    const browser_sync::sessions::SyncSessionSnapshot* snapshot) OVERRIDE;
  virtual void OnInitializationComplete(
    const browser_sync::WeakHandle<browser_sync::JsBackend>& js_backend,
      bool success) OVERRIDE;
  virtual void OnAuthError(
      const GoogleServiceAuthError& auth_error) OVERRIDE;
  virtual void OnPassphraseRequired(
      sync_api::PassphraseRequiredReason reason) OVERRIDE;
  virtual void OnPassphraseAccepted(
      const std::string& bootstrap_token) OVERRIDE;
  virtual void OnStopSyncingPermanently() OVERRIDE;
  virtual void OnUpdatedToken(const std::string& token) OVERRIDE;
  virtual void OnClearServerDataFailed() OVERRIDE;
  virtual void OnClearServerDataSucceeded() OVERRIDE;
  virtual void OnEncryptedTypesChanged(
      const syncable::ModelTypeSet& encrypted_types,
      bool encrypt_everything) OVERRIDE;
  virtual void OnEncryptionComplete() OVERRIDE;
  virtual void OnActionableError(
      const browser_sync::SyncProtocolError& sync_error) OVERRIDE;

  // DebugInfoGetter Implementation.
  virtual void GetAndClearDebugInfo(sync_pb::DebugInfo* debug_info) OVERRIDE;

 private:
  FRIEND_TEST_ALL_PREFIXES(DebugInfoEventListenerTest, VerifyEventsAdded);
  FRIEND_TEST_ALL_PREFIXES(DebugInfoEventListenerTest, VerifyQueueSize);
  FRIEND_TEST_ALL_PREFIXES(DebugInfoEventListenerTest, VerifyGetAndClearEvents);

  void AddEventToQueue(const sync_pb::DebugEventInfo& event_info);
  void CreateAndAddEvent(sync_pb::DebugEventInfo::EventType type);
  std::queue<sync_pb::DebugEventInfo> events_;

  // True indicates we had to drop one or more events to keep our limit of
  // |kMaxEntries|.
  bool events_dropped_;

  DISALLOW_COPY_AND_ASSIGN(DebugInfoEventListener);
};

}  // namespace sync_api
#endif  // CHROME_BROWSER_SYNC_INTERNAL_API_DEBUG_INFO_EVENT_LISTENER_H_
