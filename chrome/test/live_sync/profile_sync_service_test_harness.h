// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_LIVE_SYNC_PROFILE_SYNC_SERVICE_TEST_HARNESS_H_
#define CHROME_TEST_LIVE_SYNC_PROFILE_SYNC_SERVICE_TEST_HARNESS_H_

#include <string>

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

  // Wait an extra 20 seconds to ensure any rebounding updates
  // due to a conflict are processed and observed by each client.
  bool AwaitMutualSyncCycleCompletionWithConflict(
      ProfileSyncServiceTestHarness* partner);

  // Calling this acts as a barrier and blocks the caller until |this| and
  // |partner| have both completed a sync cycle.  When calling this method,
  // the |partner| should be the passive responder who responds to the actions
  // of |this|.  This method relies upon the synchronization of callbacks
  // from the message queue. Returns true if two sync cycles have completed.
  bool AwaitMutualSyncCycleCompletion(ProfileSyncServiceTestHarness* partner);

  ProfileSyncService* service() { return service_; }

  // See ProfileSyncService::ShouldPushChanges().
  bool ServiceIsPushingChanges() { return service_->ShouldPushChanges(); }

 private:
  friend class StateChangeTimeoutEvent;
  friend class ConflictTimeoutEvent;

  enum WaitState {
    WAITING_FOR_INITIAL_CALLBACK = 0,
    WAITING_FOR_READY_TO_PROCESS_CHANGES,
    WAITING_FOR_TIMESTAMP_UPDATE,
    WAITING_FOR_UPDATES,
    WAITING_FOR_NOTHING,
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

  // When awaiting quiescence, we are waiting for a change to this value.  It
  // is set to the current (ui-threadsafe) last synced timestamp returned by
  // the sync service when AwaitQuiescence is called, and then we wait for an
  // OnStatusChanged event to observe a new, more recent last synced timestamp.
  base::Time last_timestamp_;

  // The minimum value of the 'updates_received' member of SyncManager Status
  // we need to wait to observe in OnStateChanged when told to
  // WaitForUpdatesReceivedAtLeast.
  int64 min_updates_needed_;

  // Credentials used for GAIA authentication.
  std::string username_;
  std::string password_;

  DISALLOW_COPY_AND_ASSIGN(ProfileSyncServiceTestHarness);
};

#endif  // CHROME_TEST_LIVE_SYNC_PROFILE_SYNC_SERVICE_TEST_HARNESS_H_
