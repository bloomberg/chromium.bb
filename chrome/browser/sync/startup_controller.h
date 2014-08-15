// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_STARTUP_CONTROLLER_H_
#define CHROME_BROWSER_SYNC_STARTUP_CONTROLLER_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "sync/internal_api/public/base/model_type.h"

class ProfileOAuth2TokenService;
class SupervisedUserSigninManagerWrapper;

namespace sync_driver {
class SyncPrefs;
}

namespace browser_sync {

// Defines the type of behavior the sync engine should use. If configured for
// AUTO_START, the sync engine will automatically call SetSyncSetupCompleted()
// and start downloading data types as soon as sync credentials are available.
// If configured for MANUAL_START, sync will not start until the user
// completes sync setup, at which point the UI makes an explicit call to
// complete sync setup.
enum ProfileSyncServiceStartBehavior {
  AUTO_START,
  MANUAL_START,
};

// This class is used by ProfileSyncService to manage all logic and state
// pertaining to initialization of the SyncBackendHost (colloquially referred
// to as "the backend").
class StartupController {
 public:
  StartupController(ProfileSyncServiceStartBehavior start_behavior,
                    const ProfileOAuth2TokenService* token_service,
                    const sync_driver::SyncPrefs* sync_prefs,
                    const SupervisedUserSigninManagerWrapper* signin,
                    base::Closure start_backend);
  ~StartupController();

  // Starts up sync if it is not suppressed and preconditions are met.
  // Returns true if these preconditions are met, although does not imply
  // the backend was started.
  bool TryStart();

  // Called when a datatype (SyncableService) has a need for sync to start
  // ASAP, presumably because a local change event has occurred but we're
  // still in deferred start mode, meaning the SyncableService hasn't been
  // told to MergeDataAndStartSyncing yet.
  // It is expected that |type| is a currently active datatype.
  void OnDataTypeRequestsSyncStartup(syncer::ModelType type);

  // Prepares this object for a new attempt to start sync, forgetting
  // whether or not preconditions were previously met.
  // NOTE: This resets internal state managed by this class, but does not
  // touch values that are explicitly set and reset by higher layers to
  // tell this class whether a setup UI dialog is being shown to the user.
  // See setup_in_progress_.
  void Reset(const syncer::ModelTypeSet registered_types);

  void set_setup_in_progress(bool in_progress);
  bool setup_in_progress() const { return setup_in_progress_; }
  bool auto_start_enabled() const { return auto_start_enabled_; }
  base::Time start_backend_time() const { return start_backend_time_; }
  std::string GetBackendInitializationStateString() const;

  void OverrideFallbackTimeoutForTest(const base::TimeDelta& timeout);
 private:
  enum StartUpDeferredOption {
    STARTUP_BACKEND_DEFERRED,
    STARTUP_IMMEDIATE
  };
  // Returns true if all conditions to start the backend are met.
  bool StartUp(StartUpDeferredOption deferred_option);
  void OnFallbackStartupTimerExpired();

  // Records time spent in deferred state with UMA histograms.
  void RecordTimeDeferred();

  // True if we should start sync ASAP because either a SyncableService has
  // requested it, or we're done waiting for a sign and decided to go ahead.
  bool received_start_request_;

  // The time that StartUp() is called. This is used to calculate time spent
  // in the deferred state; that is, after StartUp and before invoking the
  // start_backend_ callback.
  base::Time start_up_time_;

  // If |true|, there is setup UI visible so we should not start downloading
  // data types.
  // Note: this is explicitly controlled by higher layers (UI) and is meant to
  // reflect what the UI claims the setup state to be. Therefore, only set this
  // due to explicit requests to do so via set_setup_in_progress.
  bool setup_in_progress_;

  // If true, we want to automatically start sync signin whenever we have
  // credentials (user doesn't need to go through the startup flow). This is
  // typically enabled on platforms (like ChromeOS) that have their own
  // distinct signin flow.
  const bool auto_start_enabled_;

  const sync_driver::SyncPrefs* sync_prefs_;

  const ProfileOAuth2TokenService* token_service_;

  const SupervisedUserSigninManagerWrapper* signin_;

  // The callback we invoke when it's time to call expensive
  // startup routines for the sync backend.
  base::Closure start_backend_;

  // The time at which we invoked the start_backend_ callback.
  base::Time start_backend_time_;

  base::TimeDelta fallback_timeout_;

  // Used to compute preferred_types from SyncPrefs as-needed.
  syncer::ModelTypeSet registered_types_;

  // True before calling |start_backend_| for the first time. False after that.
  bool first_start_;

  base::WeakPtrFactory<StartupController> weak_factory_;
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_STARTUP_CONTROLLER_H_
