// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/profile_sync_service_harness.h"

#include <cstddef>
#include <iterator>
#include <ostream>
#include <set>
#include <sstream>
#include <vector>

#include "base/base64.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/json/json_writer.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/sync/about_sync_util.h"
#include "chrome/browser/sync/glue/data_type_controller.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/common/chrome_switches.h"
#include "sync/internal_api/public/base/progress_marker_map.h"
#include "sync/internal_api/public/sessions/sync_session_snapshot.h"
#include "sync/internal_api/public/util/sync_string_conversions.h"

using syncer::sessions::SyncSessionSnapshot;

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
    : waiting_for_encryption_type_(syncer::UNSPECIFIED),
      wait_state_(INITIAL_WAIT_STATE),
      profile_(profile),
      service_(NULL),
      progress_marker_partner_(NULL),
      username_(username),
      password_(password),
      profile_debug_name_(profile->GetDebugName()),
      waiting_for_status_change_(false) {
  if (IsSyncAlreadySetup()) {
    service_ = ProfileSyncServiceFactory::GetInstance()->GetForProfile(
        profile_);
    service_->AddObserver(this);
    ignore_result(TryListeningToMigrationEvents());
    wait_state_ = FULLY_SYNCED;
  }
}

ProfileSyncServiceHarness::~ProfileSyncServiceHarness() {}

// static
ProfileSyncServiceHarness* ProfileSyncServiceHarness::CreateAndAttach(
    Profile* profile) {
  ProfileSyncServiceFactory* f = ProfileSyncServiceFactory::GetInstance();
  if (!f->HasProfileSyncService(profile)) {
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
  return ProfileSyncServiceFactory::GetInstance()->HasProfileSyncService(
      profile_);
}

bool ProfileSyncServiceHarness::SetupSync() {
  bool result = SetupSync(syncer::ModelTypeSet::All());
  if (result == false) {
    std::string status = GetServiceStatus();
    LOG(ERROR) << profile_debug_name_
               << ": SetupSync failed. Syncer status:\n" << status;
  } else {
    DVLOG(1) << profile_debug_name_ << ": SetupSync successful.";
  }
  return result;
}

bool ProfileSyncServiceHarness::SetupSync(
    syncer::ModelTypeSet synced_datatypes) {
  // Initialize the sync client's profile sync service object.
  service_ =
      ProfileSyncServiceFactory::GetInstance()->GetForProfile(profile_);
  if (service_ == NULL) {
    LOG(ERROR) << "SetupSync(): service_ is null.";
    return false;
  }

  // Subscribe sync client to notifications from the profile sync service.
  if (!service_->HasObserver(this))
    service_->AddObserver(this);

  // Tell the sync service that setup is in progress so we don't start syncing
  // until we've finished configuration.
  service_->SetSetupInProgress(true);

  // Authenticate sync client using GAIA credentials.
  service_->signin()->StartSignIn(username_, password_, "", "");

  // Wait for the OnBackendInitialized() callback.
  if (!AwaitBackendInitialized()) {
    LOG(ERROR) << "OnBackendInitialized() not seen after "
               << kLiveSyncOperationTimeoutMs / 1000
               << " seconds.";
    return false;
  }

  // Make sure that initial sync wasn't blocked by a missing passphrase.
  if (wait_state_ == SET_PASSPHRASE_FAILED) {
    LOG(ERROR) << "A passphrase is required for decryption. Sync cannot proceed"
                  " until SetDecryptionPassphrase is called.";
    return false;
  }

  // Make sure that initial sync wasn't blocked by rejected credentials.
  if (wait_state_ == CREDENTIALS_REJECTED) {
    LOG(ERROR) << "Credentials were rejected. Sync cannot proceed.";
    return false;
  }

  // Choose the datatypes to be synced. If all datatypes are to be synced,
  // set sync_everything to true; otherwise, set it to false.
  bool sync_everything =
      synced_datatypes.Equals(syncer::ModelTypeSet::All());
  service()->OnUserChoseDatatypes(sync_everything, synced_datatypes);

  // Notify ProfileSyncService that we are done with configuration.
  service_->SetSetupInProgress(false);

  // Subscribe sync client to notifications from the backend migrator
  // (possible only after choosing data types).
  if (!TryListeningToMigrationEvents()) {
    NOTREACHED();
    return false;
  }

  // Set an implicit passphrase for encryption if an explicit one hasn't already
  // been set. If an explicit passphrase has been set, immediately return false,
  // since a decryption passphrase is required.
  if (!service_->IsUsingSecondaryPassphrase()) {
    service_->SetEncryptionPassphrase(password_, ProfileSyncService::IMPLICIT);
  } else {
    LOG(ERROR) << "A passphrase is required for decryption. Sync cannot proceed"
                  " until SetDecryptionPassphrase is called.";
    return false;
  }

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
                  " until SetDecryptionPassphrase is called.";
    return false;
  }

  // Make sure that initial sync wasn't blocked by rejected credentials.
  if (wait_state_ == CREDENTIALS_REJECTED) {
    LOG(ERROR) << "Credentials were rejected. Sync cannot proceed.";
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
  if (waiting_for_status_change_)
    MessageLoop::current()->QuitWhenIdle();
}

bool ProfileSyncServiceHarness::RunStateChangeMachine() {
  WaitState original_wait_state = wait_state_;
  switch (wait_state_) {
    case WAITING_FOR_ON_BACKEND_INITIALIZED: {
      DVLOG(1) << GetClientInfoString("WAITING_FOR_ON_BACKEND_INITIALIZED");
      if (service()->sync_initialized()) {
        // The sync backend is initialized.
        SignalStateCompleteWithNextState(WAITING_FOR_INITIAL_SYNC);
      }
      break;
    }
    case WAITING_FOR_INITIAL_SYNC: {
      DVLOG(1) << GetClientInfoString("WAITING_FOR_INITIAL_SYNC");
      if (IsFullySynced()) {
        // The first sync cycle is now complete. We can start running tests.
        SignalStateCompleteWithNextState(FULLY_SYNCED);
        break;
      }
      if (service()->passphrase_required_reason() ==
              syncer::REASON_DECRYPTION) {
        // A passphrase is required for decryption and we don't have it. Do not
        // wait any more.
        SignalStateCompleteWithNextState(SET_PASSPHRASE_FAILED);
        break;
      }
      if (service()->GetAuthError().state() ==
          GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS) {
        // Our credentials were rejected. Do not wait any more.
        SignalStateCompleteWithNextState(CREDENTIALS_REJECTED);
        break;
      }
      break;
    }
    case WAITING_FOR_FULL_SYNC: {
      DVLOG(1) << GetClientInfoString("WAITING_FOR_FULL_SYNC");
      if (IsFullySynced()) {
        // The sync cycle we were waiting for is complete.
        SignalStateCompleteWithNextState(FULLY_SYNCED);
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
      DVLOG(1) << GetClientInfoString("WAITING_FOR_UPDATES");
      DCHECK(progress_marker_partner_);
      if (!MatchesOtherClient(progress_marker_partner_)) {
        // The client is not yet fully synced; keep waiting until we converge.
        break;
      }

      SignalStateCompleteWithNextState(FULLY_SYNCED);
      break;
    }
    case WAITING_FOR_PASSPHRASE_REQUIRED: {
      DVLOG(1) << GetClientInfoString("WAITING_FOR_PASSPHRASE_REQUIRED");
      if (service()->IsPassphraseRequired()) {
        // A passphrase is now required. Wait for it to be accepted.
        SignalStateCompleteWithNextState(WAITING_FOR_PASSPHRASE_ACCEPTED);
      }
      break;
    }
    case WAITING_FOR_PASSPHRASE_ACCEPTED: {
      DVLOG(1) << GetClientInfoString("WAITING_FOR_PASSPHRASE_ACCEPTED");
      if (service()->ShouldPushChanges() &&
          !service()->IsPassphraseRequired() &&
          service()->IsUsingSecondaryPassphrase()) {
        // The passphrase has been accepted, and sync has been restarted.
        SignalStateCompleteWithNextState(FULLY_SYNCED);
      }
      break;
    }
    case WAITING_FOR_ENCRYPTION: {
      DVLOG(1) << GetClientInfoString("WAITING_FOR_ENCRYPTION");
      // The correctness of this if condition may depend on the ordering of its
      // sub-expressions.  See crbug.com/98607, crbug.com/95619.
      // TODO(rlarocque): Figure out a less brittle way of detecting this.
      if (IsTypeEncrypted(waiting_for_encryption_type_) &&
          IsFullySynced() &&
          GetLastSessionSnapshot().num_encryption_conflicts() == 0) {
        // Encryption is now complete for the the type in which we were waiting.
        SignalStateCompleteWithNextState(FULLY_SYNCED);
        break;
      }
      break;
    }
    case WAITING_FOR_SYNC_CONFIGURATION: {
      DVLOG(1) << GetClientInfoString("WAITING_FOR_SYNC_CONFIGURATION");
      if (service()->ShouldPushChanges()) {
        // The Datatype manager is configured and sync is fully initialized.
        SignalStateCompleteWithNextState(FULLY_SYNCED);
      }
      break;
    }
    case WAITING_FOR_SYNC_DISABLED: {
      DVLOG(1) << GetClientInfoString("WAITING_FOR_SYNC_DISABLED");
      if (service()->HasSyncSetupCompleted() == false) {
        // Sync has been disabled.
        SignalStateCompleteWithNextState(SYNC_DISABLED);
      }
      break;
    }
    case WAITING_FOR_EXPONENTIAL_BACKOFF_VERIFICATION: {
      DVLOG(1) << GetClientInfoString(
          "WAITING_FOR_EXPONENTIAL_BACKOFF_VERIFICATION");
      const SyncSessionSnapshot& snap = GetLastSessionSnapshot();
      retry_verifier_.VerifyRetryInterval(snap);
      if (retry_verifier_.done()) {
        // Retry verifier is done verifying exponential backoff.
        SignalStateCompleteWithNextState(WAITING_FOR_NOTHING);
      }
      break;
    }
    case WAITING_FOR_MIGRATION_TO_START: {
      DVLOG(1) << GetClientInfoString("WAITING_FOR_MIGRATION_TO_START");
      if (HasPendingBackendMigration()) {
        // There are pending migrations. Wait for them.
        SignalStateCompleteWithNextState(WAITING_FOR_MIGRATION_TO_FINISH);
      }
      break;
    }
    case WAITING_FOR_MIGRATION_TO_FINISH: {
      DVLOG(1) << GetClientInfoString("WAITING_FOR_MIGRATION_TO_FINISH");
      if (!HasPendingBackendMigration()) {
        // Done migrating.
        SignalStateCompleteWithNextState(WAITING_FOR_NOTHING);
      }
      break;
    }
    case WAITING_FOR_ACTIONABLE_ERROR: {
      DVLOG(1) << GetClientInfoString("WAITING_FOR_ACTIONABLE_ERROR");
      ProfileSyncService::Status status = GetStatus();
      if (status.sync_protocol_error.action != syncer::UNKNOWN_ACTION &&
          service_->HasUnrecoverableError() == true) {
        // An actionable error has been detected.
        SignalStateCompleteWithNextState(WAITING_FOR_NOTHING);
      }
      break;
    }
    case SET_PASSPHRASE_FAILED: {
      // A passphrase is required for decryption. There is nothing the sync
      // client can do until SetDecryptionPassphrase() is called.
      DVLOG(1) << GetClientInfoString("SET_PASSPHRASE_FAILED");
      break;
    }
    case FULLY_SYNCED: {
      // The client is online and fully synced. There is nothing to do.
      DVLOG(1) << GetClientInfoString("FULLY_SYNCED");
      break;
    }
    case SYNC_DISABLED: {
      // Syncing is disabled for the client. There is nothing to do.
      DVLOG(1) << GetClientInfoString("SYNC_DISABLED");
      break;
    }
    case WAITING_FOR_NOTHING: {
      // We don't care about the state of the syncer for the rest of the test
      // case.
      DVLOG(1) << GetClientInfoString("WAITING_FOR_NOTHING");
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
    pending_migration_types_.PutAll(
        service()->GetBackendMigratorForTest()->
            GetPendingMigrationTypesForTest());
    DVLOG(1) << profile_debug_name_ << ": new pending migration types "
             << syncer::ModelTypeSetToString(pending_migration_types_);
  } else {
    // Merge just-finished pending migration types into
    // |migration_types_|.
    migrated_types_.PutAll(pending_migration_types_);
    pending_migration_types_.Clear();
    DVLOG(1) << profile_debug_name_ << ": new migrated types "
             << syncer::ModelTypeSetToString(migrated_types_);
  }
  RunStateChangeMachine();
}

bool ProfileSyncServiceHarness::AwaitPassphraseRequired() {
  DVLOG(1) << GetClientInfoString("AwaitPassphraseRequired");
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
  DVLOG(1) << GetClientInfoString("AwaitPassphraseAccepted");
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
  DVLOG(1) << GetClientInfoString("AwaitBackendInitialized");
  if (service()->sync_initialized()) {
    // The sync backend host has already been initialized; don't wait.
    return true;
  }

  wait_state_ = WAITING_FOR_ON_BACKEND_INITIALIZED;
  return AwaitStatusChangeWithTimeout(kLiveSyncOperationTimeoutMs,
                                      "Waiting for OnBackendInitialized().");
}

bool ProfileSyncServiceHarness::AwaitSyncRestart() {
  DVLOG(1) << GetClientInfoString("AwaitSyncRestart");
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
  DVLOG(1) << GetClientInfoString("AwaitDataSyncCompletion");

  CHECK(service()->sync_initialized());
  CHECK_NE(wait_state_, SYNC_DISABLED);

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
  DVLOG(1) << GetClientInfoString("AwaitFullSyncCompletion");
  if (wait_state_ == SYNC_DISABLED) {
    LOG(ERROR) << "Sync disabled for " << profile_debug_name_ << ".";
    return false;
  }

  if (IsFullySynced()) {
    // Client is already synced; don't wait.
    return true;
  }

  DCHECK(service()->sync_initialized());
  wait_state_ = WAITING_FOR_FULL_SYNC;
  AwaitStatusChangeWithTimeout(kLiveSyncOperationTimeoutMs, reason);
  if (wait_state_ == FULLY_SYNCED) {
    // Client is online; sync was successful.
    return true;
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
  const SyncSessionSnapshot& snap = GetLastSessionSnapshot();
  retry_verifier_.Initialize(snap);
  wait_state_ = WAITING_FOR_EXPONENTIAL_BACKOFF_VERIFICATION;
  AwaitStatusChangeWithTimeout(kExponentialBackoffVerificationTimeoutMs,
      "Verify Exponential backoff");
  return (retry_verifier_.Succeeded());
}

bool ProfileSyncServiceHarness::AwaitActionableError() {
  ProfileSyncService::Status status = GetStatus();
  CHECK(status.sync_protocol_error.action == syncer::UNKNOWN_ACTION);
  wait_state_ = WAITING_FOR_ACTIONABLE_ERROR;
  AwaitStatusChangeWithTimeout(kLiveSyncOperationTimeoutMs,
      "Waiting for actionable error");
  status = GetStatus();
  return (status.sync_protocol_error.action != syncer::UNKNOWN_ACTION &&
          service_->HasUnrecoverableError());
}

bool ProfileSyncServiceHarness::AwaitMigration(
    syncer::ModelTypeSet expected_migrated_types) {
  DVLOG(1) << GetClientInfoString("AwaitMigration");
  DVLOG(1) << profile_debug_name_ << ": waiting until migration is done for "
          << syncer::ModelTypeSetToString(expected_migrated_types);
  while (true) {
    bool migration_finished = migrated_types_.HasAll(expected_migrated_types);
    DVLOG(1) << "Migrated types "
             << syncer::ModelTypeSetToString(migrated_types_)
             << (migration_finished ? " contains " : " does not contain ")
             << syncer::ModelTypeSetToString(expected_migrated_types);
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
        DVLOG(1) << profile_debug_name_
                 << ": wait state = " << wait_state_
                 << " after migration start is not "
                 << "WAITING_FOR_MIGRATION_TO_FINISH";
        return false;
      }
    }
    AwaitStatusChangeWithTimeout(kLiveSyncOperationTimeoutMs,
                                 "Wait for migration to finish");
    if (wait_state_ != WAITING_FOR_NOTHING) {
      DVLOG(1) << profile_debug_name_
               << ": wait state = " << wait_state_
               << " after migration finish is not WAITING_FOR_NOTHING";
      return false;
    }
    // We must use AwaitDataSyncCompletion rather than the more common
    // AwaitFullSyncCompletion.  As long as crbug.com/97780 is open, we will
    // rely on self-notifications to ensure that progress markers are updated,
    // which allows AwaitFullSyncCompletion to return.  However, in some
    // migration tests these notifications are completely disabled, so the
    // progress markers do not get updated.  This is why we must use the less
    // strict condition, AwaitDataSyncCompletion.
    if (!AwaitDataSyncCompletion(
            "Config sync cycle after migration cycle")) {
      return false;
    }
  }
}

bool ProfileSyncServiceHarness::AwaitMutualSyncCycleCompletion(
    ProfileSyncServiceHarness* partner) {
  DVLOG(1) << GetClientInfoString("AwaitMutualSyncCycleCompletion");
  if (!AwaitFullSyncCompletion("Sync cycle completion on active client."))
    return false;
  return partner->WaitUntilProgressMarkersMatch(this,
      "Sync cycle completion on passive client.");
}

bool ProfileSyncServiceHarness::AwaitGroupSyncCycleCompletion(
    std::vector<ProfileSyncServiceHarness*>& partners) {
  DVLOG(1) << GetClientInfoString("AwaitGroupSyncCycleCompletion");
  if (!AwaitFullSyncCompletion("Sync cycle completion on active client."))
    return false;
  bool return_value = true;
  for (std::vector<ProfileSyncServiceHarness*>::iterator it =
      partners.begin(); it != partners.end(); ++it) {
    if ((this != *it) && ((*it)->wait_state_ != SYNC_DISABLED)) {
      return_value = return_value &&
          (*it)->WaitUntilProgressMarkersMatch(this,
          "Sync cycle completion on partner client.");
    }
  }
  return return_value;
}

// static
bool ProfileSyncServiceHarness::AwaitQuiescence(
    std::vector<ProfileSyncServiceHarness*>& clients) {
  DVLOG(1) << "AwaitQuiescence.";
  bool return_value = true;
  for (std::vector<ProfileSyncServiceHarness*>::iterator it =
      clients.begin(); it != clients.end(); ++it) {
    if ((*it)->wait_state_ != SYNC_DISABLED)
      return_value = return_value &&
          (*it)->AwaitGroupSyncCycleCompletion(clients);
  }
  return return_value;
}

bool ProfileSyncServiceHarness::WaitUntilProgressMarkersMatch(
    ProfileSyncServiceHarness* partner, const std::string& reason) {
  DVLOG(1) << GetClientInfoString("WaitUntilProgressMarkersMatch");
  if (wait_state_ == SYNC_DISABLED) {
    LOG(ERROR) << "Sync disabled for " << profile_debug_name_ << ".";
    return false;
  }

  if (MatchesOtherClient(partner)) {
    // Progress markers already match; don't wait.
    return true;
  }

  DCHECK(!progress_marker_partner_);
  progress_marker_partner_ = partner;
  partner->service()->AddObserver(this);
  wait_state_ = WAITING_FOR_UPDATES;
  bool return_value =
      AwaitStatusChangeWithTimeout(kLiveSyncOperationTimeoutMs, reason);
  partner->service()->RemoveObserver(this);
  progress_marker_partner_ = NULL;
  return return_value;
}

bool ProfileSyncServiceHarness::AwaitStatusChangeWithTimeout(
    int timeout_milliseconds,
    const std::string& reason) {
  DVLOG(1) << GetClientInfoString("AwaitStatusChangeWithTimeout");
  if (wait_state_ == SYNC_DISABLED) {
    LOG(ERROR) << "Sync disabled for " << profile_debug_name_ << ".";
    return false;
  }
  scoped_refptr<StateChangeTimeoutEvent> timeout_signal(
      new StateChangeTimeoutEvent(this, reason));
  {
    // Set the flag to tell SignalStateComplete() that it's OK to quit out of
    // the MessageLoop if we hit a state transition.
    waiting_for_status_change_ = true;
    MessageLoop* loop = MessageLoop::current();
    MessageLoop::ScopedNestableTaskAllower allow(loop);
    loop->PostDelayedTask(
        FROM_HERE,
        base::Bind(&StateChangeTimeoutEvent::Callback,
                   timeout_signal.get()),
        base::TimeDelta::FromMilliseconds(timeout_milliseconds));
    loop->Run();
    waiting_for_status_change_ = false;
  }

  if (timeout_signal->Abort()) {
    DVLOG(1) << GetClientInfoString("AwaitStatusChangeWithTimeout succeeded");
    return true;
  } else {
    DVLOG(0) << GetClientInfoString("AwaitStatusChangeWithTimeout timed out");
    return false;
  }
}

ProfileSyncService::Status ProfileSyncServiceHarness::GetStatus() {
  DCHECK(service() != NULL) << "GetStatus(): service() is NULL.";
  ProfileSyncService::Status result;
  service()->QueryDetailedSyncStatus(&result);
  return result;
}

// We use this function to share code between IsFullySynced and IsDataSynced
// while ensuring that all conditions are evaluated using on the same snapshot.
bool ProfileSyncServiceHarness::IsDataSyncedImpl(
    const SyncSessionSnapshot& snap) {
  return ServiceIsPushingChanges() &&
         GetStatus().notifications_enabled &&
         !service()->HasUnsyncedItems() &&
         !HasPendingBackendMigration();
}

bool ProfileSyncServiceHarness::IsDataSynced() {
  if (service() == NULL) {
    DVLOG(1) << GetClientInfoString("IsDataSynced(): false");
    return false;
  }

  const SyncSessionSnapshot& snap = GetLastSessionSnapshot();
  bool is_data_synced = IsDataSyncedImpl(snap);

  DVLOG(1) << GetClientInfoString(
      is_data_synced ? "IsDataSynced: true" : "IsDataSynced: false");
  return is_data_synced;
}

bool ProfileSyncServiceHarness::IsFullySynced() {
  if (service() == NULL) {
    DVLOG(1) << GetClientInfoString("IsFullySynced: false");
    return false;
  }
  const SyncSessionSnapshot& snap = GetLastSessionSnapshot();
  // If we didn't try to commit anything in the previous cycle, there's a
  // good chance that we're now fully up to date.
  bool is_fully_synced =
      snap.model_neutral_state().num_successful_commits == 0
      && snap.model_neutral_state().commit_result == syncer::SYNCER_OK
      && IsDataSyncedImpl(snap);

  DVLOG(1) << GetClientInfoString(
      is_fully_synced ? "IsFullySynced: true" : "IsFullySynced: false");
  return is_fully_synced;
}

bool ProfileSyncServiceHarness::HasPendingBackendMigration() {
  browser_sync::BackendMigrator* migrator =
      service()->GetBackendMigratorForTest();
  return migrator && migrator->state() != browser_sync::BackendMigrator::IDLE;
}

bool ProfileSyncServiceHarness::AutoStartEnabled() {
  return service_->auto_start_enabled();
}

bool ProfileSyncServiceHarness::MatchesOtherClient(
    ProfileSyncServiceHarness* partner) {
  // TODO(akalin): Shouldn't this belong with the intersection check?
  // Otherwise, this function isn't symmetric.
  if (!IsFullySynced()) {
    DVLOG(2) << profile_debug_name_ << ": not synced, assuming doesn't match";
    return false;
  }

  // Only look for a match if we have at least one enabled datatype in
  // common with the partner client.
  const syncer::ModelTypeSet common_types =
      Intersection(service()->GetPreferredDataTypes(),
                   partner->service()->GetPreferredDataTypes());

  DVLOG(2) << profile_debug_name_ << ", " << partner->profile_debug_name_
           << ": common types are "
           << syncer::ModelTypeSetToString(common_types);

  if (!common_types.Empty() && !partner->IsFullySynced()) {
    DVLOG(2) << "non-empty common types and "
             << partner->profile_debug_name_ << " isn't synced";
    return false;
  }

  for (syncer::ModelTypeSet::Iterator i = common_types.First();
       i.Good(); i.Inc()) {
    const std::string marker = GetSerializedProgressMarker(i.Get());
    const std::string partner_marker =
        partner->GetSerializedProgressMarker(i.Get());
    if (marker != partner_marker) {
      if (VLOG_IS_ON(2)) {
        std::string marker_base64, partner_marker_base64;
        if (!base::Base64Encode(marker, &marker_base64)) {
          NOTREACHED();
        }
        if (!base::Base64Encode(
                partner_marker, &partner_marker_base64)) {
          NOTREACHED();
        }
        DVLOG(2) << syncer::ModelTypeToString(i.Get()) << ": "
                 << profile_debug_name_ << " progress marker = "
                 << marker_base64 << ", "
                 << partner->profile_debug_name_
                 << " partner progress marker = "
                 << partner_marker_base64;
      }
      return false;
    }
  }
  return true;
}

SyncSessionSnapshot ProfileSyncServiceHarness::GetLastSessionSnapshot() const {
  DCHECK(service_ != NULL) << "Sync service has not yet been set up.";
  if (service_->sync_initialized()) {
    return service_->GetLastSessionSnapshot();
  }
  return SyncSessionSnapshot();
}

bool ProfileSyncServiceHarness::EnableSyncForDatatype(
    syncer::ModelType datatype) {
  DVLOG(1) << GetClientInfoString(
      "EnableSyncForDatatype("
      + std::string(syncer::ModelTypeToString(datatype)) + ")");

  if (wait_state_ == SYNC_DISABLED) {
    return SetupSync(syncer::ModelTypeSet(datatype));
  }

  if (service() == NULL) {
    LOG(ERROR) << "EnableSyncForDatatype(): service() is null.";
    return false;
  }

  syncer::ModelTypeSet synced_datatypes = service()->GetPreferredDataTypes();
  if (synced_datatypes.Has(datatype)) {
    DVLOG(1) << "EnableSyncForDatatype(): Sync already enabled for datatype "
             << syncer::ModelTypeToString(datatype)
             << " on " << profile_debug_name_ << ".";
    return true;
  }

  synced_datatypes.Put(syncer::ModelTypeFromInt(datatype));
  service()->OnUserChoseDatatypes(false, synced_datatypes);
  if (AwaitDataSyncCompletion("Datatype configuration.")) {
    DVLOG(1) << "EnableSyncForDatatype(): Enabled sync for datatype "
             << syncer::ModelTypeToString(datatype)
             << " on " << profile_debug_name_ << ".";
    return true;
  }

  DVLOG(0) << GetClientInfoString("EnableSyncForDatatype failed");
  return false;
}

bool ProfileSyncServiceHarness::DisableSyncForDatatype(
    syncer::ModelType datatype) {
  DVLOG(1) << GetClientInfoString(
      "DisableSyncForDatatype("
      + std::string(syncer::ModelTypeToString(datatype)) + ")");

  if (service() == NULL) {
    LOG(ERROR) << "DisableSyncForDatatype(): service() is null.";
    return false;
  }

  syncer::ModelTypeSet synced_datatypes = service()->GetPreferredDataTypes();
  if (!synced_datatypes.Has(datatype)) {
    DVLOG(1) << "DisableSyncForDatatype(): Sync already disabled for datatype "
             << syncer::ModelTypeToString(datatype)
             << " on " << profile_debug_name_ << ".";
    return true;
  }

  synced_datatypes.Remove(datatype);
  service()->OnUserChoseDatatypes(false, synced_datatypes);
  if (AwaitFullSyncCompletion("Datatype reconfiguration.")) {
    DVLOG(1) << "DisableSyncForDatatype(): Disabled sync for datatype "
             << syncer::ModelTypeToString(datatype)
             << " on " << profile_debug_name_ << ".";
    return true;
  }

  DVLOG(0) << GetClientInfoString("DisableSyncForDatatype failed");
  return false;
}

bool ProfileSyncServiceHarness::EnableSyncForAllDatatypes() {
  DVLOG(1) << GetClientInfoString("EnableSyncForAllDatatypes");

  if (wait_state_ == SYNC_DISABLED) {
    return SetupSync();
  }

  if (service() == NULL) {
    LOG(ERROR) << "EnableSyncForAllDatatypes(): service() is null.";
    return false;
  }

  service()->OnUserChoseDatatypes(true, syncer::ModelTypeSet::All());
  if (AwaitFullSyncCompletion("Datatype reconfiguration.")) {
    DVLOG(1) << "EnableSyncForAllDatatypes(): Enabled sync for all datatypes "
             << "on " << profile_debug_name_ << ".";
    return true;
  }

  DVLOG(0) << GetClientInfoString("EnableSyncForAllDatatypes failed");
  return false;
}

bool ProfileSyncServiceHarness::DisableSyncForAllDatatypes() {
  DVLOG(1) << GetClientInfoString("DisableSyncForAllDatatypes");

  if (service() == NULL) {
    LOG(ERROR) << "DisableSyncForAllDatatypes(): service() is null.";
    return false;
  }

  service()->DisableForUser();
  wait_state_ = SYNC_DISABLED;
  DVLOG(1) << "DisableSyncForAllDatatypes(): Disabled sync for all "
           << "datatypes on " << profile_debug_name_;
  return true;
}

std::string ProfileSyncServiceHarness::GetSerializedProgressMarker(
    syncer::ModelType model_type) const {
  const SyncSessionSnapshot& snap = GetLastSessionSnapshot();
  const syncer::ProgressMarkerMap& markers_map =
      snap.download_progress_markers();

  syncer::ProgressMarkerMap::const_iterator it =
      markers_map.find(model_type);
  return (it != markers_map.end()) ? it->second : "";
}

std::string ProfileSyncServiceHarness::GetClientInfoString(
    const std::string& message) {
  std::stringstream os;
  os << profile_debug_name_ << ": " << message << ": ";
  if (service()) {
    const SyncSessionSnapshot& snap = GetLastSessionSnapshot();
    const ProfileSyncService::Status& status = GetStatus();
    // Capture select info from the sync session snapshot and syncer status.
    os << ", has_unsynced_items: "
       << (service()->sync_initialized() ? service()->HasUnsyncedItems() : 0)
       << ", did_commit: "
       << (snap.model_neutral_state().num_successful_commits == 0 &&
           snap.model_neutral_state().commit_result == syncer::SYNCER_OK)
       << ", encryption conflicts: "
       << snap.num_encryption_conflicts()
       << ", hierarchy conflicts: "
       << snap.num_hierarchy_conflicts()
       << ", server conflicts: "
       << snap.num_server_conflicts()
       << ", num_updates_downloaded : "
       << snap.model_neutral_state().num_updates_downloaded_total
       << ", passphrase_required_reason: "
       << syncer::PassphraseRequiredReasonToString(
           service()->passphrase_required_reason())
       << ", notifications_enabled: "
       << status.notifications_enabled
       << ", service_is_pushing_changes: "
       << ServiceIsPushingChanges()
       << ", has_pending_backend_migration: "
       << HasPendingBackendMigration();
  } else {
    os << "Sync service not available";
  }
  return os.str();
}

// TODO(zea): Rename this EnableEncryption, since we no longer turn on
// encryption for individual types but for all.
bool ProfileSyncServiceHarness::EnableEncryptionForType(
    syncer::ModelType type) {
  const syncer::ModelTypeSet encrypted_types =
      service_->GetEncryptedDataTypes();
  if (encrypted_types.Has(type))
    return true;
  service_->EnableEncryptEverything();

  // In order to kick off the encryption we have to reconfigure. Just grab the
  // currently synced types and use them.
  const syncer::ModelTypeSet synced_datatypes =
      service_->GetPreferredDataTypes();
  bool sync_everything =
      synced_datatypes.Equals(syncer::ModelTypeSet::All());
  service_->OnUserChoseDatatypes(sync_everything, synced_datatypes);

  // Wait some time to let the enryption finish.
  return WaitForTypeEncryption(type);
}

bool ProfileSyncServiceHarness::WaitForTypeEncryption(syncer::ModelType type) {
  // The correctness of this if condition depends on the ordering of its
  // sub-expressions.  See crbug.com/95619.
  // TODO(rlarocque): Figure out a less brittle way of detecting this.
  if (IsTypeEncrypted(type) &&
      IsFullySynced() &&
      GetLastSessionSnapshot().num_encryption_conflicts() == 0) {
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

bool ProfileSyncServiceHarness::IsTypeEncrypted(syncer::ModelType type) {
  const syncer::ModelTypeSet encrypted_types =
      service_->GetEncryptedDataTypes();
  bool is_type_encrypted = service_->GetEncryptedDataTypes().Has(type);
  DVLOG(2) << syncer::ModelTypeToString(type) << " is "
           << (is_type_encrypted ? "" : "not ") << "encrypted; "
           << "encrypted types = "
           << syncer::ModelTypeSetToString(encrypted_types);
  return is_type_encrypted;
}

bool ProfileSyncServiceHarness::IsTypeRunning(syncer::ModelType type) {
  browser_sync::DataTypeController::StateMap state_map;
  service_->GetDataTypeControllerStates(&state_map);
  return (state_map.count(type) != 0 &&
          state_map[type] == browser_sync::DataTypeController::RUNNING);
}

bool ProfileSyncServiceHarness::IsTypePreferred(syncer::ModelType type) {
  return service_->GetPreferredDataTypes().Has(type);
}

size_t ProfileSyncServiceHarness::GetNumEntries() const {
  return GetLastSessionSnapshot().num_entries();
}

size_t ProfileSyncServiceHarness::GetNumDatatypes() const {
  browser_sync::DataTypeController::StateMap state_map;
  service_->GetDataTypeControllerStates(&state_map);
  return state_map.size();
}

std::string ProfileSyncServiceHarness::GetServiceStatus() {
  scoped_ptr<DictionaryValue> value(
      sync_ui_util::ConstructAboutInformation(service_));
  std::string service_status;
  base::JSONWriter::WriteWithOptions(value.get(),
                                     base::JSONWriter::OPTIONS_PRETTY_PRINT,
                                     &service_status);
  return service_status;
}
