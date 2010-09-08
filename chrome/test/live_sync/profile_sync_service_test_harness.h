// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_LIVE_SYNC_PROFILE_SYNC_SERVICE_TEST_HARNESS_H_
#define CHROME_TEST_LIVE_SYNC_PROFILE_SYNC_SERVICE_TEST_HARNESS_H_
#pragma once

#include <string>
#include <vector>

#include "base/time.h"
#include "chrome/browser/sync/profile_sync_service.h"

using browser_sync::sessions::SyncSessionSnapshot;

class Profile;

// An instance of this class is basically our notion of a "sync client" for
// test purposes.  It harnesses the ProfileSyncService member of the profile
// passed to it on construction and automates certain things like setup and
// authentication.  It provides ways to "wait" adequate periods of time for
// several clients to get to the same state.
class ProfileSyncServiceTestHarness : public ProfileSyncServiceObserver {
 public:
  ProfileSyncServiceTestHarness(Profile* p, const std::string& username,
                                const std::string& password, int id);

  // Creates a ProfileSyncService for the profile passed at construction and
  // enables sync.  Returns true only after sync has been fully initialized and
  // authenticated, and we are ready to process changes.
  bool SetupSync();

  // Retries a sync setup when the previous attempt was aborted by an
  // authentication failure.
  bool RetryAuthentication();

  // ProfileSyncServiceObserver implementation.
  virtual void OnStateChanged();

  // Blocks the caller until this harness has completed a single sync cycle
  // since the previous one.  Returns true if a sync cycle has completed.
  bool AwaitSyncCycleCompletion(const std::string& reason);

  // Blocks the caller until this harness has observed that the sync engine
  // has "synced" up to at least the specified local timestamp.
  bool WaitUntilTimestampIsAtLeast(int64 timestamp, const std::string& reason);

  // Calling this acts as a barrier and blocks the caller until |this| and
  // |partner| have both completed a sync cycle.  When calling this method,
  // the |partner| should be the passive responder who responds to the actions
  // of |this|.  This method relies upon the synchronization of callbacks
  // from the message queue. Returns true if two sync cycles have completed.
  // Note: Use this method when exactly one client makes local change(s), and
  // exactly one client is waiting to receive those changes.
  bool AwaitMutualSyncCycleCompletion(ProfileSyncServiceTestHarness* partner);

  // Blocks the caller until |this| completes its ongoing sync cycle and every
  // other client in |partners| has a timestamp that is greater than or equal to
  // the timestamp of |this|. Note: Use this method when exactly one client
  // makes local change(s), and more than one client is waiting to receive those
  // changes.
  bool AwaitGroupSyncCycleCompletion(
      std::vector<ProfileSyncServiceTestHarness*>& partners);

  // Blocks the caller until every client in |clients| completes its ongoing
  // sync cycle and all the clients' timestamps match.  Note: Use this method
  // when more than one client makes local change(s), and more than one client
  // is waiting to receive those changes.
  static bool AwaitQuiescence(
      std::vector<ProfileSyncServiceTestHarness*>& clients);

  ProfileSyncService* service() { return service_; }

  // See ProfileSyncService::ShouldPushChanges().
  bool ServiceIsPushingChanges() { return service_->ShouldPushChanges(); }

 private:
  friend class StateChangeTimeoutEvent;

  enum WaitState {
    // The sync client awaits the OnAuthError() callback.
    WAITING_FOR_ON_AUTH_ERROR = 0,
    // The sync client awaits the OnBackendInitialized() callback.
    WAITING_FOR_ON_BACKEND_INITIALIZED,
    // The sync client is waiting for notifications_enabled to become true.
    WAITING_FOR_NOTIFICATIONS_ENABLED,
    // The sync client is waiting for an ongoing sync cycle to complete.
    WAITING_FOR_SYNC_TO_FINISH,
    // The sync client anticipates incoming updates leading to a new sync cycle.
    WAITING_FOR_UPDATES,
    // The sync client is fully synced and there are no pending updates.
    FULLY_SYNCED,
    // An authentication error has occurred.
    AUTH_ERROR,
    NUMBER_OF_STATES,
  };

  // Called from the observer when the current wait state has been completed.
  void SignalStateCompleteWithNextState(WaitState next_state);
  void SignalStateComplete();

  // Finite state machine for controlling state.  Returns true only if a state
  // change has taken place.
  bool RunStateChangeMachine();

  // Returns true if a status change took place, false on timeout.
  virtual bool AwaitStatusChangeWithTimeout(int timeout_seconds,
                                            const std::string& reason);

  // Returns true if the service initialized correctly. Set is_auth_retry to
  // true when calling this method second time after an authentication failure.
  bool WaitForServiceInit(bool is_auth_retry);

  // Logs message with relevant info about client's sync state (if available).
  void LogClientInfo(std::string message);

  WaitState wait_state_;

  Profile* profile_;
  ProfileSyncService* service_;

  // Returns a snapshot of the current sync session.
  const SyncSessionSnapshot* GetLastSessionSnapshot() const;

  // State tracking.  Used for debugging and tracking of state.
  ProfileSyncService::Status last_status_;

  // This value tracks the max sync timestamp (e.g. synced-to revision) inside
  // the sync engine.  It gets updated when a sync cycle ends and the session
  // snapshot implies syncing is "done".
  int64 last_timestamp_;

  // The minimum value of the 'max_local_timestamp' member of a
  // SyncSessionSnapshot we need to wait to observe in OnStateChanged when told
  // to WaitUntilTimestampIsAtLeast(...).
  int64 min_timestamp_needed_;

  // Credentials used for GAIA authentication.
  std::string username_;
  std::string password_;

  // Client ID, used for logging purposes.
  int id_;

  DISALLOW_COPY_AND_ASSIGN(ProfileSyncServiceTestHarness);
};

#endif  // CHROME_TEST_LIVE_SYNC_PROFILE_SYNC_SERVICE_TEST_HARNESS_H_
