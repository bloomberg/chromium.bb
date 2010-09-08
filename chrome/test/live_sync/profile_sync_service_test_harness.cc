// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/net/gaia/token_service.h"
#include "chrome/browser/sync/glue/sync_backend_host.h"
#include "chrome/browser/sync/sessions/session_state.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/net/gaia/gaia_constants.h"
#include "chrome/common/net/gaia/google_service_auth_error.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/live_sync/profile_sync_service_test_harness.h"
#include "chrome/test/ui_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

// The default value for min_timestamp_needed_ when we're not in the
// WAITING_FOR_UPDATES state.
static const int kMinTimestampNeededNone = -1;

static const ProfileSyncService::Status kInvalidStatus = {};

// Simple class to implement a timeout using PostDelayedTask.  If it is not
// aborted before picked up by a message queue, then it asserts with the message
// provided.  This class is not thread safe.
class StateChangeTimeoutEvent
    : public base::RefCountedThreadSafe<StateChangeTimeoutEvent> {
 public:
  explicit StateChangeTimeoutEvent(ProfileSyncServiceTestHarness* caller,
                                   const std::string& message);

  // The entry point to the class from PostDelayedTask.
  void Callback();

  // Cancels the actions of the callback.  Returns true if success, false
  // if the callback has already timed out.
  bool Abort();

 private:
  friend class base::RefCountedThreadSafe<StateChangeTimeoutEvent>;

  ~StateChangeTimeoutEvent();

  bool aborted_;
  bool did_timeout_;

  // Due to synchronization of the IO loop, the caller will always be alive
  // if the class is not aborted.
  ProfileSyncServiceTestHarness* caller_;

  // Informative message to assert in the case of a timeout.
  std::string message_;

  DISALLOW_COPY_AND_ASSIGN(StateChangeTimeoutEvent);
};

StateChangeTimeoutEvent::StateChangeTimeoutEvent(
    ProfileSyncServiceTestHarness* caller,
    const std::string& message)
    : aborted_(false), did_timeout_(false), caller_(caller), message_(message) {
}

StateChangeTimeoutEvent::~StateChangeTimeoutEvent() {
}

void StateChangeTimeoutEvent::Callback() {
  if (!aborted_) {
    if (!caller_->RunStateChangeMachine()) {
      // Report the message.
      did_timeout_ = true;
      EXPECT_FALSE(aborted_) << message_;
      caller_->SignalStateComplete();
    }
  }
}

bool StateChangeTimeoutEvent::Abort() {
  aborted_ = true;
  caller_ = NULL;
  return !did_timeout_;
}

ProfileSyncServiceTestHarness::ProfileSyncServiceTestHarness(Profile* p,
    const std::string& username, const std::string& password, int id)
    : wait_state_(WAITING_FOR_ON_BACKEND_INITIALIZED),
      profile_(p), service_(NULL),
      last_status_(kInvalidStatus),
      last_timestamp_(0),
      min_timestamp_needed_(kMinTimestampNeededNone),
      username_(username), password_(password), id_(id) {
  // Ensure the profile has enough prefs registered for use by sync.
  if (!p->GetPrefs()->FindPreference(prefs::kAcceptLanguages))
    TabContents::RegisterUserPrefs(p->GetPrefs());
}

bool ProfileSyncServiceTestHarness::SetupSync() {
  service_ = profile_->GetProfileSyncService("");
  service_->AddObserver(this);
  service_->signin_.StartSignIn(username_, password_, "", "");

  return WaitForServiceInit();
}

void ProfileSyncServiceTestHarness::SignalStateCompleteWithNextState(
    WaitState next_state) {

  wait_state_ = next_state;
  SignalStateComplete();
}

void ProfileSyncServiceTestHarness::SignalStateComplete() {
  MessageLoopForUI::current()->Quit();
}

bool ProfileSyncServiceTestHarness::RunStateChangeMachine() {
  WaitState state = wait_state_;
  ProfileSyncService::Status status(service_->QueryDetailedSyncStatus());
  switch (wait_state_) {
    case WAITING_FOR_ON_BACKEND_INITIALIZED: {
      LogClientInfo("WAITING_FOR_ON_BACKEND_INITIALIZED");
      if (service_->GetAuthError().state() != GoogleServiceAuthError::NONE) {
        SignalStateCompleteWithNextState(AUTH_ERROR);
      }
      if (service_->sync_initialized()) {
        SignalStateCompleteWithNextState(WAITING_FOR_NOTIFICATIONS_ENABLED);
      }
      break;
    }
    case WAITING_FOR_NOTIFICATIONS_ENABLED: {
      LogClientInfo("WAITING_FOR_NOTIFICATIONS_ENABLED");
      if (status.notifications_enabled) {
        SignalStateCompleteWithNextState(FULLY_SYNCED);
      }
      break;
    }
    case WAITING_FOR_SERVER_REACHABLE: {
      LogClientInfo("WAITING_FOR_SERVER_REACHABLE");
      const SyncSessionSnapshot* snap = GetLastSessionSnapshot();
      if (!status.server_reachable) {
        break;
      }
      if (service()->backend()->HasUnsyncedItems() ||
          snap->has_more_to_sync || snap->unsynced_count != 0) {
        SignalStateCompleteWithNextState(WAITING_FOR_SYNC_TO_FINISH);
        break;
      }
      last_timestamp_ = snap->max_local_timestamp;
      SignalStateCompleteWithNextState(FULLY_SYNCED);
      break;
    }
    case WAITING_FOR_SYNC_TO_FINISH: {
      LogClientInfo("WAITING_FOR_SYNC_TO_FINISH");
      const SyncSessionSnapshot* snap = GetLastSessionSnapshot();
      DCHECK(snap) << "Should have been at least one sync session by now";
      // TODO(rsimha): In an ideal world, snap->has_more_to_sync == false should
      // be a sufficient condition for sync to have completed. However, the
      // additional check of snap->unsynced_count is required due to
      // http://crbug.com/48989.
      if (service()->backend()->HasUnsyncedItems() ||
          snap->has_more_to_sync || snap->unsynced_count != 0) {
        if (!status.server_reachable)
          SignalStateCompleteWithNextState(WAITING_FOR_SERVER_REACHABLE);
        break;
      }
      EXPECT_LE(last_timestamp_, snap->max_local_timestamp);
      last_timestamp_ = snap->max_local_timestamp;
      SignalStateCompleteWithNextState(FULLY_SYNCED);
      break;
    }
    case WAITING_FOR_UPDATES: {
      LogClientInfo("WAITING_FOR_UPDATES");
      const SyncSessionSnapshot* snap = GetLastSessionSnapshot();
      DCHECK(snap) << "Should have been at least one sync session by now";
      if (snap->max_local_timestamp < min_timestamp_needed_) {
        break;
      }
      EXPECT_LE(last_timestamp_, snap->max_local_timestamp);
      last_timestamp_ = snap->max_local_timestamp;
      SignalStateCompleteWithNextState(FULLY_SYNCED);
      break;
    }
    case FULLY_SYNCED: {
      LogClientInfo("FULLY_SYNCED");
      break;
    }
    case AUTH_ERROR: {
      LogClientInfo("AUTH_ERROR");
      break;
    }
    default:
      // Invalid state during observer callback which may be triggered by other
      // classes using the the UI message loop.  Defer to their handling.
      break;
  }
  last_status_ = status;
  return state != wait_state_;
}

void ProfileSyncServiceTestHarness::OnStateChanged() {
  RunStateChangeMachine();
}

bool ProfileSyncServiceTestHarness::AwaitSyncCycleCompletion(
    const std::string& reason) {
  LogClientInfo("AwaitSyncCycleCompletion");
  const SyncSessionSnapshot* snap = GetLastSessionSnapshot();
  DCHECK(snap) << "Should have been at least one sync session by now";
  // TODO(rsimha): Remove additional checks of snap->has_more_to_sync and
  // snap->unsynced_count once http://crbug.com/48989 is fixed.
  if (service()->backend()->HasUnsyncedItems() ||
      snap->has_more_to_sync ||
      snap->unsynced_count != 0) {
    wait_state_ = WAITING_FOR_SYNC_TO_FINISH;
    return AwaitStatusChangeWithTimeout(60, reason);
  } else {
    EXPECT_LE(last_timestamp_, snap->max_local_timestamp);
    last_timestamp_ = snap->max_local_timestamp;
    return true;
  }
}

bool ProfileSyncServiceTestHarness::AwaitMutualSyncCycleCompletion(
    ProfileSyncServiceTestHarness* partner) {
  LogClientInfo("AwaitMutualSyncCycleCompletion");
  bool success = AwaitSyncCycleCompletion(
      "Sync cycle completion on active client.");
  if (!success)
    return false;
  return partner->WaitUntilTimestampIsAtLeast(last_timestamp_,
      "Sync cycle completion on passive client.");
}

bool ProfileSyncServiceTestHarness::AwaitGroupSyncCycleCompletion(
    std::vector<ProfileSyncServiceTestHarness*>& partners) {
  LogClientInfo("AwaitGroupSyncCycleCompletion");
  bool success = AwaitSyncCycleCompletion(
      "Sync cycle completion on active client.");
  if (!success)
    return false;
  bool return_value = true;
  for (std::vector<ProfileSyncServiceTestHarness*>::iterator it =
      partners.begin(); it != partners.end(); ++it) {
    if (this != *it) {
      return_value = return_value &&
          (*it)->WaitUntilTimestampIsAtLeast(last_timestamp_,
          "Sync cycle completion on partner client.");
    }
  }
  return return_value;
}

// static
bool ProfileSyncServiceTestHarness::AwaitQuiescence(
    std::vector<ProfileSyncServiceTestHarness*>& clients) {
  LOG(INFO) << "AwaitQuiescence.";
  bool return_value = true;
  for (std::vector<ProfileSyncServiceTestHarness*>::iterator it =
      clients.begin(); it != clients.end(); ++it) {
    return_value = return_value &&
        (*it)->AwaitGroupSyncCycleCompletion(clients);
  }
  return return_value;
}

bool ProfileSyncServiceTestHarness::WaitUntilTimestampIsAtLeast(
    int64 timestamp, const std::string& reason) {
  LogClientInfo("WaitUntilTimestampIsAtLeast");
  min_timestamp_needed_ = timestamp;
  const SyncSessionSnapshot* snap = GetLastSessionSnapshot();
  DCHECK(snap) << "Should have been at least one sync session by now";
  if (snap->max_local_timestamp < min_timestamp_needed_) {
    wait_state_ = WAITING_FOR_UPDATES;
    return AwaitStatusChangeWithTimeout(60, reason);
  } else {
    return true;
  }
}

bool ProfileSyncServiceTestHarness::AwaitStatusChangeWithTimeout(
    int timeout_seconds,
    const std::string& reason) {
  LogClientInfo("AwaitStatusChangeWithTimeout");
  scoped_refptr<StateChangeTimeoutEvent> timeout_signal(
      new StateChangeTimeoutEvent(this, reason));
  MessageLoopForUI* loop = MessageLoopForUI::current();
  loop->PostDelayedTask(
      FROM_HERE,
      NewRunnableMethod(timeout_signal.get(),
                        &StateChangeTimeoutEvent::Callback),
      1000 * timeout_seconds);
  LogClientInfo("Before RunMessageLoop");
  ui_test_utils::RunMessageLoop();
  LogClientInfo("After RunMessageLoop");
  return timeout_signal->Abort();
}

bool ProfileSyncServiceTestHarness::WaitForServiceInit() {
  LogClientInfo("WaitForServiceInit");

  // Wait for the OnBackendInitialized() callback.
  EXPECT_EQ(wait_state_, WAITING_FOR_ON_BACKEND_INITIALIZED);
  EXPECT_TRUE(AwaitStatusChangeWithTimeout(30,
      "Waiting for OnBackendInitialized().")) <<
      "OnBackendInitialized() not seen after 30 seconds.";

  if (wait_state_ == AUTH_ERROR) {
    return false;
  }

  // Choose datatypes to be synced.
  syncable::ModelTypeSet set;
  for (int i = syncable::FIRST_REAL_MODEL_TYPE;
      i < syncable::MODEL_TYPE_COUNT; ++i) {
    set.insert(syncable::ModelTypeFromInt(i));
  }
  service_->OnUserChoseDatatypes(true, set);

  // Wait for notifications_enabled to be set to true.
  EXPECT_EQ(wait_state_, WAITING_FOR_NOTIFICATIONS_ENABLED);
  EXPECT_TRUE(AwaitStatusChangeWithTimeout(30,
      "Waiting for notifications_enabled to be set to true.")) <<
      "notifications_enabled not set to true after 30 seconds.";

  return true;
}

const SyncSessionSnapshot*
    ProfileSyncServiceTestHarness::GetLastSessionSnapshot() const {
  EXPECT_FALSE(service_ == NULL) << "Sync service has not yet been set up.";
  if (service_->backend()) {
    return service_->backend()->GetLastSessionSnapshot();
  }
  return NULL;
}

void ProfileSyncServiceTestHarness::LogClientInfo(std::string message) {
  const SyncSessionSnapshot* snap = GetLastSessionSnapshot();
  if (snap) {
    LOG(INFO) << "Client " << id_ << ": " << message << ": "
        << "has_more_to_sync: " << snap->has_more_to_sync
        << ", max_local_timestamp: " << snap->max_local_timestamp
        << ", unsynced_count: " << snap->unsynced_count
        << ", has_unsynced_items: " << service()->backend()->HasUnsyncedItems()
        << ".";
  } else {
    LOG(INFO) << "Client " << id_ << ": " << message << ": "
        << "Snap not available.";
  }
}
