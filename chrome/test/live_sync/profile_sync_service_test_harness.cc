// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/pref_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/sync/glue/sync_backend_host.h"
#include "chrome/browser/sync/sessions/session_state.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/live_sync/profile_sync_service_test_harness.h"
#include "chrome/test/ui_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

using browser_sync::sessions::SyncSessionSnapshot;

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

ProfileSyncServiceTestHarness::ProfileSyncServiceTestHarness(
    Profile* p, const std::string& username, const std::string& password)
    : wait_state_(WAITING_FOR_ON_AUTH_ERROR), profile_(p), service_(NULL),
      last_status_(kInvalidStatus),
      last_timestamp_(0),
      min_timestamp_needed_(kMinTimestampNeededNone),
      username_(username), password_(password) {
  // Ensure the profile has enough prefs registered for use by sync.
  if (!p->GetPrefs()->FindPreference(prefs::kAcceptLanguages))
    TabContents::RegisterUserPrefs(p->GetPrefs());
}

bool ProfileSyncServiceTestHarness::SetupSync() {
  service_ = profile_->GetProfileSyncService();
  service_->StartUp();
  service_->AddObserver(this);
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
    case WAITING_FOR_ON_AUTH_ERROR: {
      SignalStateCompleteWithNextState(WAITING_FOR_ON_BACKEND_INITIALIZED);
      break;
    }
    case WAITING_FOR_ON_BACKEND_INITIALIZED: {
      if (service_->sync_initialized()) {
        SignalStateCompleteWithNextState(WAITING_FOR_NOTIFICATIONS_ENABLED);
      }
      break;
    }
    case WAITING_FOR_NOTIFICATIONS_ENABLED: {
      if (status.notifications_enabled) {
        SignalStateCompleteWithNextState(FULLY_SYNCED);
      }
      break;
    }
    case WAITING_FOR_SYNC_TO_FINISH: {
      const SyncSessionSnapshot* snap =
          service_->backend()->GetLastSessionSnapshot();
      DCHECK(snap) << "Should have been at least one sync session by now";
      // TODO(rsimha): In an ideal world, snap->has_more_to_sync == false should
      // be a sufficient condition for sync to have completed. However, the
      // additional check of snap->unsynced_count is required due to
      // http://crbug.com/48989.
      if (snap->has_more_to_sync || snap->unsynced_count != 0) {
        break;
      }

      EXPECT_LE(last_timestamp_, snap->max_local_timestamp);
      last_timestamp_ = snap->max_local_timestamp;
      SignalStateCompleteWithNextState(FULLY_SYNCED);
      break;
    }
    case WAITING_FOR_UPDATES: {
      const SyncSessionSnapshot* snap =
          service_->backend()->GetLastSessionSnapshot();
      DCHECK(snap) << "Should have been at least one sync session by now";
      if (snap->max_local_timestamp < min_timestamp_needed_)
        break;

      SignalStateCompleteWithNextState(FULLY_SYNCED);
      break;
    }
    case FULLY_SYNCED:
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
  wait_state_ = WAITING_FOR_SYNC_TO_FINISH;
  return AwaitStatusChangeWithTimeout(60, reason);
}

bool ProfileSyncServiceTestHarness::AwaitMutualSyncCycleCompletion(
    ProfileSyncServiceTestHarness* partner) {
  bool success = AwaitSyncCycleCompletion(
      "Sync cycle completion on active client.");
  if (!success)
    return false;
  return partner->WaitUntilTimestampIsAtLeast(last_timestamp_,
      "Sync cycle completion on passive client.");
}

bool ProfileSyncServiceTestHarness::AwaitGroupSyncCycleCompletion(
    std::vector<ProfileSyncServiceTestHarness*>& partners) {
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

bool ProfileSyncServiceTestHarness::WaitUntilTimestampIsAtLeast(
    int64 timestamp, const std::string& reason) {
  min_timestamp_needed_ = timestamp;
  const SyncSessionSnapshot* snap =
      service_->backend()->GetLastSessionSnapshot();
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
  scoped_refptr<StateChangeTimeoutEvent> timeout_signal(
      new StateChangeTimeoutEvent(this, reason));
  MessageLoopForUI* loop = MessageLoopForUI::current();
  loop->PostDelayedTask(
      FROM_HERE,
      NewRunnableMethod(timeout_signal.get(),
                        &StateChangeTimeoutEvent::Callback),
      1000 * timeout_seconds);
  ui_test_utils::RunMessageLoop();
  return timeout_signal->Abort();
}

bool ProfileSyncServiceTestHarness::WaitForServiceInit() {
  // Wait for the OnAuthError() callback.
  EXPECT_EQ(wait_state_, WAITING_FOR_ON_AUTH_ERROR);
  if (!AwaitStatusChangeWithTimeout(30,
      "Waiting for the OnAuthError() callback.")) {
    return false;
  }

  // Enter GAIA credentials and wait for the OnBackendInitialized() callback.
  service_->backend()->Authenticate(username_, password_, std::string());
  EXPECT_EQ(wait_state_, WAITING_FOR_ON_BACKEND_INITIALIZED);
  if (!AwaitStatusChangeWithTimeout(30,
      "Waiting for OnBackendInitialized().")) {
    return false;
  }

  // Choose datatypes to be synced and wait for notifications_enabled to be set
  // to true.
  syncable::ModelTypeSet set;
  for (int i = syncable::FIRST_REAL_MODEL_TYPE;
      i < syncable::MODEL_TYPE_COUNT; ++i) {
    set.insert(syncable::ModelTypeFromInt(i));
  }
  service_->OnUserChoseDatatypes(true, set);
  EXPECT_EQ(wait_state_, WAITING_FOR_NOTIFICATIONS_ENABLED);
  if (!AwaitStatusChangeWithTimeout(30,
      "Waiting for notifications_enabled to be set to true.")) {
    return false;
  }

  return true;
}
