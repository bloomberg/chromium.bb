// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/net/gaia/token_service.h"
#include "chrome/browser/sync/profile_sync_service_harness.h"
#include "chrome/browser/sync/glue/sync_backend_host.h"
#include "chrome/browser/sync/sessions/session_state.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/net/gaia/gaia_constants.h"
#include "chrome/common/net/gaia/google_service_auth_error.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/test_timeouts.h"
#include "chrome/test/ui_test_utils.h"

// The default value for min_timestamp_needed_ when we're not in the
// WAITING_FOR_UPDATES state.
static const int kMinTimestampNeededNone = -1;

// Simple class to implement a timeout using PostDelayedTask.  If it is not
// aborted before picked up by a message queue, then it asserts with the message
// provided.  This class is not thread safe.
class StateChangeTimeoutEvent
    : public base::RefCountedThreadSafe<StateChangeTimeoutEvent> {
 public:
  explicit StateChangeTimeoutEvent(ProfileSyncServiceHarness* caller,
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
  ProfileSyncServiceHarness* caller_;

  // Informative message to assert in the case of a timeout.
  std::string message_;

  DISALLOW_COPY_AND_ASSIGN(StateChangeTimeoutEvent);
};

StateChangeTimeoutEvent::StateChangeTimeoutEvent(
    ProfileSyncServiceHarness* caller,
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
      DCHECK(!aborted_) << message_;
      caller_->SignalStateComplete();
    }
  }
}

bool StateChangeTimeoutEvent::Abort() {
  aborted_ = true;
  caller_ = NULL;
  return !did_timeout_;
}

ProfileSyncServiceHarness::ProfileSyncServiceHarness(Profile* p,
    const std::string& username, const std::string& password, int id)
    : wait_state_(WAITING_FOR_ON_BACKEND_INITIALIZED),
      profile_(p), service_(NULL),
      last_timestamp_(0),
      min_timestamp_needed_(kMinTimestampNeededNone),
      username_(username), password_(password), id_(id) {}

bool ProfileSyncServiceHarness::SetupSync() {
  service_ = profile_->GetProfileSyncService("");
  DCHECK(service_ != NULL) << "SetupSync(): service_ is null.";
  service_->AddObserver(this);
  service_->signin()->StartSignIn(username_, password_, "", "");
  return WaitForServiceInit();
}

void ProfileSyncServiceHarness::SignalStateCompleteWithNextState(
    WaitState next_state) {
  wait_state_ = next_state;
  SignalStateComplete();
}

void ProfileSyncServiceHarness::SignalStateComplete() {
  MessageLoopForUI::current()->Quit();
}

bool ProfileSyncServiceHarness::RunStateChangeMachine() {
  WaitState original_wait_state = wait_state_;
  switch (wait_state_) {
    case WAITING_FOR_ON_BACKEND_INITIALIZED: {
      LogClientInfo("WAITING_FOR_ON_BACKEND_INITIALIZED");
      if (service()->sync_initialized()) {
        // The sync backend is initialized. Start waiting for the first sync
        // cycle to complete.
        SignalStateCompleteWithNextState(WAITING_FOR_INITIAL_SYNC);
      }
      break;
    }
    case WAITING_FOR_INITIAL_SYNC: {
      LogClientInfo("WAITING_FOR_INITIAL_SYNC");
      if (IsSynced()) {
        // The first sync cycle is now complete. We can start running tests.
        SignalStateCompleteWithNextState(FULLY_SYNCED);
      }
      break;
    }
    case WAITING_FOR_SYNC_TO_FINISH: {
      LogClientInfo("WAITING_FOR_SYNC_TO_FINISH");
      if (!IsSynced()) {
        // The client is not yet fully synced. Continue waiting.
        if (!GetStatus().server_reachable) {
          // The client cannot reach the sync server because the network is
          // disabled. There is no need to wait anymore.
          SignalStateCompleteWithNextState(SERVER_UNREACHABLE);
        }
        break;
      }
      GetUpdatedTimestamp();
      SignalStateCompleteWithNextState(FULLY_SYNCED);
      break;
    }
    case WAITING_FOR_UPDATES: {
      LogClientInfo("WAITING_FOR_UPDATES");
      if (!IsSynced() || GetUpdatedTimestamp() < min_timestamp_needed_) {
        // The client is not yet fully synced. Continue waiting until the client
        // is at the required minimum timestamp.
        break;
      }
      SignalStateCompleteWithNextState(FULLY_SYNCED);
      break;
    }
    case SERVER_UNREACHABLE: {
      LogClientInfo("SERVER_UNREACHABLE");
      if (GetStatus().server_reachable) {
        // The client was offline due to the network being disabled, but is now
        // back online. Wait for the pending sync cycle to complete.
        SignalStateCompleteWithNextState(WAITING_FOR_SYNC_TO_FINISH);
      }
      break;
    }
    case FULLY_SYNCED: {
      // The client is online and fully synced. There is nothing to do.
      LogClientInfo("FULLY_SYNCED");
      break;
    }
    default:
      // Invalid state during observer callback which may be triggered by other
      // classes using the the UI message loop.  Defer to their handling.
      break;
  }
  return original_wait_state != wait_state_;
}

void ProfileSyncServiceHarness::OnStateChanged() {
  RunStateChangeMachine();
}

bool ProfileSyncServiceHarness::AwaitSyncCycleCompletion(
    const std::string& reason) {
  LogClientInfo("AwaitSyncCycleCompletion");
  if (!IsSynced()) {
    if (wait_state_ == SERVER_UNREACHABLE) {
      // Client was offline; wait for it to go online, and then wait for sync.
      AwaitStatusChangeWithTimeout(
          TestTimeouts::live_operation_timeout_ms(), reason);
      DCHECK_EQ(wait_state_, WAITING_FOR_SYNC_TO_FINISH);
      return AwaitStatusChangeWithTimeout(
          TestTimeouts::live_operation_timeout_ms(), reason);
    } else {
      DCHECK(service()->sync_initialized());
      wait_state_ = WAITING_FOR_SYNC_TO_FINISH;
      AwaitStatusChangeWithTimeout(
          TestTimeouts::live_operation_timeout_ms(), reason);
      if (wait_state_ == FULLY_SYNCED) {
        // Client is online; sync was successful.
        return true;
      } else if (wait_state_ == SERVER_UNREACHABLE){
        // Client is offline; sync was unsuccessful.
        return false;
      } else {
        LOG(FATAL) << "Invalid wait state:" << wait_state_;
        return false;
      }
    }
  } else {
    // Client is already synced; don't wait.
    GetUpdatedTimestamp();
    return true;
  }
}

bool ProfileSyncServiceHarness::AwaitMutualSyncCycleCompletion(
    ProfileSyncServiceHarness* partner) {
  LogClientInfo("AwaitMutualSyncCycleCompletion");
  bool success = AwaitSyncCycleCompletion(
      "Sync cycle completion on active client.");
  if (!success)
    return false;
  return partner->WaitUntilTimestampIsAtLeast(last_timestamp_,
      "Sync cycle completion on passive client.");
}

bool ProfileSyncServiceHarness::AwaitGroupSyncCycleCompletion(
    std::vector<ProfileSyncServiceHarness*>& partners) {
  LogClientInfo("AwaitGroupSyncCycleCompletion");
  bool success = AwaitSyncCycleCompletion(
      "Sync cycle completion on active client.");
  if (!success)
    return false;
  bool return_value = true;
  for (std::vector<ProfileSyncServiceHarness*>::iterator it =
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
bool ProfileSyncServiceHarness::AwaitQuiescence(
    std::vector<ProfileSyncServiceHarness*>& clients) {
  LOG(INFO) << "AwaitQuiescence.";
  bool return_value = true;
  for (std::vector<ProfileSyncServiceHarness*>::iterator it =
      clients.begin(); it != clients.end(); ++it) {
    return_value = return_value &&
        (*it)->AwaitGroupSyncCycleCompletion(clients);
  }
  return return_value;
}

bool ProfileSyncServiceHarness::WaitUntilTimestampIsAtLeast(
    int64 timestamp, const std::string& reason) {
  LogClientInfo("WaitUntilTimestampIsAtLeast");
  min_timestamp_needed_ = timestamp;
  if (GetUpdatedTimestamp() < min_timestamp_needed_) {
    wait_state_ = WAITING_FOR_UPDATES;
    return AwaitStatusChangeWithTimeout(
        TestTimeouts::live_operation_timeout_ms(), reason);
  } else {
    return true;
  }
}

bool ProfileSyncServiceHarness::AwaitStatusChangeWithTimeout(
    int timeout_milliseconds,
    const std::string& reason) {
  LogClientInfo("AwaitStatusChangeWithTimeout");
  scoped_refptr<StateChangeTimeoutEvent> timeout_signal(
      new StateChangeTimeoutEvent(this, reason));
  MessageLoopForUI* loop = MessageLoopForUI::current();
  loop->PostDelayedTask(
      FROM_HERE,
      NewRunnableMethod(timeout_signal.get(),
                        &StateChangeTimeoutEvent::Callback),
      timeout_milliseconds);
  AwaitStatusChange();
  LogClientInfo("AwaitStatusChangeWithTimeout succeeded");
  return timeout_signal->Abort();
}

void ProfileSyncServiceHarness::AwaitStatusChange() {
  ui_test_utils::RunMessageLoop();
}

bool ProfileSyncServiceHarness::WaitForServiceInit() {
  LogClientInfo("WaitForServiceInit");

  // Wait for the OnBackendInitialized() callback.
  DCHECK_EQ(wait_state_, WAITING_FOR_ON_BACKEND_INITIALIZED);
  if (!AwaitStatusChangeWithTimeout(TestTimeouts::live_operation_timeout_ms(),
      "Waiting for OnBackendInitialized().")) {
    LOG(FATAL) << "OnBackendInitialized() not seen after "
               << TestTimeouts::live_operation_timeout_ms() / 1000
               << " seconds.";
  }

  // Choose datatypes to be synced.
  syncable::ModelTypeSet synced_datatypes;
  for (int i = syncable::FIRST_REAL_MODEL_TYPE;
      i < syncable::MODEL_TYPE_COUNT; ++i) {
    synced_datatypes.insert(syncable::ModelTypeFromInt(i));
  }
  service()->OnUserChoseDatatypes(true, synced_datatypes);

  // Wait for initial sync cycle to complete.
  DCHECK_EQ(wait_state_, WAITING_FOR_INITIAL_SYNC);
  if (!AwaitStatusChangeWithTimeout(TestTimeouts::live_operation_timeout_ms(),
      "Waiting for initial sync cycle to complete.")) {
    LOG(FATAL) << "Initial sync cycle did not complete after "
               << TestTimeouts::live_operation_timeout_ms() / 1000
               << " seconds.";
  }

  return true;
}

ProfileSyncService::Status ProfileSyncServiceHarness::GetStatus() {
  DCHECK(service() != NULL) << "GetStatus(): service() is NULL.";
  return service()->QueryDetailedSyncStatus();
}

bool ProfileSyncServiceHarness::IsSynced() {
  const SyncSessionSnapshot* snap = GetLastSessionSnapshot();
  // TODO(rsimha): Remove additional checks of snap->has_more_to_sync and
  // snap->unsynced_count once http://crbug.com/48989 is fixed.
  return (service() &&
          snap &&
          ServiceIsPushingChanges() &&
          GetStatus().notifications_enabled &&
          !service()->backend()->HasUnsyncedItems() &&
          !snap->has_more_to_sync &&
          snap->unsynced_count == 0);
}

const SyncSessionSnapshot*
    ProfileSyncServiceHarness::GetLastSessionSnapshot() const {
  DCHECK(service_ != NULL) << "Sync service has not yet been set up.";
  if (service_->backend()) {
    return service_->backend()->GetLastSessionSnapshot();
  }
  return NULL;
}

void ProfileSyncServiceHarness::EnableSyncForDatatype(
    syncable::ModelType datatype) {
  syncable::ModelTypeSet synced_datatypes;
  DCHECK(service() != NULL)
      << "EnableSyncForDatatype(): service() is null.";
  service()->GetPreferredDataTypes(&synced_datatypes);
  syncable::ModelTypeSet::iterator it = synced_datatypes.find(
      syncable::ModelTypeFromInt(datatype));
  if (it == synced_datatypes.end()) {
    synced_datatypes.insert(syncable::ModelTypeFromInt(datatype));
    service()->OnUserChoseDatatypes(false, synced_datatypes);
    AwaitSyncCycleCompletion("Waiting for datatype configuration.");
    LOG(INFO) << "EnableSyncForDatatype(): Enabled sync for datatype "
              << syncable::ModelTypeToString(datatype) << ".";
  } else {
    LOG(INFO) << "EnableSyncForDatatype(): Sync already enabled for datatype "
              << syncable::ModelTypeToString(datatype) << ".";
  }
}

void ProfileSyncServiceHarness::DisableSyncForDatatype(
    syncable::ModelType datatype) {
  syncable::ModelTypeSet synced_datatypes;
  DCHECK(service() != NULL)
      << "DisableSyncForDatatype(): service() is null.";
  service()->GetPreferredDataTypes(&synced_datatypes);
  syncable::ModelTypeSet::iterator it = synced_datatypes.find(
      syncable::ModelTypeFromInt(datatype));
  if (it != synced_datatypes.end()) {
    synced_datatypes.erase(it);
    service()->OnUserChoseDatatypes(false, synced_datatypes);
    AwaitSyncCycleCompletion("Waiting for datatype configuration.");
    LOG(INFO) << "DisableSyncForDatatype(): Disabled sync for datatype "
              << syncable::ModelTypeToString(datatype) << ".";
  } else {
    LOG(INFO) << "DisableSyncForDatatype(): Sync already disabled for datatype "
              << syncable::ModelTypeToString(datatype) << ".";
  }
}

void ProfileSyncServiceHarness::EnableSyncForAllDatatypes() {
  syncable::ModelTypeSet synced_datatypes;
  for (int i = syncable::FIRST_REAL_MODEL_TYPE;
      i < syncable::MODEL_TYPE_COUNT; ++i) {
      synced_datatypes.insert(syncable::ModelTypeFromInt(i));
  }
  DCHECK(service() != NULL)
      << "EnableSyncForAllDatatypes(): service() is null.";
  service()->OnUserChoseDatatypes(true, synced_datatypes);
  AwaitSyncCycleCompletion("Waiting for datatype configuration.");
  LOG(INFO) << "EnableSyncForAllDatatypes(): Enabled sync for all datatypes.";
}

int64 ProfileSyncServiceHarness::GetUpdatedTimestamp() {
  const SyncSessionSnapshot* snap = GetLastSessionSnapshot();
  DCHECK(snap != NULL) << "GetUpdatedTimestamp(): Sync snapshot is NULL.";
  DCHECK_LE(last_timestamp_, snap->max_local_timestamp);
  last_timestamp_ = snap->max_local_timestamp;
  return last_timestamp_;
}

void ProfileSyncServiceHarness::LogClientInfo(std::string message) {
  const SyncSessionSnapshot* snap = GetLastSessionSnapshot();
  if (snap) {
    LOG(INFO) << "Client " << id_ << ": " << message << ": "
        << "max_local_timestamp: " << snap->max_local_timestamp
        << ", has_more_to_sync: " << snap->has_more_to_sync
        << ", unsynced_count: " << snap->unsynced_count
        << ", has_unsynced_items: " << service()->backend()->HasUnsyncedItems()
        << ", notifications_enabled: " << GetStatus().notifications_enabled
        << ", service_is_pushing_changes: " << ServiceIsPushingChanges()
        << ".";
  } else {
    LOG(INFO) << "Client " << id_ << ": " << message << ": "
        << "Snap not available.";
  }
}
