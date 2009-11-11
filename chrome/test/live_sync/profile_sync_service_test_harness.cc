// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/test/live_sync/profile_sync_service_test_harness.h"
#include "chrome/test/ui_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

// The default value for min_updates_needed_ when we're not in a call to
// WaitForUpdatesRecievedAtLeast.
static const int kMinUpdatesNeededNone = -1;

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

class ConflictTimeoutEvent
    : public base::RefCountedThreadSafe<ConflictTimeoutEvent> {
 public:
  explicit ConflictTimeoutEvent(ProfileSyncServiceTestHarness* caller)
      : caller_(caller), did_run_(false) {
  }

  // The entry point to the class from PostDelayedTask.
  void Callback();
  bool did_run_;

 private:
  friend class base::RefCountedThreadSafe<ConflictTimeoutEvent>;

  ~ConflictTimeoutEvent() { }

  // Due to synchronization of the IO loop, the caller will always be alive
  // if the class is not aborted.
  ProfileSyncServiceTestHarness* caller_;

  DISALLOW_COPY_AND_ASSIGN(ConflictTimeoutEvent);
};

void ConflictTimeoutEvent::Callback() {
  caller_->SignalStateComplete();
  did_run_ = true;
}


ProfileSyncServiceTestHarness::ProfileSyncServiceTestHarness(
    Profile* p, const std::string& username, const std::string& password)
    : wait_state_(WAITING_FOR_INITIAL_CALLBACK), profile_(p), service_(NULL),
      last_status_(kInvalidStatus), min_updates_needed_(kMinUpdatesNeededNone),
      username_(username), password_(password) {
  // Ensure the profile has enough prefs registered for use by sync.
  if (!p->GetPrefs()->IsPrefRegistered(prefs::kAcceptLanguages))
    TabContents::RegisterUserPrefs(p->GetPrefs());
  if (!p->GetPrefs()->IsPrefRegistered(prefs::kCookieBehavior))
    Browser::RegisterUserPrefs(p->GetPrefs());
}

bool ProfileSyncServiceTestHarness::SetupSync() {
  service_ = profile_->GetProfileSyncService();
  service_->SetSyncSetupCompleted();
  service_->EnableForUser();

  // Needed to avoid showing the login dialog. Well aware this is egregious.
  service_->expecting_first_run_auth_needed_event_ = false;
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
    case WAITING_FOR_INITIAL_CALLBACK:
      SignalStateCompleteWithNextState(WAITING_FOR_READY_TO_PROCESS_CHANGES);
      break;
    case WAITING_FOR_READY_TO_PROCESS_CHANGES:
      if (service_->ShouldPushChanges()) {
        SignalStateCompleteWithNextState(WAITING_FOR_NOTHING);
      }
      break;
    case WAITING_FOR_TIMESTAMP_UPDATE: {
      const base::Time current_timestamp = service_->last_synced_time();
      if (current_timestamp == last_timestamp_) {
        break;
      }
      EXPECT_TRUE(last_timestamp_ < current_timestamp);
      last_timestamp_ = current_timestamp;
      SignalStateCompleteWithNextState(WAITING_FOR_NOTHING);
      break;
    }
    case WAITING_FOR_UPDATES:
      if (status.updates_received < min_updates_needed_) {
        break;
      }
      SignalStateCompleteWithNextState(WAITING_FOR_NOTHING);
      break;
    case WAITING_FOR_NOTHING:
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
  wait_state_ = WAITING_FOR_TIMESTAMP_UPDATE;
  return AwaitStatusChangeWithTimeout(60, reason);
}

bool ProfileSyncServiceTestHarness::AwaitMutualSyncCycleCompletion(
    ProfileSyncServiceTestHarness* partner) {
  return AwaitSyncCycleCompletion("Sync cycle completion on active client.") &&
         partner->AwaitSyncCycleCompletion(
            "Sync cycle completion on passive client.");
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

bool ProfileSyncServiceTestHarness::AwaitMutualSyncCycleCompletionWithConflict(
    ProfileSyncServiceTestHarness* partner) {

  if (!AwaitMutualSyncCycleCompletion(partner)) {
    return false;
  }
  if (!partner->AwaitMutualSyncCycleCompletion(this)) {
    return false;
  }

  scoped_refptr<ConflictTimeoutEvent> timeout_signal(
      new ConflictTimeoutEvent(this));

  // Now we want to wait an extra 20 seconds to ensure any rebounding updates
  // due to a conflict are processed and observed by each client.
  MessageLoopForUI* loop = MessageLoopForUI::current();
  loop->PostDelayedTask(FROM_HERE, NewRunnableMethod(timeout_signal.get(),
      &ConflictTimeoutEvent::Callback), 1000 * 20);
  // It is possible that timeout has not run yet and loop exited due to
  // OnStateChanged event. So to avoid pre-mature termination of loop,
  // we are re-running the loop until did_run_ becomes true.
  while (!timeout_signal->did_run_) {
     ui_test_utils::RunMessageLoop();
  }
  return true;
}

bool ProfileSyncServiceTestHarness::WaitForServiceInit() {
  // Wait for the initial (auth needed) callback.
  EXPECT_EQ(wait_state_, WAITING_FOR_INITIAL_CALLBACK);
  if (!AwaitStatusChangeWithTimeout(30, "Waiting for authwatcher calback.")) {
    return false;
  }

  // Wait for the OnBackendInitialized callback.
  service_->backend()->Authenticate(username_, password_);
  EXPECT_EQ(wait_state_, WAITING_FOR_READY_TO_PROCESS_CHANGES);
  if (!AwaitStatusChangeWithTimeout(30, "Waiting on backend initialization.")) {
    return false;
  }
  return service_->sync_initialized();
}
