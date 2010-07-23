// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_LIVE_SYNC_PROFILE_SYNC_SERVICE_TEST_HARNESS_H_
#define CHROME_TEST_LIVE_SYNC_PROFILE_SYNC_SERVICE_TEST_HARNESS_H_

#include <string>
#include <vector>

#include "base/time.h"
#include "chrome/browser/sync/profile_sync_service.h"

class Profile;

// An instance of this class is basically our notion of a "sync client" for
// test purposes.  It harnesses the ProfileSyncService member of the profile
// passed to it on construction and automates certain things like setup and
// authentication.  It provides ways to "wait" adequate periods of time for
// several clients to get to the same state.
class ProfileSyncServiceTestHarness : public ProfileSyncServiceObserver {
 public:
  ProfileSyncServiceTestHarness(Profile* p, const std::string& username,
                                const std::string& password);

  // Creates a ProfileSyncService for the profile passed at construction and
  // enables sync.  Returns true only after sync has been fully initialized and
  // authenticated, and we are ready to process changes.
  bool SetupSync();

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
  bool AwaitMutualSyncCycleCompletion(ProfileSyncServiceTestHarness* partner);

  // Blocks the caller until |this| completes its ongoing sync cycle and every
  // other client in |partners| has a timestamp that is greater than or equal to
  // the timestamp of |this|.
  bool AwaitGroupSyncCycleCompletion(
      std::vector<ProfileSyncServiceTestHarness*>& partners);

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
    NUMBER_OF_STATES
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

  // Returns true if the service initialized correctly.
  bool WaitForServiceInit();

  WaitState wait_state_;

  Profile* profile_;
  ProfileSyncService* service_;

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

  DISALLOW_COPY_AND_ASSIGN(ProfileSyncServiceTestHarness);
};

#endif  // CHROME_TEST_LIVE_SYNC_PROFILE_SYNC_SERVICE_TEST_HARNESS_H_
