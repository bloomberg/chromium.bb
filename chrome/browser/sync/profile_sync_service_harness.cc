// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/profile_sync_service_harness.h"

#include <stddef.h>
#include <algorithm>
#include <iterator>
#include <ostream>
#include <set>
#include <sstream>
#include <vector>

#include "base/base64.h"
#include "base/compiler_specific.h"
#include "base/json/json_writer.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop.h"
#include "base/task.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/glue/data_type_controller.h"
#include "chrome/browser/sync/sessions/session_state.h"
#include "chrome/browser/sync/signin_manager.h"
#include "chrome/browser/sync/sync_ui_util.h"

using browser_sync::sessions::SyncSessionSnapshot;

// TODO(rsimha): Remove the following lines once crbug.com/91863 is fixed.
// The amount of time for which we wait for a live sync operation to complete.
static const int kLiveSyncOperationTimeoutMs = 45000;

// The amount of time we wait for test cases that verify exponential backoff.
static const int kExponentialBackoffVerificationTimeoutMs = 60000;

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
    const std::string& password)
    : waiting_for_encryption_type_(syncable::UNSPECIFIED),
      wait_state_(INITIAL_WAIT_STATE),
      profile_(profile),
      service_(NULL),
      timestamp_match_partner_(NULL),
      username_(username),
      password_(password),
      profile_debug_name_(profile->GetDebugName()) {
  if (IsSyncAlreadySetup()) {
    service_ = profile_->GetProfileSyncService();
    service_->AddObserver(this);
    ignore_result(TryListeningToMigrationEvents());
    wait_state_ = FULLY_SYNCED;
  }
}

ProfileSyncServiceHarness::~ProfileSyncServiceHarness() {}

// static
ProfileSyncServiceHarness* ProfileSyncServiceHarness::CreateAndAttach(
    Profile* profile) {
  if (!profile->HasProfileSyncService()) {
    NOTREACHED() << "Profile has never signed into sync.";
    return NULL;
  }
  return new ProfileSyncServiceHarness(profile, "", "");
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
    LOG(ERROR) << profile_debug_name_
               << ": SetupSync failed. Syncer status:\n" << status;
  } else {
    VLOG(1) << profile_debug_name_ << ": SetupSync successful.";
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

  // Subscribe sync client to notifications from the backend migrator
  // (possible only after choosing data types).
  if (!TryListeningToMigrationEvents()) {
    NOTREACHED();
    return false;
  }

  // Make sure that a partner client hasn't already set an explicit passphrase.
  if (wait_state_ == SET_PASSPHRASE_FAILED) {
    LOG(ERROR) << "A passphrase is required for decryption. Sync cannot proceed"
                  " until SetPassphrase is called.";
    return false;
  }

  // Set our implicit passphrase.
  service_->SetPassphrase(password_, false);

  // Wait for initial sync cycle to be completed.
  DCHECK_EQ(wait_state_, WAITING_FOR_INITIAL_SYNC);
  if (!AwaitStatusChangeWithTimeout(kLiveSyncOperationTimeoutMs,
      "Waiting for initial sync cycle to complete.")) {
    LOG(ERROR) << "Initial sync cycle did not complete after "
               << kLiveSyncOperationTimeoutMs / 1000
               << " seconds.";
    return false;
  }

  // Make sure that initial sync wasn't blocked by a missing passphrase.
  if (wait_state_ == SET_PASSPHRASE_FAILED) {
    LOG(ERROR) << "A passphrase is required for decryption. Sync cannot proceed"
                  " until SetPassphrase is called.";
    return false;
  }

  // Indicate to the browser that sync setup is complete.
  service()->SetSyncSetupCompleted();

  return true;
}

bool ProfileSyncServiceHarness::TryListeningToMigrationEvents() {
  browser_sync::BackendMigrator* migrator =
      service_->GetBackendMigratorForTest();
  if (migrator && !migrator->HasMigrationObserver(this)) {
    migrator->AddMigrationObserver(this);
    return true;
  }
  return false;
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
      VLOG(1) << GetClientInfoString("WAITING_FOR_ON_BACKEND_INITIALIZED");
      if (service()->sync_initialized()) {
        // The sync backend is initialized.
        SignalStateCompleteWithNextState(WAITING_FOR_INITIAL_SYNC);
      }
      break;
    }
    case WAITING_FOR_INITIAL_SYNC: {
      VLOG(1) << GetClientInfoString("WAITING_FOR_INITIAL_SYNC");
      if (IsFullySynced()) {
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
    case WAITING_FOR_FULL_SYNC: {
      VLOG(1) << GetClientInfoString("WAITING_FOR_FULL_SYNC");
      if (IsFullySynced()) {
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
    case WAITING_FOR_DATA_SYNC: {
      if (IsDataSynced()) {
        SignalStateCompleteWithNextState(FULLY_SYNCED);
        break;
      }
      break;
    }
    case WAITING_FOR_UPDATES: {
      VLOG(1) << GetClientInfoString("WAITING_FOR_UPDATES");
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
      VLOG(1) << GetClientInfoString("WAITING_FOR_PASSPHRASE_REQUIRED");
      if (service()->IsPassphraseRequired()) {
        // A passphrase is now required. Wait for it to be accepted.
        SignalStateCompleteWithNextState(WAITING_FOR_PASSPHRASE_ACCEPTED);
      }
      break;
    }
    case WAITING_FOR_PASSPHRASE_ACCEPTED: {
      VLOG(1) << GetClientInfoString("WAITING_FOR_PASSPHRASE_ACCEPTED");
      if (service()->ShouldPushChanges() &&
          !service()->IsPassphraseRequired() &&
          service()->IsUsingSecondaryPassphrase()) {
        // The passphrase has been accepted, and sync has been restarted.
        SignalStateCompleteWithNextState(FULLY_SYNCED);
      }
      break;
    }
    case WAITING_FOR_ENCRYPTION: {
      VLOG(1) << GetClientInfoString("WAITING_FOR_ENCRYPTION");
      // The correctness of this if condition may depend on the ordering of its
      // sub-expressions.  See crbug.com/98607, crbug.com/95619.
      // TODO(rlarocque): Figure out a less brittle way of detecting this.
      if (IsTypeEncrypted(waiting_for_encryption_type_) &&
          IsFullySynced() &&
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
      VLOG(1) << GetClientInfoString("WAITING_FOR_SYNC_CONFIGURATION");
      if (service()->ShouldPushChanges()) {
        // The Datatype manager is configured and sync is fully initialized.
        SignalStateCompleteWithNextState(FULLY_SYNCED);
      }
      break;
    }
    case WAITING_FOR_SYNC_DISABLED: {
      VLOG(1) << GetClientInfoString("WAITING_FOR_SYNC_DISABLED");
      if (service()->HasSyncSetupCompleted() == false) {
        // Sync has been disabled.
        SignalStateCompleteWithNextState(SYNC_DISABLED);
      }
      break;
    }
    case WAITING_FOR_EXPONENTIAL_BACKOFF_VERIFICATION: {
      VLOG(1) << GetClientInfoString(
                     "WAITING_FOR_EXPONENTIAL_BACKOFF_VERIFICATION");
      const browser_sync::sessions::SyncSessionSnapshot *snap =
          GetLastSessionSnapshot();
      CHECK(snap);
      retry_verifier_.VerifyRetryInterval(*snap);
      if (retry_verifier_.done()) {
        // Retry verifier is done verifying exponential backoff.
        SignalStateCompleteWithNextState(WAITING_FOR_NOTHING);
      }
      break;
    }
    case WAITING_FOR_MIGRATION_TO_START: {
      VLOG(1) << GetClientInfoString("WAITING_FOR_MIGRATION_TO_START");
      if (HasPendingBackendMigration()) {
        // There are pending migrations. Wait for them.
        SignalStateCompleteWithNextState(WAITING_FOR_MIGRATION_TO_FINISH);
      }
      break;
    }
    case WAITING_FOR_MIGRATION_TO_FINISH: {
      VLOG(1) << GetClientInfoString("WAITING_FOR_MIGRATION_TO_FINISH");
      if (!HasPendingBackendMigration()) {
        // Done migrating.
        SignalStateCompleteWithNextState(WAITING_FOR_NOTHING);
      }
      break;
    }
    case WAITING_FOR_ACTIONABLE_ERROR: {
      VLOG(1) << GetClientInfoString("WAITING_FOR_ACTIONABLE_ERROR");
      ProfileSyncService::Status status = GetStatus();
      if (status.sync_protocol_error.action != browser_sync::UNKNOWN_ACTION &&
          service_->unrecoverable_error_detected() == true) {
        // An actionable error has been detected.
        SignalStateCompleteWithNextState(WAITING_FOR_NOTHING);
      }
      break;
    }
    case SERVER_UNREACHABLE: {
      VLOG(1) << GetClientInfoString("SERVER_UNREACHABLE");
      if (GetStatus().server_reachable) {
        // The client was offline due to the network being disabled, but is now
        // back online. Wait for the pending sync cycle to complete.
        SignalStateCompleteWithNextState(WAITING_FOR_FULL_SYNC);
      }
      break;
    }
    case SET_PASSPHRASE_FAILED: {
      // A passphrase is required for decryption. There is nothing the sync
      // client can do until SetPassphrase() is called.
      VLOG(1) << GetClientInfoString("SET_PASSPHRASE_FAILED");
      break;
    }
    case FULLY_SYNCED: {
      // The client is online and fully synced. There is nothing to do.
      VLOG(1) << GetClientInfoString("FULLY_SYNCED");
      break;
    }
    case SYNC_DISABLED: {
      // Syncing is disabled for the client. There is nothing to do.
      VLOG(1) << GetClientInfoString("SYNC_DISABLED");
      break;
    }
    case WAITING_FOR_NOTHING: {
      // We don't care about the state of the syncer for the rest of the test
      // case.
      VLOG(1) << GetClientInfoString("WAITING_FOR_NOTHING");
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

void ProfileSyncServiceHarness::OnMigrationStateChange() {
  // Update migration state.
  if (HasPendingBackendMigration()) {
    // Merge current pending migration types into
    // |pending_migration_types_|.
    syncable::ModelTypeSet new_pending_migration_types =
        service()->GetBackendMigratorForTest()->
        GetPendingMigrationTypesForTest();
    syncable::ModelTypeSet temp;
    std::set_union(pending_migration_types_.begin(),
                   pending_migration_types_.end(),
                   new_pending_migration_types.begin(),
                   new_pending_migration_types.end(),
                   std::inserter(temp, temp.end()));
    std::swap(pending_migration_types_, temp);
    VLOG(1) << profile_debug_name_ << ": new pending migration types "
            << syncable::ModelTypeSetToString(pending_migration_types_);
  } else {
    // Merge just-finished pending migration types into
    // |migration_types_|.
    syncable::ModelTypeSet temp;
    std::set_union(pending_migration_types_.begin(),
                   pending_migration_types_.end(),
                   migrated_types_.begin(),
                   migrated_types_.end(),
                   std::inserter(temp, temp.end()));
    std::swap(migrated_types_, temp);
    pending_migration_types_.clear();
    VLOG(1) << profile_debug_name_ << ": new migrated types "
            << syncable::ModelTypeSetToString(migrated_types_);
  }
  RunStateChangeMachine();
}

bool ProfileSyncServiceHarness::AwaitPassphraseRequired() {
  VLOG(1) << GetClientInfoString("AwaitPassphraseRequired");
  if (wait_state_ == SYNC_DISABLED) {
    LOG(ERROR) << "Sync disabled for " << profile_debug_name_ << ".";
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
  VLOG(1) << GetClientInfoString("AwaitPassphraseAccepted");
  if (wait_state_ == SYNC_DISABLED) {
    LOG(ERROR) << "Sync disabled for " << profile_debug_name_ << ".";
    return false;
  }

  if (service()->ShouldPushChanges() &&
      !service()->IsPassphraseRequired() &&
      service()->IsUsingSecondaryPassphrase()) {
    // Passphrase is already accepted; don't wait.
    return true;
  }

  wait_state_ = WAITING_FOR_PASSPHRASE_ACCEPTED;
  return AwaitStatusChangeWithTimeout(kLiveSyncOperationTimeoutMs,
                                      "Waiting for passphrase to be accepted.");
}

bool ProfileSyncServiceHarness::AwaitBackendInitialized() {
  VLOG(1) << GetClientInfoString("AwaitBackendInitialized");
  if (service()->sync_initialized()) {
    // The sync backend host has already been initialized; don't wait.
    return true;
  }

  wait_state_ = WAITING_FOR_ON_BACKEND_INITIALIZED;
  return AwaitStatusChangeWithTimeout(kLiveSyncOperationTimeoutMs,
                                      "Waiting for OnBackendInitialized().");
}

bool ProfileSyncServiceHarness::AwaitSyncRestart() {
  VLOG(1) << GetClientInfoString("AwaitSyncRestart");
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

bool ProfileSyncServiceHarness::AwaitDataSyncCompletion(
    const std::string& reason) {
  VLOG(1) << GetClientInfoString("AwaitDataSyncCompletion");

  CHECK(service()->sync_initialized());
  CHECK_NE(wait_state_, SYNC_DISABLED);
  CHECK_NE(wait_state_, SERVER_UNREACHABLE);

  if (IsDataSynced()) {
    // Client is already synced; don't wait.
    return true;
  }

  wait_state_ = WAITING_FOR_DATA_SYNC;
  AwaitStatusChangeWithTimeout(kLiveSyncOperationTimeoutMs, reason);
  if (wait_state_ == FULLY_SYNCED) {
    return true;
  } else {
    LOG(ERROR) << "AwaitDataSyncCompletion failed, state is now: "
               << wait_state_;
    return false;
  }
}

bool ProfileSyncServiceHarness::AwaitFullSyncCompletion(
    const std::string& reason) {
  VLOG(1) << GetClientInfoString("AwaitFullSyncCompletion");
  if (wait_state_ == SYNC_DISABLED) {
    LOG(ERROR) << "Sync disabled for " << profile_debug_name_ << ".";
    return false;
  }

  if (IsFullySynced()) {
    // Client is already synced; don't wait.
    return true;
  }

  if (wait_state_ == SERVER_UNREACHABLE) {
    // Client was offline; wait for it to go online, and then wait for sync.
    AwaitStatusChangeWithTimeout(kLiveSyncOperationTimeoutMs, reason);
    DCHECK_EQ(wait_state_, WAITING_FOR_FULL_SYNC);
    return AwaitStatusChangeWithTimeout(kLiveSyncOperationTimeoutMs, reason);
  }

  DCHECK(service()->sync_initialized());
  wait_state_ = WAITING_FOR_FULL_SYNC;
  AwaitStatusChangeWithTimeout(kLiveSyncOperationTimeoutMs, reason);
  if (wait_state_ == FULLY_SYNCED) {
    // Client is online; sync was successful.
    return true;
  } else if (wait_state_ == SERVER_UNREACHABLE) {
    // Client is offline; sync was unsuccessful.
    LOG(ERROR) << "Client went offline after waiting for sync to finish";
    return false;
  } else {
    LOG(ERROR) << "Invalid wait state: " << wait_state_;
    return false;
  }
}

bool ProfileSyncServiceHarness::AwaitSyncDisabled(const std::string& reason) {
  DCHECK(service()->HasSyncSetupCompleted());
  DCHECK_NE(wait_state_, SYNC_DISABLED);
  wait_state_ = WAITING_FOR_SYNC_DISABLED;
  AwaitStatusChangeWithTimeout(kLiveSyncOperationTimeoutMs, reason);
  return wait_state_ == SYNC_DISABLED;
}

bool ProfileSyncServiceHarness::AwaitExponentialBackoffVerification() {
  const browser_sync::sessions::SyncSessionSnapshot *snap =
      GetLastSessionSnapshot();
  CHECK(snap);
  retry_verifier_.Initialize(*snap);
  wait_state_ = WAITING_FOR_EXPONENTIAL_BACKOFF_VERIFICATION;
  AwaitStatusChangeWithTimeout(kExponentialBackoffVerificationTimeoutMs,
      "Verify Exponential backoff");
  return (retry_verifier_.Succeeded());
}

bool ProfileSyncServiceHarness::AwaitActionableError() {
  ProfileSyncService::Status status = GetStatus();
  CHECK(status.sync_protocol_error.action == browser_sync::UNKNOWN_ACTION);
  wait_state_ = WAITING_FOR_ACTIONABLE_ERROR;
  AwaitStatusChangeWithTimeout(kLiveSyncOperationTimeoutMs,
      "Waiting for actionable error");
  status = GetStatus();
  return (status.sync_protocol_error.action != browser_sync::UNKNOWN_ACTION &&
          service_->unrecoverable_error_detected());
}

bool ProfileSyncServiceHarness::AwaitMigration(
    const syncable::ModelTypeSet& expected_migrated_types) {
  VLOG(1) << GetClientInfoString("AwaitMigration");
  VLOG(1) << profile_debug_name_ << ": waiting until migration is done for "
          << syncable::ModelTypeSetToString(expected_migrated_types);
  while (true) {
    bool migration_finished =
        std::includes(migrated_types_.begin(), migrated_types_.end(),
                      expected_migrated_types.begin(),
                      expected_migrated_types.end());
    VLOG(1) << "Migrated types "
            << syncable::ModelTypeSetToString(migrated_types_)
            << (migration_finished ? " contains " : " does not contain ")
            << syncable::ModelTypeSetToString(expected_migrated_types);
    if (migration_finished) {
      return true;
    }

    if (HasPendingBackendMigration()) {
      wait_state_ = WAITING_FOR_MIGRATION_TO_FINISH;
    } else {
      wait_state_ = WAITING_FOR_MIGRATION_TO_START;
      AwaitStatusChangeWithTimeout(kLiveSyncOperationTimeoutMs,
                                   "Wait for migration to start");
      if (wait_state_ != WAITING_FOR_MIGRATION_TO_FINISH) {
        VLOG(1) << profile_debug_name_
                << ": wait state = " << wait_state_
                << " after migration start is not "
                << "WAITING_FOR_MIGRATION_TO_FINISH";
        return false;
      }
    }
    AwaitStatusChangeWithTimeout(kLiveSyncOperationTimeoutMs,
                                 "Wait for migration to finish");
    if (wait_state_ != WAITING_FOR_NOTHING) {
      VLOG(1) << profile_debug_name_
              << ": wait state = " << wait_state_
              << " after migration finish is not WAITING_FOR_NOTHING";
      return false;
    }
    if (!AwaitFullSyncCompletion(
            "Config sync cycle after migration cycle")) {
      return false;
    }
  }
}

bool ProfileSyncServiceHarness::AwaitMutualSyncCycleCompletion(
    ProfileSyncServiceHarness* partner) {
  VLOG(1) << GetClientInfoString("AwaitMutualSyncCycleCompletion");
  if (!AwaitFullSyncCompletion("Sync cycle completion on active client."))
    return false;
  return partner->WaitUntilTimestampMatches(this,
      "Sync cycle completion on passive client.");
}

bool ProfileSyncServiceHarness::AwaitGroupSyncCycleCompletion(
    std::vector<ProfileSyncServiceHarness*>& partners) {
  VLOG(1) << GetClientInfoString("AwaitGroupSyncCycleCompletion");
  if (!AwaitFullSyncCompletion("Sync cycle completion on active client."))
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
  VLOG(1) << GetClientInfoString("WaitUntilTimestampMatches");
  if (wait_state_ == SYNC_DISABLED) {
    LOG(ERROR) << "Sync disabled for " << profile_debug_name_ << ".";
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
  VLOG(1) << GetClientInfoString("AwaitStatusChangeWithTimeout");
  if (wait_state_ == SYNC_DISABLED) {
    LOG(ERROR) << "Sync disabled for " << profile_debug_name_ << ".";
    return false;
  }
  scoped_refptr<StateChangeTimeoutEvent> timeout_signal(
      new StateChangeTimeoutEvent(this, reason));
  MessageLoop* loop = MessageLoop::current();
  bool did_allow_nestable_tasks = loop->NestableTasksAllowed();
  loop->SetNestableTasksAllowed(true);
  loop->PostDelayedTask(
      FROM_HERE,
      base::Bind(&StateChangeTimeoutEvent::Callback,
                 timeout_signal.get()),
      timeout_milliseconds);
  loop->Run();
  loop->SetNestableTasksAllowed(did_allow_nestable_tasks);
  if (timeout_signal->Abort()) {
    VLOG(1) << GetClientInfoString("AwaitStatusChangeWithTimeout succeeded");
    return true;
  } else {
    VLOG(0) << GetClientInfoString("AwaitStatusChangeWithTimeout timed out");
    return false;
  }
}

ProfileSyncService::Status ProfileSyncServiceHarness::GetStatus() {
  DCHECK(service() != NULL) << "GetStatus(): service() is NULL.";
  return service()->QueryDetailedSyncStatus();
}

// We use this function to share code between IsFullySynced and IsDataSynced
// while ensuring that all conditions are evaluated using on the same snapshot.
bool ProfileSyncServiceHarness::IsDataSyncedImpl(
    const browser_sync::sessions::SyncSessionSnapshot *snap) {
  return snap &&
      snap->num_blocking_conflicting_updates == 0 &&
      ServiceIsPushingChanges() &&
      GetStatus().notifications_enabled &&
      !service()->HasUnsyncedItems() &&
      !snap->has_more_to_sync &&
      !HasPendingBackendMigration() &&
      service()->passphrase_required_reason() !=
        sync_api::REASON_SET_PASSPHRASE_FAILED;
}

bool ProfileSyncServiceHarness::IsDataSynced() {
  if (service() == NULL) {
    VLOG(1) << GetClientInfoString("IsDataSynced(): false");
    return false;
  }

  const SyncSessionSnapshot* snap = GetLastSessionSnapshot();
  bool is_data_synced = IsDataSyncedImpl(snap);

  VLOG(1) << GetClientInfoString(
      is_data_synced ? "IsDataSynced: true" : "IsDataSynced: false");
  return is_data_synced;
}

bool ProfileSyncServiceHarness::IsFullySynced() {
  if (service() == NULL) {
    VLOG(1) << GetClientInfoString("IsFullySynced: false");
    return false;
  }
  const SyncSessionSnapshot* snap = GetLastSessionSnapshot();
  // snap->unsynced_count == 0 is a fairly reliable indicator of whether or not
  // our timestamp is in sync with the server.
  bool is_fully_synced = IsDataSyncedImpl(snap) &&
      snap->unsynced_count == 0;

  VLOG(1) << GetClientInfoString(
      is_fully_synced ? "IsFullySynced: true" : "IsFullySynced: false");
  return is_fully_synced;
}

bool ProfileSyncServiceHarness::HasPendingBackendMigration() {
  browser_sync::BackendMigrator* migrator =
      service()->GetBackendMigratorForTest();
  return migrator && migrator->state() != browser_sync::BackendMigrator::IDLE;
}

bool ProfileSyncServiceHarness::MatchesOtherClient(
    ProfileSyncServiceHarness* partner) {
  // TODO(akalin): Shouldn't this belong with the intersection check?
  // Otherwise, this function isn't symmetric.
  if (!IsFullySynced()) {
    VLOG(2) << profile_debug_name_ << ": not synced, assuming doesn't match";
    return false;
  }

  // Only look for a match if we have at least one enabled datatype in
  // common with the partner client.
  syncable::ModelTypeSet types, other_types, intersection_types;
  service()->GetPreferredDataTypes(&types);
  partner->service()->GetPreferredDataTypes(&other_types);
  std::set_intersection(types.begin(), types.end(), other_types.begin(),
                        other_types.end(),
                        inserter(intersection_types,
                                 intersection_types.begin()));

  VLOG(2) << profile_debug_name_ << ", " << partner->profile_debug_name_
          << ": common types are "
          << syncable::ModelTypeSetToString(intersection_types);

  if (!intersection_types.empty() && !partner->IsFullySynced()) {
    VLOG(2) << "non-empty common types and "
            << partner->profile_debug_name_ << " isn't synced";
    return false;
  }

  for (syncable::ModelTypeSet::iterator i = intersection_types.begin();
       i != intersection_types.end(); ++i) {
    const std::string timestamp = GetUpdatedTimestamp(*i);
    const std::string partner_timestamp = partner->GetUpdatedTimestamp(*i);
    if (timestamp != partner_timestamp) {
      if (VLOG_IS_ON(2)) {
        std::string timestamp_base64, partner_timestamp_base64;
        if (!base::Base64Encode(timestamp, &timestamp_base64)) {
          NOTREACHED();
        }
        if (!base::Base64Encode(
                partner_timestamp, &partner_timestamp_base64)) {
          NOTREACHED();
        }
        VLOG(2) << syncable::ModelTypeToString(*i) << ": "
                << profile_debug_name_ << " timestamp = "
                << timestamp_base64 << ", "
                << partner->profile_debug_name_
                << " partner timestamp = "
                << partner_timestamp_base64;
      }
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
  VLOG(1) << GetClientInfoString(
      "EnableSyncForDatatype("
      + std::string(syncable::ModelTypeToString(datatype)) + ")");

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
            << " on " << profile_debug_name_ << ".";
    return true;
  }

  synced_datatypes.insert(syncable::ModelTypeFromInt(datatype));
  service()->OnUserChoseDatatypes(false, synced_datatypes);
  if (AwaitFullSyncCompletion("Datatype configuration.")) {
    VLOG(1) << "EnableSyncForDatatype(): Enabled sync for datatype "
            << syncable::ModelTypeToString(datatype)
            << " on " << profile_debug_name_ << ".";
    return true;
  }

  VLOG(0) << GetClientInfoString("EnableSyncForDatatype failed");
  return false;
}

bool ProfileSyncServiceHarness::DisableSyncForDatatype(
    syncable::ModelType datatype) {
  VLOG(1) << GetClientInfoString(
      "DisableSyncForDatatype("
      + std::string(syncable::ModelTypeToString(datatype)) + ")");

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
            << " on " << profile_debug_name_ << ".";
    return true;
  }

  synced_datatypes.erase(it);
  service()->OnUserChoseDatatypes(false, synced_datatypes);
  if (AwaitFullSyncCompletion("Datatype reconfiguration.")) {
    VLOG(1) << "DisableSyncForDatatype(): Disabled sync for datatype "
            << syncable::ModelTypeToString(datatype)
            << " on " << profile_debug_name_ << ".";
    return true;
  }

  VLOG(0) << GetClientInfoString("DisableSyncForDatatype failed");
  return false;
}

bool ProfileSyncServiceHarness::EnableSyncForAllDatatypes() {
  VLOG(1) << GetClientInfoString("EnableSyncForAllDatatypes");

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
  if (AwaitFullSyncCompletion("Datatype reconfiguration.")) {
    VLOG(1) << "EnableSyncForAllDatatypes(): Enabled sync for all datatypes on "
            << profile_debug_name_ << ".";
    return true;
  }

  VLOG(0) << GetClientInfoString("EnableSyncForAllDatatypes failed");
  return false;
}

bool ProfileSyncServiceHarness::DisableSyncForAllDatatypes() {
  VLOG(1) << GetClientInfoString("DisableSyncForAllDatatypes");

  if (service() == NULL) {
    LOG(ERROR) << "DisableSyncForAllDatatypes(): service() is null.";
    return false;
  }

  service()->DisableForUser();
  wait_state_ = SYNC_DISABLED;
  VLOG(1) << "DisableSyncForAllDatatypes(): Disabled sync for all datatypes on "
          << profile_debug_name_;
  return true;
}

std::string ProfileSyncServiceHarness::GetUpdatedTimestamp(
    syncable::ModelType model_type) {
  const SyncSessionSnapshot* snap = GetLastSessionSnapshot();
  DCHECK(snap != NULL) << "GetUpdatedTimestamp(): Sync snapshot is NULL.";
  return snap->download_progress_markers[model_type];
}

std::string ProfileSyncServiceHarness::GetClientInfoString(
    const std::string& message) {
  std::stringstream os;
  os << profile_debug_name_ << ": " << message << ": ";
  if (service()) {
    const SyncSessionSnapshot* snap = GetLastSessionSnapshot();
    const ProfileSyncService::Status& status = GetStatus();
    if (snap) {
      // Capture select info from the sync session snapshot and syncer status.
      os << "has_more_to_sync: "
         << snap->has_more_to_sync
         << ", has_unsynced_items: "
         << service()->HasUnsyncedItems()
         << ", unsynced_count: "
         << snap->unsynced_count
         << ", num_conflicting_updates: "
         << snap->num_conflicting_updates
         << ", num_blocking_conflicting_updates: "
         << snap->num_blocking_conflicting_updates
         << ", num_updates_downloaded : "
         << snap->syncer_status.num_updates_downloaded_total
         << ", passphrase_required_reason: "
         << sync_api::PassphraseRequiredReasonToString(
             service()->passphrase_required_reason())
         << ", notifications_enabled: "
         << status.notifications_enabled
         << ", service_is_pushing_changes: "
         << ServiceIsPushingChanges()
         << ", has_pending_backend_migration: "
         << HasPendingBackendMigration();
    } else {
      os << "Sync session snapshot not available";
    }
  } else {
    os << "Sync service not available";
  }
  return os.str();
}

// TODO(zea): Rename this EnableEncryption, since we no longer turn on
// encryption for individual types but for all.
bool ProfileSyncServiceHarness::EnableEncryptionForType(
    syncable::ModelType type) {
  syncable::ModelTypeSet encrypted_types;
  service_->GetEncryptedDataTypes(&encrypted_types);
  if (encrypted_types.count(type) > 0)
    return true;
  service_->EnableEncryptEverything();

  // In order to kick off the encryption we have to reconfigure. Just grab the
  // currently synced types and use them.
  syncable::ModelTypeSet synced_datatypes;
  service_->GetPreferredDataTypes(&synced_datatypes);
  bool sync_everything = (synced_datatypes.size() ==
      (syncable::MODEL_TYPE_COUNT - syncable::FIRST_REAL_MODEL_TYPE));
  service_->OnUserChoseDatatypes(sync_everything,
                                 synced_datatypes);

  // Wait some time to let the enryption finish.
  return WaitForTypeEncryption(type);
}

bool ProfileSyncServiceHarness::WaitForTypeEncryption(
    syncable::ModelType type) {
  // The correctness of this if condition depends on the ordering of its
  // sub-expressions.  See crbug.com/95619.
  // TODO(rlarocque): Figure out a less brittle way of detecting this.
  if (IsTypeEncrypted(type) &&
      IsFullySynced() &&
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
  bool is_type_encrypted = (encrypted_types.count(type) != 0);
  VLOG(2) << syncable::ModelTypeToString(type) << " is "
          << (is_type_encrypted ? "" : "not ") << "encrypted; "
          << "encrypted types = "
          << syncable::ModelTypeSetToString(encrypted_types);
  return is_type_encrypted;
}

bool ProfileSyncServiceHarness::IsTypeRunning(syncable::ModelType type) {
  browser_sync::DataTypeController::StateMap state_map;
  service_->GetDataTypeControllerStates(&state_map);
  return (state_map.count(type) != 0 &&
          state_map[type] == browser_sync::DataTypeController::RUNNING);
}

bool ProfileSyncServiceHarness::IsTypePreferred(syncable::ModelType type) {
  syncable::ModelTypeSet synced_types;
  service_->GetPreferredDataTypes(&synced_types);
  return (synced_types.count(type) != 0);
}

std::string ProfileSyncServiceHarness::GetServiceStatus() {
  DictionaryValue value;
  sync_ui_util::ConstructAboutInformation(service_, &value);
  std::string service_status;
  base::JSONWriter::Write(&value, true, &service_status);
  return service_status;
}
