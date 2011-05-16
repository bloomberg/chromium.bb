// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/profile_sync_service_harness.h"

#include <stddef.h>
#include <algorithm>
#include <iterator>
#include <ostream>
#include <set>
#include <vector>

#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop.h"
#include "base/task.h"
#include "base/tracked.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sessions/session_state.h"
#include "chrome/browser/sync/signin_manager.h"
#include "chrome/browser/sync/sync_ui_util.h"

using browser_sync::sessions::SyncSessionSnapshot;

// The amount of time for which we wait for a live sync operation to complete.
static const int kLiveSyncOperationTimeoutMs = 45000;

// Simple class to implement a timeout using PostDelayedTask.  If it is not
// aborted before picked up by a message queue, then it asserts with the message
// provided.  This class is not thread safe.
class StateChangeTimeoutEvent
    : public base::RefCountedThreadSafe<StateChangeTimeoutEvent> {
 public:
  StateChangeTimeoutEvent(ProfileSyncServiceHarness* caller,
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

ProfileSyncServiceHarness::ProfileSyncServiceHarness(
    Profile* profile,
    const std::string& username,
    const std::string& password,
    int id)
    : waiting_for_encryption_type_(syncable::UNSPECIFIED),
      wait_state_(INITIAL_WAIT_STATE),
      profile_(profile),
      service_(NULL),
      timestamp_match_partner_(NULL),
      username_(username),
      password_(password),
      id_(id) {
  if (IsSyncAlreadySetup()) {
    service_ = profile_->GetProfileSyncService();
    service_->AddObserver(this);
    wait_state_ = FULLY_SYNCED;
  }
}

// static
ProfileSyncServiceHarness* ProfileSyncServiceHarness::CreateAndAttach(
    Profile* profile) {
  if (!profile->HasProfileSyncService()) {
    NOTREACHED() << "Profile has never signed into sync.";
    return NULL;
  }
  return new ProfileSyncServiceHarness(profile, "", "", 0);
}

void ProfileSyncServiceHarness::SetCredentials(const std::string& username,
                                               const std::string& password) {
  username_ = username;
  password_ = password;
}

bool ProfileSyncServiceHarness::IsSyncAlreadySetup() {
  return profile_->HasProfileSyncService();
}

bool ProfileSyncServiceHarness::SetupSync() {
  syncable::ModelTypeSet synced_datatypes;
  for (int i = syncable::FIRST_REAL_MODEL_TYPE;
      i < syncable::MODEL_TYPE_COUNT; ++i) {
    synced_datatypes.insert(syncable::ModelTypeFromInt(i));
  }
  bool result = SetupSync(synced_datatypes);
  if (result == false) {
    std::string status = GetServiceStatus();
    LOG(ERROR) << "Client " << id_ << ": SetupSync failed. Syncer status:\n"
               << status;
  } else {
    VLOG(1) << "Client " << id_ << ": SetupSync successful.";
  }
  return result;
}

bool ProfileSyncServiceHarness::SetupSync(
    const syncable::ModelTypeSet& synced_datatypes) {
  // Initialize the sync client's profile sync service object.
  service_ = profile_->GetProfileSyncService("");
  if (service_ == NULL) {
    LOG(ERROR) << "SetupSync(): service_ is null.";
    return false;
  }

  // Subscribe sync client to notifications from the profile sync service.
  if (!service_->HasObserver(this))
    service_->AddObserver(this);

  // Authenticate sync client using GAIA credentials.
  service_->signin()->StartSignIn(username_, password_, "", "");

  // Wait for the OnBackendInitialized() callback.
  if (!AwaitBackendInitialized()) {
    LOG(ERROR) << "OnBackendInitialized() not seen after "
               << kLiveSyncOperationTimeoutMs / 1000
               << " seconds.";
    return false;
  }

  // Choose the datatypes to be synced. If all datatypes are to be synced,
  // set sync_everything to true; otherwise, set it to false.
  bool sync_everything = (synced_datatypes.size() ==
      (syncable::MODEL_TYPE_COUNT - syncable::FIRST_REAL_MODEL_TYPE));
  service()->OnUserChoseDatatypes(sync_everything, synced_datatypes);

  // Wait for initial sync cycle to be completed.
  DCHECK_EQ(wait_state_, WAITING_FOR_INITIAL_SYNC);
  if (!AwaitStatusChangeWithTimeout(kLiveSyncOperationTimeoutMs,
      "Waiting for initial sync cycle to complete.")) {
    LOG(ERROR) << "Initial sync cycle did not complete after "
               << kLiveSyncOperationTimeoutMs / 1000
               << " seconds.";
    return false;
  }

  if (wait_state_ == SET_PASSPHRASE_FAILED) {
    LOG(ERROR) << "A passphrase is required for decryption. Sync cannot proceed"
                  " until SetPassphrase is called.";
    return false;
  }

  // Indicate to the browser that sync setup is complete.
  service()->SetSyncSetupCompleted();

  return true;
}

void ProfileSyncServiceHarness::SignalStateCompleteWithNextState(
    WaitState next_state) {
  wait_state_ = next_state;
  SignalStateComplete();
}

void ProfileSyncServiceHarness::SignalStateComplete() {
  MessageLoop::current()->Quit();
}

bool ProfileSyncServiceHarness::RunStateChangeMachine() {
  WaitState original_wait_state = wait_state_;
  switch (wait_state_) {
    case WAITING_FOR_ON_BACKEND_INITIALIZED: {
      LogClientInfo("WAITING_FOR_ON_BACKEND_INITIALIZED", 1);
      if (service()->sync_initialized()) {
        // The sync backend is initialized.
        SignalStateCompleteWithNextState(WAITING_FOR_INITIAL_SYNC);
      }
      break;
    }
    case WAITING_FOR_INITIAL_SYNC: {
      LogClientInfo("WAITING_FOR_INITIAL_SYNC", 1);
      if (IsSynced()) {
        // The first sync cycle is now complete. We can start running tests.
        SignalStateCompleteWithNextState(FULLY_SYNCED);
        break;
      }
      if (service()->passphrase_required_reason() ==
              sync_api::REASON_SET_PASSPHRASE_FAILED) {
        // A passphrase is required for decryption and we don't have it. Do not
        // wait any more.
        SignalStateCompleteWithNextState(SET_PASSPHRASE_FAILED);
        break;
      }
      break;
    }
    case WAITING_FOR_SYNC_TO_FINISH: {
      LogClientInfo("WAITING_FOR_SYNC_TO_FINISH", 1);
      if (IsSynced()) {
        // The sync cycle we were waiting for is complete.
        SignalStateCompleteWithNextState(FULLY_SYNCED);
        break;
      }
      if (service()->passphrase_required_reason() ==
              sync_api::REASON_SET_PASSPHRASE_FAILED) {
        // A passphrase is required for decryption and we don't have it. Do not
        // wait any more.
        SignalStateCompleteWithNextState(SET_PASSPHRASE_FAILED);
        break;
      }
      if (!GetStatus().server_reachable) {
        // The client cannot reach the sync server because the network is
        // disabled. There is no need to wait anymore.
        SignalStateCompleteWithNextState(SERVER_UNREACHABLE);
        break;
      }
      break;
    }
    case WAITING_FOR_UPDATES: {
      LogClientInfo("WAITING_FOR_UPDATES", 1);
      DCHECK(timestamp_match_partner_);
      if (!MatchesOtherClient(timestamp_match_partner_)) {
        // The client is not yet fully synced; keep waiting until we converge.
        break;
      }
      timestamp_match_partner_->service()->RemoveObserver(this);
      timestamp_match_partner_ = NULL;

      SignalStateCompleteWithNextState(FULLY_SYNCED);
      break;
    }
    case WAITING_FOR_PASSPHRASE_REQUIRED: {
      LogClientInfo("WAITING_FOR_PASSPHRASE_REQUIRED", 1);
      if (service()->IsPassphraseRequired()) {
        // A passphrase is now required. Wait for it to be accepted.
        SignalStateCompleteWithNextState(WAITING_FOR_PASSPHRASE_ACCEPTED);
      }
      break;
    }
    case WAITING_FOR_PASSPHRASE_ACCEPTED: {
      LogClientInfo("WAITING_FOR_PASSPHRASE_ACCEPTED", 1);
      if (service()->ShouldPushChanges() &&
          !service()->IsPassphraseRequired()) {
        // The passphrase has been accepted, and sync has been restarted.
        SignalStateCompleteWithNextState(FULLY_SYNCED);
      }
      break;
    }
    case WAITING_FOR_ENCRYPTION: {
      LogClientInfo("WAITING_FOR_ENCRYPTION", 1);
      if (IsSynced() &&
          IsTypeEncrypted(waiting_for_encryption_type_) &&
          GetLastSessionSnapshot()->num_conflicting_updates == 0) {
        // Encryption is now complete for the the type in which we were waiting.
        SignalStateCompleteWithNextState(FULLY_SYNCED);
        break;
      }
      if (!GetStatus().server_reachable) {
        // The client cannot reach the sync server because the network is
        // disabled. There is no need to wait anymore.
        SignalStateCompleteWithNextState(SERVER_UNREACHABLE);
        break;
      }
      break;
    }
    case WAITING_FOR_SYNC_CONFIGURATION: {
      LogClientInfo("WAITING_FOR_SYNC_CONFIGURATION", 1);
      if (service()->ShouldPushChanges()) {
        // The Datatype manager is configured and sync is fully initialized.
        SignalStateCompleteWithNextState(FULLY_SYNCED);
      }
      break;
    }
    case SERVER_UNREACHABLE: {
      LogClientInfo("SERVER_UNREACHABLE", 1);
      if (GetStatus().server_reachable) {
        // The client was offline due to the network being disabled, but is now
        // back online. Wait for the pending sync cycle to complete.
        SignalStateCompleteWithNextState(WAITING_FOR_SYNC_TO_FINISH);
      }
      break;
    }
    case SET_PASSPHRASE_FAILED: {
      // A passphrase is required for decryption. There is nothing the sync
      // client can do until SetPassphrase() is called.
      LogClientInfo("SET_PASSPHRASE_FAILED", 1);
      break;
    }
    case FULLY_SYNCED: {
      // The client is online and fully synced. There is nothing to do.
      LogClientInfo("FULLY_SYNCED", 1);
      break;
    }
    case SYNC_DISABLED: {
      // Syncing is disabled for the client. There is nothing to do.
      LogClientInfo("SYNC_DISABLED", 1);
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

bool ProfileSyncServiceHarness::AwaitPassphraseRequired() {
  LogClientInfo("AwaitPassphraseRequired", 1);
  if (wait_state_ == SYNC_DISABLED) {
    LOG(ERROR) << "Sync disabled for Client " << id_ << ".";
    return false;
  }

  if (service()->IsPassphraseRequired()) {
    // It's already true that a passphrase is required; don't wait.
    return true;
  }

  wait_state_ = WAITING_FOR_PASSPHRASE_REQUIRED;
  return AwaitStatusChangeWithTimeout(kLiveSyncOperationTimeoutMs,
                                      "Waiting for passphrase to be required.");
}

bool ProfileSyncServiceHarness::AwaitPassphraseAccepted() {
  LogClientInfo("AwaitPassphraseAccepted", 1);
  if (wait_state_ == SYNC_DISABLED) {
    LOG(ERROR) << "Sync disabled for Client " << id_ << ".";
    return false;
  }

  if (service()->ShouldPushChanges() &&
      !service()->IsPassphraseRequired()) {
    // Passphrase is already accepted; don't wait.
    return true;
  }

  wait_state_ = WAITING_FOR_PASSPHRASE_ACCEPTED;
  return AwaitStatusChangeWithTimeout(kLiveSyncOperationTimeoutMs,
                                      "Waiting for passphrase to be accepted.");
}

bool ProfileSyncServiceHarness::AwaitBackendInitialized() {
  LogClientInfo("AwaitBackendInitialized", 1);
  if (service()->sync_initialized()) {
    // The sync backend host has already been initialized; don't wait.
    return true;
  }

  wait_state_ = WAITING_FOR_ON_BACKEND_INITIALIZED;
  return AwaitStatusChangeWithTimeout(kLiveSyncOperationTimeoutMs,
                                      "Waiting for OnBackendInitialized().");
}

bool ProfileSyncServiceHarness::AwaitSyncRestart() {
  LogClientInfo("AwaitSyncRestart", 1);
  if (service()->ShouldPushChanges()) {
    // Sync has already been restarted; don't wait.
    return true;
  }

  // Wait for the sync backend to be initialized.
  if (!AwaitBackendInitialized()) {
    LOG(ERROR) << "OnBackendInitialized() not seen after "
               << kLiveSyncOperationTimeoutMs / 1000
               << " seconds.";
    return false;
  }

  // Wait for sync configuration to complete.
  wait_state_ = WAITING_FOR_SYNC_CONFIGURATION;
  return AwaitStatusChangeWithTimeout(kLiveSyncOperationTimeoutMs,
                                      "Waiting for sync configuration.");
}

bool ProfileSyncServiceHarness::AwaitSyncCycleCompletion(
    const std::string& reason) {
  LogClientInfo("AwaitSyncCycleCompletion", 1);
  if (wait_state_ == SYNC_DISABLED) {
    LOG(ERROR) << "Sync disabled for Client " << id_ << ".";
    return false;
  }

  if (IsSynced()) {
    // Client is already synced; don't wait.
    return true;
  }

  if (wait_state_ == SERVER_UNREACHABLE) {
    // Client was offline; wait for it to go online, and then wait for sync.
    AwaitStatusChangeWithTimeout(kLiveSyncOperationTimeoutMs, reason);
    DCHECK_EQ(wait_state_, WAITING_FOR_SYNC_TO_FINISH);
    return AwaitStatusChangeWithTimeout(kLiveSyncOperationTimeoutMs, reason);
  } else {
    DCHECK(service()->sync_initialized());
    wait_state_ = WAITING_FOR_SYNC_TO_FINISH;
    AwaitStatusChangeWithTimeout(kLiveSyncOperationTimeoutMs, reason);
    if (wait_state_ == FULLY_SYNCED) {
      // Client is online; sync was successful.
      return true;
    } else if (wait_state_ == SERVER_UNREACHABLE) {
      // Client is offline; sync was unsuccessful.
      return false;
    } else {
      LOG(ERROR) << "Invalid wait state:" << wait_state_;
      return false;
    }
  }
}

bool ProfileSyncServiceHarness::AwaitMutualSyncCycleCompletion(
    ProfileSyncServiceHarness* partner) {
  LogClientInfo("AwaitMutualSyncCycleCompletion", 1);
  if (!AwaitSyncCycleCompletion("Sync cycle completion on active client."))
    return false;
  return partner->WaitUntilTimestampMatches(this,
      "Sync cycle completion on passive client.");
}

bool ProfileSyncServiceHarness::AwaitGroupSyncCycleCompletion(
    std::vector<ProfileSyncServiceHarness*>& partners) {
  LogClientInfo("AwaitGroupSyncCycleCompletion", 1);
  if (!AwaitSyncCycleCompletion("Sync cycle completion on active client."))
    return false;
  bool return_value = true;
  for (std::vector<ProfileSyncServiceHarness*>::iterator it =
      partners.begin(); it != partners.end(); ++it) {
    if ((this != *it) && ((*it)->wait_state_ != SYNC_DISABLED)) {
      return_value = return_value &&
          (*it)->WaitUntilTimestampMatches(this,
          "Sync cycle completion on partner client.");
    }
  }
  return return_value;
}

// static
bool ProfileSyncServiceHarness::AwaitQuiescence(
    std::vector<ProfileSyncServiceHarness*>& clients) {
  VLOG(1) << "AwaitQuiescence.";
  bool return_value = true;
  for (std::vector<ProfileSyncServiceHarness*>::iterator it =
      clients.begin(); it != clients.end(); ++it) {
    if ((*it)->wait_state_ != SYNC_DISABLED)
      return_value = return_value &&
          (*it)->AwaitGroupSyncCycleCompletion(clients);
  }
  return return_value;
}

bool ProfileSyncServiceHarness::WaitUntilTimestampMatches(
    ProfileSyncServiceHarness* partner, const std::string& reason) {
  LogClientInfo("WaitUntilTimestampMatches", 1);
  if (wait_state_ == SYNC_DISABLED) {
    LOG(ERROR) << "Sync disabled for Client " << id_ << ".";
    return false;
  }

  if (MatchesOtherClient(partner)) {
    // Timestamps already match; don't wait.
    return true;
  }

  DCHECK(!timestamp_match_partner_);
  timestamp_match_partner_ = partner;
  partner->service()->AddObserver(this);
  wait_state_ = WAITING_FOR_UPDATES;
  return AwaitStatusChangeWithTimeout(kLiveSyncOperationTimeoutMs, reason);
}

bool ProfileSyncServiceHarness::AwaitStatusChangeWithTimeout(
    int timeout_milliseconds,
    const std::string& reason) {
  LogClientInfo("AwaitStatusChangeWithTimeout", 1);
  if (wait_state_ == SYNC_DISABLED) {
    LOG(ERROR) << "Sync disabled for Client " << id_ << ".";
    return false;
  }
  scoped_refptr<StateChangeTimeoutEvent> timeout_signal(
      new StateChangeTimeoutEvent(this, reason));
  MessageLoop* loop = MessageLoop::current();
  bool did_allow_nestable_tasks = loop->NestableTasksAllowed();
  loop->SetNestableTasksAllowed(true);
  loop->PostDelayedTask(
      FROM_HERE,
      NewRunnableMethod(timeout_signal.get(),
                        &StateChangeTimeoutEvent::Callback),
      timeout_milliseconds);
  loop->Run();
  loop->SetNestableTasksAllowed(did_allow_nestable_tasks);
  if (timeout_signal->Abort()) {
    LogClientInfo("AwaitStatusChangeWithTimeout succeeded", 1);
    return true;
  } else {
    LogClientInfo("AwaitStatusChangeWithTimeout timed out", 0);
    return false;
  }
}

ProfileSyncService::Status ProfileSyncServiceHarness::GetStatus() {
  DCHECK(service() != NULL) << "GetStatus(): service() is NULL.";
  return service()->QueryDetailedSyncStatus();
}

bool ProfileSyncServiceHarness::IsSynced() {
  LogClientInfo("IsSynced", 1);
  if (service() == NULL)
    return false;
  const SyncSessionSnapshot* snap = GetLastSessionSnapshot();
  // TODO(rsimha): Remove additional checks of snap->has_more_to_sync and
  // snap->unsynced_count once http://crbug.com/48989 is fixed.
  return (snap &&
          snap->num_blocking_conflicting_updates == 0 &&
          ServiceIsPushingChanges() &&
          GetStatus().notifications_enabled &&
          !service()->HasUnsyncedItems() &&
          !snap->has_more_to_sync &&
          snap->unsynced_count == 0 &&
          !service()->HasPendingBackendMigration() &&
          service()->passphrase_required_reason() !=
              sync_api::REASON_SET_PASSPHRASE_FAILED);
}

bool ProfileSyncServiceHarness::MatchesOtherClient(
    ProfileSyncServiceHarness* partner) {
  if (!IsSynced())
    return false;

  // Only look for a match if we have at least one enabled datatype in
  // common with the partner client.
  syncable::ModelTypeSet types, other_types, intersection_types;
  service()->GetPreferredDataTypes(&types);
  partner->service()->GetPreferredDataTypes(&other_types);
  std::set_intersection(types.begin(), types.end(), other_types.begin(),
                        other_types.end(),
                        inserter(intersection_types,
                                 intersection_types.begin()));
  for (syncable::ModelTypeSet::iterator i = intersection_types.begin();
       i != intersection_types.end();
       ++i) {
    if (!partner->IsSynced() ||
        partner->GetUpdatedTimestamp(*i) != GetUpdatedTimestamp(*i)) {
      return false;
    }
  }
  return true;
}

const SyncSessionSnapshot*
    ProfileSyncServiceHarness::GetLastSessionSnapshot() const {
  DCHECK(service_ != NULL) << "Sync service has not yet been set up.";
  if (service_->sync_initialized()) {
    return service_->GetLastSessionSnapshot();
  }
  return NULL;
}

bool ProfileSyncServiceHarness::EnableSyncForDatatype(
    syncable::ModelType datatype) {
  LogClientInfo("EnableSyncForDatatype", 1);

  syncable::ModelTypeSet synced_datatypes;
  if (wait_state_ == SYNC_DISABLED) {
    synced_datatypes.insert(datatype);
    return SetupSync(synced_datatypes);
  }

  if (service() == NULL) {
    LOG(ERROR) << "EnableSyncForDatatype(): service() is null.";
    return false;
  }

  service()->GetPreferredDataTypes(&synced_datatypes);
  syncable::ModelTypeSet::iterator it = synced_datatypes.find(datatype);
  if (it != synced_datatypes.end()) {
    VLOG(1) << "EnableSyncForDatatype(): Sync already enabled for datatype "
            << syncable::ModelTypeToString(datatype)
            << " on Client " << id_ << ".";
    return true;
  }

  synced_datatypes.insert(syncable::ModelTypeFromInt(datatype));
  service()->OnUserChoseDatatypes(false, synced_datatypes);
  if (AwaitSyncCycleCompletion("Datatype configuration.")) {
    VLOG(1) << "EnableSyncForDatatype(): Enabled sync for datatype "
            << syncable::ModelTypeToString(datatype)
            << " on Client " << id_ << ".";
    return true;
  }

  LogClientInfo("EnableSyncForDatatype failed", 0);
  return false;
}

bool ProfileSyncServiceHarness::DisableSyncForDatatype(
    syncable::ModelType datatype) {
  LogClientInfo("DisableSyncForDatatype", 1);

  syncable::ModelTypeSet synced_datatypes;
  if (service() == NULL) {
    LOG(ERROR) << "DisableSyncForDatatype(): service() is null.";
    return false;
  }

  service()->GetPreferredDataTypes(&synced_datatypes);
  syncable::ModelTypeSet::iterator it = synced_datatypes.find(datatype);
  if (it == synced_datatypes.end()) {
    VLOG(1) << "DisableSyncForDatatype(): Sync already disabled for datatype "
            << syncable::ModelTypeToString(datatype)
            << " on Client " << id_ << ".";
    return true;
  }

  synced_datatypes.erase(it);
  service()->OnUserChoseDatatypes(false, synced_datatypes);
  if (AwaitSyncCycleCompletion("Datatype reconfiguration.")) {
    VLOG(1) << "DisableSyncForDatatype(): Disabled sync for datatype "
            << syncable::ModelTypeToString(datatype)
            << " on Client " << id_ << ".";
    return true;
  }

  LogClientInfo("DisableSyncForDatatype failed", 0);
  return false;
}

bool ProfileSyncServiceHarness::EnableSyncForAllDatatypes() {
  LogClientInfo("EnableSyncForAllDatatypes", 1);

  if (wait_state_ == SYNC_DISABLED) {
    return SetupSync();
  }

  if (service() == NULL) {
    LOG(ERROR) << "EnableSyncForAllDatatypes(): service() is null.";
    return false;
  }

  syncable::ModelTypeSet synced_datatypes;
  for (int i = syncable::FIRST_REAL_MODEL_TYPE;
       i < syncable::MODEL_TYPE_COUNT;
       ++i) {
    synced_datatypes.insert(syncable::ModelTypeFromInt(i));
  }
  service()->OnUserChoseDatatypes(true, synced_datatypes);
  if (AwaitSyncCycleCompletion("Datatype reconfiguration.")) {
    VLOG(1) << "EnableSyncForAllDatatypes(): Enabled sync for all datatypes on "
               "Client " << id_ << ".";
    return true;
  }

  LogClientInfo("EnableSyncForAllDatatypes failed", 0);
  return false;
}

bool ProfileSyncServiceHarness::DisableSyncForAllDatatypes() {
  LogClientInfo("DisableSyncForAllDatatypes", 1);

  if (service() == NULL) {
    LOG(ERROR) << "DisableSyncForAllDatatypes(): service() is null.";
    return false;
  }

  service()->DisableForUser();
  wait_state_ = SYNC_DISABLED;
  VLOG(1) << "DisableSyncForAllDatatypes(): Disabled sync for all datatypes on "
             "Client " << id_;
  return true;
}

std::string ProfileSyncServiceHarness::GetUpdatedTimestamp(
    syncable::ModelType model_type) {
  const SyncSessionSnapshot* snap = GetLastSessionSnapshot();
  DCHECK(snap != NULL) << "GetUpdatedTimestamp(): Sync snapshot is NULL.";
  return snap->download_progress_markers[model_type];
}

void ProfileSyncServiceHarness::LogClientInfo(const std::string& message,
                                              int log_level) {
  if (service()) {
    const SyncSessionSnapshot* snap = GetLastSessionSnapshot();
    if (snap) {
      VLOG(log_level) << "Client " << id_ << ": " << message
                      << ": num_updates_downloaded : "
                      << snap->syncer_status.num_updates_downloaded_total
                      << ", has_more_to_sync: " << snap->has_more_to_sync
                      << ", unsynced_count: " << snap->unsynced_count
                      << ", num_blocking_conflicting_updates: "
                      << snap->num_blocking_conflicting_updates
                      << ", num_conflicting_updates: "
                      << snap->num_conflicting_updates
                      << ", has_unsynced_items: "
                      << service()->HasUnsyncedItems()
                      << ", passphrase_required_reason: "
                      << sync_api::PassphraseRequiredReasonToString(
                            service()->passphrase_required_reason())
                      << ", notifications_enabled: "
                      << GetStatus().notifications_enabled
                      << ", service_is_pushing_changes: "
                      << ServiceIsPushingChanges()
                      << ", has_pending_backend_migration: "
                      << service()->HasPendingBackendMigration();
    } else {
      VLOG(log_level) << "Client " << id_ << ": " << message
                      << ": Sync session snapshot not available.";
    }
  } else {
    VLOG(log_level) << "Client " << id_ << ": " << message
                    << ": Sync service not available.";
  }
}

bool ProfileSyncServiceHarness::EnableEncryptionForType(
    syncable::ModelType type) {
  syncable::ModelTypeSet encrypted_types;
  service_->GetEncryptedDataTypes(&encrypted_types);
  if (encrypted_types.count(type) > 0)
    return true;
  encrypted_types.insert(type);
  service_->EncryptDataTypes(encrypted_types);

  // Wait some time to let the enryption finish.
  return WaitForTypeEncryption(type);
}

bool ProfileSyncServiceHarness::WaitForTypeEncryption(
    syncable::ModelType type) {
  if (IsSynced() &&
      IsTypeEncrypted(type) &&
      GetLastSessionSnapshot()->num_conflicting_updates == 0) {
    // Encryption is already complete for |type|; do not wait.
    return true;
  }

  std::string reason = "Waiting for encryption.";
  wait_state_ = WAITING_FOR_ENCRYPTION;
  waiting_for_encryption_type_ = type;
  if (!AwaitStatusChangeWithTimeout(kLiveSyncOperationTimeoutMs, reason)) {
    LOG(ERROR) << "Did not receive EncryptionComplete notification after"
               << kLiveSyncOperationTimeoutMs / 1000
               << " seconds.";
    return false;
  }
  return IsTypeEncrypted(type);
}

bool ProfileSyncServiceHarness::IsTypeEncrypted(syncable::ModelType type) {
  syncable::ModelTypeSet encrypted_types;
  service_->GetEncryptedDataTypes(&encrypted_types);
  if (encrypted_types.count(type) == 0) {
    return false;
  }
  return true;
}

std::string ProfileSyncServiceHarness::GetServiceStatus() {
  DictionaryValue value;
  sync_ui_util::ConstructAboutInformation(service_, &value);
  std::string service_status;
  base::JSONWriter::Write(&value, true, &service_status);
  return service_status;
}
