// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"

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
#include "base/message_loop/message_loop.h"
#include "base/prefs/pref_service.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/invalidation/p2p_invalidation_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/profile_oauth2_token_service.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_base.h"
#include "chrome/browser/sync/about_sync_util.h"
#include "chrome/browser/sync/glue/data_type_controller.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/test/integration/status_change_checker.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_service.h"
#include "google_apis/gaia/gaia_constants.h"
#include "sync/internal_api/public/base/progress_marker_map.h"
#include "sync/internal_api/public/sessions/sync_session_snapshot.h"
#include "sync/internal_api/public/util/sync_string_conversions.h"

#if defined(ENABLE_MANAGED_USERS)
#include "chrome/browser/managed_mode/managed_user_constants.h"
#endif

using syncer::sessions::SyncSessionSnapshot;
using invalidation::P2PInvalidationService;

// The amount of time for which we wait for a sync operation to complete.
static const int kSyncOperationTimeoutMs = 45000;

// Simple class to implement a timeout using PostDelayedTask.  If it is not
// aborted before picked up by a message queue, then it asserts.  This class is
// not thread safe.
class StateChangeTimeoutEvent
    : public base::RefCountedThreadSafe<StateChangeTimeoutEvent> {
 public:
  explicit StateChangeTimeoutEvent(ProfileSyncServiceHarness* caller);

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

  DISALLOW_COPY_AND_ASSIGN(StateChangeTimeoutEvent);
};

StateChangeTimeoutEvent::StateChangeTimeoutEvent(
    ProfileSyncServiceHarness* caller)
    : aborted_(false), did_timeout_(false), caller_(caller) {
}

StateChangeTimeoutEvent::~StateChangeTimeoutEvent() {
}

void StateChangeTimeoutEvent::Callback() {
  if (!aborted_) {
    DCHECK(caller_->status_change_checker_);
    // TODO(rsimha): Simply return false on timeout instead of repeating the
    // exit condition check.
    if (!caller_->status_change_checker_->IsExitConditionSatisfied()) {
      did_timeout_ = true;
      DCHECK(!aborted_);
      caller_->SignalStateComplete();
    }
  }
}

bool StateChangeTimeoutEvent::Abort() {
  aborted_ = true;
  caller_ = NULL;
  return !did_timeout_;
}

namespace {

// Helper function which returns true if the sync backend has been initialized,
// or if backend initialization was blocked for some reason.
bool DoneWaitingForBackendInitialization(
    const ProfileSyncServiceHarness* harness) {
  DCHECK(harness);
  // Backend is initialized.
  if (harness->service()->sync_initialized())
    return true;
  // Backend initialization is blocked by an auth error.
  if (harness->HasAuthError())
    return true;
  // Backend initialization is blocked by a failure to fetch Oauth2 tokens.
  if (harness->service()->IsRetryingAccessTokenFetchForTest())
    return true;
  // Still waiting on backend initialization.
  return false;
}

// Helper function which returns true if initial sync is complete, or if the
// initial sync is blocked for some reason.
bool DoneWaitingForInitialSync(const ProfileSyncServiceHarness* harness) {
  DCHECK(harness);
  // Initial sync is complete.
  if (harness->IsFullySynced())
    return true;
  // Initial sync is blocked because custom passphrase is required.
  if (harness->service()->passphrase_required_reason() ==
      syncer::REASON_DECRYPTION) {
    return true;
  }
  // Initial sync is blocked by an auth error.
  if (harness->HasAuthError())
    return true;
  // Still waiting on initial sync.
  return false;
}

// Helper function which returns true if the sync client is fully synced, or if
// sync is blocked for some reason.
bool DoneWaitingForFullSync(const ProfileSyncServiceHarness* harness) {
  DCHECK(harness);
  // Sync is complete.
  if (harness->IsFullySynced())
    return true;
  // Sync is blocked by an auth error.
  if (harness->HasAuthError())
    return true;
  // Sync is blocked by a failure to fetch Oauth2 tokens.
  if (harness->service()->IsRetryingAccessTokenFetchForTest())
    return true;
  // Still waiting on sync.
  return false;
}

// Helper function which returns true if the sync client requires a custom
// passphrase to be entered for decryption.
bool IsPassphraseRequired(const ProfileSyncServiceHarness* harness) {
  DCHECK(harness);
  return harness->service()->IsPassphraseRequired();
}

// Helper function which returns true if the custom passphrase entered was
// accepted.
bool IsPassphraseAccepted(const ProfileSyncServiceHarness* harness) {
  DCHECK(harness);
  return (!harness->service()->IsPassphraseRequired() &&
          harness->service()->IsUsingSecondaryPassphrase());
}

// Helper function which returns true if the sync client no longer has a pending
// backend migration.
bool NoPendingBackendMigration(const ProfileSyncServiceHarness* harness) {
  DCHECK(harness);
  return !harness->HasPendingBackendMigration();
}

// Helper function which returns true if the client has detected an actionable
// error.
bool HasActionableError(const ProfileSyncServiceHarness* harness) {
  DCHECK(harness);
  ProfileSyncService::Status status = harness->GetStatus();
  return (status.sync_protocol_error.action != syncer::UNKNOWN_ACTION &&
          harness->service()->HasUnrecoverableError() == true);
}

}  // namespace

// static
ProfileSyncServiceHarness* ProfileSyncServiceHarness::Create(
    Profile* profile,
    const std::string& username,
    const std::string& password) {
  return new ProfileSyncServiceHarness(profile, username, password, NULL);
}

// static
ProfileSyncServiceHarness* ProfileSyncServiceHarness::CreateForIntegrationTest(
    Profile* profile,
    const std::string& username,
    const std::string& password,
    P2PInvalidationService* p2p_invalidation_service) {
  return new ProfileSyncServiceHarness(profile,
                                       username,
                                       password,
                                       p2p_invalidation_service);
}

ProfileSyncServiceHarness::ProfileSyncServiceHarness(
    Profile* profile,
    const std::string& username,
    const std::string& password,
    P2PInvalidationService* p2p_invalidation_service)
    : profile_(profile),
      service_(ProfileSyncServiceFactory::GetForProfile(profile)),
      p2p_invalidation_service_(p2p_invalidation_service),
      progress_marker_partner_(NULL),
      username_(username),
      password_(password),
      oauth2_refesh_token_number_(0),
      profile_debug_name_(profile->GetDebugName()),
      status_change_checker_(NULL) {
}

ProfileSyncServiceHarness::~ProfileSyncServiceHarness() {
  if (service()->HasObserver(this))
    service()->RemoveObserver(this);
}

void ProfileSyncServiceHarness::SetCredentials(const std::string& username,
                                               const std::string& password) {
  username_ = username;
  password_ = password;
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
  if (service() == NULL) {
    LOG(ERROR) << "SetupSync(): service() is null.";
    return false;
  }

  // Subscribe sync client to notifications from the profile sync service.
  if (!service()->HasObserver(this))
    service()->AddObserver(this);

  // Tell the sync service that setup is in progress so we don't start syncing
  // until we've finished configuration.
  service()->SetSetupInProgress(true);

  // Authenticate sync client using GAIA credentials.
  service()->signin()->SetAuthenticatedUsername(username_);
  profile_->GetPrefs()->SetString(prefs::kGoogleServicesUsername,
                                  username_);
  GoogleServiceSigninSuccessDetails details(username_, password_);
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_GOOGLE_SIGNIN_SUCCESSFUL,
      content::Source<Profile>(profile_),
      content::Details<const GoogleServiceSigninSuccessDetails>(&details));

#if defined(ENABLE_MANAGED_USERS)
  std::string account_id = profile_->IsManaged() ?
      managed_users::kManagedUserPseudoEmail : username_;
#else
  std::string account_id = username_;
#endif
  DCHECK(!account_id.empty());
  ProfileOAuth2TokenServiceFactory::GetForProfile(profile_)->
      UpdateCredentials(account_id, GenerateFakeOAuth2RefreshTokenString());

  // Wait for the OnBackendInitialized() callback.
  if (!AwaitBackendInitialized()) {
    LOG(ERROR) << "OnBackendInitialized() not seen after "
               << kSyncOperationTimeoutMs / 1000
               << " seconds.";
    return false;
  }

  // Make sure that initial sync wasn't blocked by a missing passphrase.
  if (service()->passphrase_required_reason() == syncer::REASON_DECRYPTION) {
    LOG(ERROR) << "A passphrase is required for decryption. Sync cannot proceed"
                  " until SetDecryptionPassphrase is called.";
    return false;
  }

  // Make sure that initial sync wasn't blocked by rejected credentials.
  if (HasAuthError()) {
    LOG(ERROR) << "Credentials were rejected. Sync cannot proceed.";
    return false;
  }

  // Choose the datatypes to be synced. If all datatypes are to be synced,
  // set sync_everything to true; otherwise, set it to false.
  bool sync_everything =
      synced_datatypes.Equals(syncer::ModelTypeSet::All());
  service()->OnUserChoseDatatypes(sync_everything, synced_datatypes);

  // Notify ProfileSyncService that we are done with configuration.
  FinishSyncSetup();

  // Subscribe sync client to notifications from the backend migrator
  // (possible only after choosing data types).
  if (!TryListeningToMigrationEvents()) {
    NOTREACHED();
    return false;
  }

  // Set an implicit passphrase for encryption if an explicit one hasn't already
  // been set. If an explicit passphrase has been set, immediately return false,
  // since a decryption passphrase is required.
  if (!service()->IsUsingSecondaryPassphrase()) {
    service()->SetEncryptionPassphrase(password_, ProfileSyncService::IMPLICIT);
  } else {
    LOG(ERROR) << "A passphrase is required for decryption. Sync cannot proceed"
                  " until SetDecryptionPassphrase is called.";
    return false;
  }

  // Wait for initial sync cycle to be completed.
  DCHECK(service()->sync_initialized());
  StatusChangeChecker initial_sync_checker(
      base::Bind(&DoneWaitingForInitialSync, base::Unretained(this)),
      "DoneWaitingForInitialSync");
  if (!AwaitStatusChange(&initial_sync_checker, "SetupSync")) {
    LOG(ERROR) << "Initial sync cycle did not complete after "
               << kSyncOperationTimeoutMs / 1000
               << " seconds.";
    return false;
  }

  // Make sure that initial sync wasn't blocked by a missing passphrase.
  if (service()->passphrase_required_reason() == syncer::REASON_DECRYPTION) {
    LOG(ERROR) << "A passphrase is required for decryption. Sync cannot proceed"
                  " until SetDecryptionPassphrase is called.";
    return false;
  }

  // Make sure that initial sync wasn't blocked by rejected credentials.
  if (service()->GetAuthError().state() ==
      GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS) {
    LOG(ERROR) << "Credentials were rejected. Sync cannot proceed.";
    return false;
  }

  return true;
}

bool ProfileSyncServiceHarness::TryListeningToMigrationEvents() {
  browser_sync::BackendMigrator* migrator =
      service()->GetBackendMigratorForTest();
  if (migrator && !migrator->HasMigrationObserver(this)) {
    migrator->AddMigrationObserver(this);
    return true;
  }
  return false;
}

void ProfileSyncServiceHarness::SignalStateComplete() {
  base::MessageLoop::current()->QuitWhenIdle();
}

void ProfileSyncServiceHarness::OnStateChanged() {
  if (!status_change_checker_)
    return;

  DVLOG(1) << GetClientInfoString(status_change_checker_->callback_name());
  if (status_change_checker_->IsExitConditionSatisfied())
    base::MessageLoop::current()->QuitWhenIdle();
}

void ProfileSyncServiceHarness::OnSyncCycleCompleted() {
  // Integration tests still use p2p notifications.
  const SyncSessionSnapshot& snap = GetLastSessionSnapshot();
  bool is_notifiable_commit =
      (snap.model_neutral_state().num_successful_commits > 0);
  if (is_notifiable_commit && p2p_invalidation_service_) {
    syncer::ModelTypeSet model_types =
        snap.model_neutral_state().commit_request_types;
    syncer::ObjectIdSet ids = ModelTypeSetToObjectIdSet(model_types);
    p2p_invalidation_service_->SendInvalidation(ids);
  }
  OnStateChanged();
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
  OnStateChanged();
}

bool ProfileSyncServiceHarness::AwaitPassphraseRequired() {
  DVLOG(1) << GetClientInfoString("AwaitPassphraseRequired");
  if (IsSyncDisabled()) {
    LOG(ERROR) << "Sync disabled for " << profile_debug_name_ << ".";
    return false;
  }

  if (service()->IsPassphraseRequired()) {
    // It's already true that a passphrase is required; don't wait.
    return true;
  }

  StatusChangeChecker passphrase_required_checker(
      base::Bind(&::IsPassphraseRequired, base::Unretained(this)),
      "IsPassphraseRequired");
  return AwaitStatusChange(&passphrase_required_checker,
                           "AwaitPassphraseRequired");
}

bool ProfileSyncServiceHarness::AwaitPassphraseAccepted() {
  DVLOG(1) << GetClientInfoString("AwaitPassphraseAccepted");
  if (IsSyncDisabled()) {
    LOG(ERROR) << "Sync disabled for " << profile_debug_name_ << ".";
    return false;
  }

  if (!service()->IsPassphraseRequired() &&
      service()->IsUsingSecondaryPassphrase()) {
    // Passphrase is already accepted; don't wait.
    FinishSyncSetup();
    return true;
  }

  StatusChangeChecker passphrase_accepted_checker(
      base::Bind(&::IsPassphraseAccepted, base::Unretained(this)),
      "IsPassphraseAccepted");
  bool return_value = AwaitStatusChange(&passphrase_accepted_checker,
                                        "AwaitPassphraseAccepted");
  if (return_value)
    FinishSyncSetup();
  return return_value;
}

bool ProfileSyncServiceHarness::AwaitBackendInitialized() {
  DVLOG(1) << GetClientInfoString("AwaitBackendInitialized");
  if (service()->sync_initialized()) {
    // The sync backend host has already been initialized; don't wait.
    return true;
  }

  StatusChangeChecker backend_initialized_checker(
      base::Bind(&DoneWaitingForBackendInitialization,
                 base::Unretained(this)),
      "DoneWaitingForBackendInitialization");
  AwaitStatusChange(&backend_initialized_checker, "AwaitBackendInitialized");
  return service()->sync_initialized();
}

bool ProfileSyncServiceHarness::AwaitDataSyncCompletion() {
  DVLOG(1) << GetClientInfoString("AwaitDataSyncCompletion");

  DCHECK(service()->sync_initialized());
  DCHECK(!IsSyncDisabled());

  if (IsDataSynced()) {
    // Client is already synced; don't wait.
    return true;
  }

  StatusChangeChecker data_synced_checker(
      base::Bind(&ProfileSyncServiceHarness::IsDataSynced,
                 base::Unretained(this)),
      "IsDataSynced");
  return AwaitStatusChange(&data_synced_checker, "AwaitDataSyncCompletion");
}

bool ProfileSyncServiceHarness::AwaitFullSyncCompletion() {
  DVLOG(1) << GetClientInfoString("AwaitFullSyncCompletion");
  if (IsSyncDisabled()) {
    LOG(ERROR) << "Sync disabled for " << profile_debug_name_ << ".";
    return false;
  }

  if (IsFullySynced()) {
    // Client is already synced; don't wait.
    return true;
  }

  DCHECK(service()->sync_initialized());
  StatusChangeChecker fully_synced_checker(
      base::Bind(&DoneWaitingForFullSync, base::Unretained(this)),
      "DoneWaitingForFullSync");
  AwaitStatusChange(&fully_synced_checker, "AwaitFullSyncCompletion");
  return IsFullySynced();
}

bool ProfileSyncServiceHarness::AwaitSyncDisabled() {
  DCHECK(service()->HasSyncSetupCompleted());
  DCHECK(!IsSyncDisabled());
  StatusChangeChecker sync_disabled_checker(
      base::Bind(&ProfileSyncServiceHarness::IsSyncDisabled,
                 base::Unretained(this)),
      "IsSyncDisabled");
  return AwaitStatusChange(&sync_disabled_checker, "AwaitSyncDisabled");
}

// TODO(rsimha): Move this method from PSSHarness to the class that uses it.
bool ProfileSyncServiceHarness::AwaitExponentialBackoffVerification() {
  const SyncSessionSnapshot& snap = GetLastSessionSnapshot();
  retry_verifier_.reset(new RetryVerifier());
  retry_verifier_->Initialize(snap);
  StatusChangeChecker exponential_backoff_checker(
      base::Bind(&ProfileSyncServiceHarness::IsExponentialBackoffDone,
                 base::Unretained(this)),
      "IsExponentialBackoffDone");
  AwaitStatusChange(&exponential_backoff_checker,
                    "AwaitExponentialBackoffVerification");
  return retry_verifier_->Succeeded();
}

bool ProfileSyncServiceHarness::AwaitActionableError() {
  ProfileSyncService::Status status = GetStatus();
  CHECK(status.sync_protocol_error.action == syncer::UNKNOWN_ACTION);
  StatusChangeChecker actionable_error_checker(
      base::Bind(&HasActionableError, base::Unretained(this)),
      "HasActionableError");
  return AwaitStatusChange(&actionable_error_checker, "AwaitActionableError");
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

    if (!HasPendingBackendMigration()) {
      StatusChangeChecker pending_migration_checker(
          base::Bind(&ProfileSyncServiceHarness::HasPendingBackendMigration,
                     base::Unretained(this)),
          "HasPendingBackendMigration");
      if (!AwaitStatusChange(&pending_migration_checker,
                             "AwaitMigration Start")) {
        DVLOG(1) << profile_debug_name_ << ": Migration did not start";
        return false;
      }
    }

    StatusChangeChecker migration_finished_checker(
        base::Bind(&::NoPendingBackendMigration, base::Unretained(this)),
        "NoPendingBackendMigration");
    if (!AwaitStatusChange(&migration_finished_checker,
                           "AwaitMigration Finish")) {
      DVLOG(1) << profile_debug_name_ << ": Migration did not finish";
      return false;
    }

    // We must use AwaitDataSyncCompletion rather than the more common
    // AwaitFullSyncCompletion.  As long as crbug.com/97780 is open, we will
    // rely on self-notifications to ensure that progress markers are updated,
    // which allows AwaitFullSyncCompletion to return.  However, in some
    // migration tests these notifications are completely disabled, so the
    // progress markers do not get updated.  This is why we must use the less
    // strict condition, AwaitDataSyncCompletion.
    if (!AwaitDataSyncCompletion())
      return false;
  }
}

bool ProfileSyncServiceHarness::AwaitMutualSyncCycleCompletion(
    ProfileSyncServiceHarness* partner) {
  DVLOG(1) << GetClientInfoString("AwaitMutualSyncCycleCompletion");
  if (!AwaitFullSyncCompletion())
    return false;
  return partner->WaitUntilProgressMarkersMatch(this);
}

bool ProfileSyncServiceHarness::AwaitGroupSyncCycleCompletion(
    std::vector<ProfileSyncServiceHarness*>& partners) {
  DVLOG(1) << GetClientInfoString("AwaitGroupSyncCycleCompletion");
  if (!AwaitFullSyncCompletion())
    return false;
  bool return_value = true;
  for (std::vector<ProfileSyncServiceHarness*>::iterator it =
      partners.begin(); it != partners.end(); ++it) {
    if ((this != *it) && (!(*it)->IsSyncDisabled())) {
      return_value = return_value &&
          (*it)->WaitUntilProgressMarkersMatch(this);
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
    if (!(*it)->IsSyncDisabled()) {
      return_value = return_value &&
          (*it)->AwaitGroupSyncCycleCompletion(clients);
    }
  }
  return return_value;
}

bool ProfileSyncServiceHarness::WaitUntilProgressMarkersMatch(
    ProfileSyncServiceHarness* partner) {
  DVLOG(1) << GetClientInfoString("WaitUntilProgressMarkersMatch");
  if (IsSyncDisabled()) {
    LOG(ERROR) << "Sync disabled for " << profile_debug_name_ << ".";
    return false;
  }

  // TODO(rsimha): Replace the mechanism of matching up progress markers with
  // one that doesn't require every client to have the same progress markers.
  DCHECK(!progress_marker_partner_);
  progress_marker_partner_ = partner;
  bool return_value = false;
  if (MatchesPartnerClient()) {
    // Progress markers already match; don't wait.
    return_value = true;
  } else {
    partner->service()->AddObserver(this);
    StatusChangeChecker matches_other_client_checker(
        base::Bind(&ProfileSyncServiceHarness::MatchesPartnerClient,
                   base::Unretained(this)),
        "MatchesPartnerClient");
    return_value = AwaitStatusChange(&matches_other_client_checker,
                                     "WaitUntilProgressMarkersMatch");
    partner->service()->RemoveObserver(this);
  }
  progress_marker_partner_ = NULL;
  return return_value;
}

bool ProfileSyncServiceHarness::AwaitStatusChange(
    StatusChangeChecker* checker, const std::string& source) {
  DVLOG(1) << GetClientInfoString("AwaitStatusChange");

  if (IsSyncDisabled()) {
    LOG(ERROR) << "Sync disabled for " << profile_debug_name_ << ".";
    return false;
  }

  DCHECK(status_change_checker_ == NULL);
  status_change_checker_ = checker;

  scoped_refptr<StateChangeTimeoutEvent> timeout_signal(
      new StateChangeTimeoutEvent(this));
  {
    base::MessageLoop* loop = base::MessageLoop::current();
    base::MessageLoop::ScopedNestableTaskAllower allow(loop);
    loop->PostDelayedTask(
        FROM_HERE,
        base::Bind(&StateChangeTimeoutEvent::Callback,
                   timeout_signal.get()),
        base::TimeDelta::FromMilliseconds(kSyncOperationTimeoutMs));
    loop->Run();
  }

  status_change_checker_ = NULL;

  if (timeout_signal->Abort()) {
    DVLOG(1) << GetClientInfoString("AwaitStatusChange succeeded");
    return true;
  } else {
    DVLOG(0) << GetClientInfoString(base::StringPrintf(
        "AwaitStatusChange called from %s timed out", source.c_str()));
    return false;
  }
}

std::string ProfileSyncServiceHarness::GenerateFakeOAuth2RefreshTokenString() {
  return base::StringPrintf("oauth2_refresh_token_%d",
                            ++oauth2_refesh_token_number_);
}

ProfileSyncService::Status ProfileSyncServiceHarness::GetStatus() const {
  DCHECK(service() != NULL) << "GetStatus(): service() is NULL.";
  ProfileSyncService::Status result;
  service()->QueryDetailedSyncStatus(&result);
  return result;
}

bool ProfileSyncServiceHarness::IsExponentialBackoffDone() const {
  const SyncSessionSnapshot& snap = GetLastSessionSnapshot();
  retry_verifier_->VerifyRetryInterval(snap);
  return (retry_verifier_->done());
}

bool ProfileSyncServiceHarness::IsSyncDisabled() const {
  return !service()->setup_in_progress() &&
         !service()->HasSyncSetupCompleted();
}

bool ProfileSyncServiceHarness::HasAuthError() const {
  return service()->GetAuthError().state() ==
             GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS ||
         service()->GetAuthError().state() ==
             GoogleServiceAuthError::SERVICE_ERROR ||
         service()->GetAuthError().state() ==
             GoogleServiceAuthError::REQUEST_CANCELED;
}

// We use this function to share code between IsFullySynced and IsDataSynced.
bool ProfileSyncServiceHarness::IsDataSyncedImpl() const {
  return ServiceIsPushingChanges() &&
         GetStatus().notifications_enabled &&
         !service()->HasUnsyncedItems() &&
         !HasPendingBackendMigration();
}

bool ProfileSyncServiceHarness::IsDataSynced() const {
  if (service() == NULL) {
    DVLOG(1) << GetClientInfoString("IsDataSynced(): false");
    return false;
  }

  bool is_data_synced = IsDataSyncedImpl();

  DVLOG(1) << GetClientInfoString(
      is_data_synced ? "IsDataSynced: true" : "IsDataSynced: false");
  return is_data_synced;
}

bool ProfileSyncServiceHarness::IsFullySynced() const {
  if (service() == NULL) {
    DVLOG(1) << GetClientInfoString("IsFullySynced: false");
    return false;
  }
  // If we didn't try to commit anything in the previous cycle, there's a
  // good chance that we're now fully up to date.
  const SyncSessionSnapshot& snap = GetLastSessionSnapshot();
  bool is_fully_synced =
      snap.model_neutral_state().num_successful_commits == 0
      && snap.model_neutral_state().commit_result == syncer::SYNCER_OK
      && IsDataSyncedImpl();

  DVLOG(1) << GetClientInfoString(
      is_fully_synced ? "IsFullySynced: true" : "IsFullySynced: false");
  return is_fully_synced;
}

void ProfileSyncServiceHarness::FinishSyncSetup() {
  service()->SetSetupInProgress(false);
  service()->SetSyncSetupCompleted();
}

bool ProfileSyncServiceHarness::HasPendingBackendMigration() const {
  browser_sync::BackendMigrator* migrator =
      service()->GetBackendMigratorForTest();
  return migrator && migrator->state() != browser_sync::BackendMigrator::IDLE;
}

bool ProfileSyncServiceHarness::AutoStartEnabled() {
  return service()->auto_start_enabled();
}

bool ProfileSyncServiceHarness::MatchesPartnerClient() const {
  // TODO(akalin): Shouldn't this belong with the intersection check?
  // Otherwise, this function isn't symmetric.
  DCHECK(progress_marker_partner_);
  if (!IsFullySynced()) {
    DVLOG(2) << profile_debug_name_ << ": not synced, assuming doesn't match";
    return false;
  }

  // Only look for a match if we have at least one enabled datatype in
  // common with the partner client.
  const syncer::ModelTypeSet common_types =
      Intersection(service()->GetActiveDataTypes(),
                   progress_marker_partner_->service()->GetActiveDataTypes());

  DVLOG(2) << profile_debug_name_ << ", "
           << progress_marker_partner_->profile_debug_name_
           << ": common types are "
           << syncer::ModelTypeSetToString(common_types);

  if (!common_types.Empty() && !progress_marker_partner_->IsFullySynced()) {
    DVLOG(2) << "non-empty common types and "
             << progress_marker_partner_->profile_debug_name_
             << " isn't synced";
    return false;
  }

  for (syncer::ModelTypeSet::Iterator i = common_types.First();
       i.Good(); i.Inc()) {
    const std::string marker = GetSerializedProgressMarker(i.Get());
    const std::string partner_marker =
        progress_marker_partner_->GetSerializedProgressMarker(i.Get());
    if (marker != partner_marker) {
      if (VLOG_IS_ON(2)) {
        std::string marker_base64, partner_marker_base64;
        base::Base64Encode(marker, &marker_base64);
        base::Base64Encode(partner_marker, &partner_marker_base64);
        DVLOG(2) << syncer::ModelTypeToString(i.Get()) << ": "
                 << profile_debug_name_ << " progress marker = "
                 << marker_base64 << ", "
                 << progress_marker_partner_->profile_debug_name_
                 << " partner progress marker = "
                 << partner_marker_base64;
      }
      return false;
    }
  }
  return true;
}

SyncSessionSnapshot ProfileSyncServiceHarness::GetLastSessionSnapshot() const {
  DCHECK(service() != NULL) << "Sync service has not yet been set up.";
  if (service()->sync_initialized()) {
    return service()->GetLastSessionSnapshot();
  }
  return SyncSessionSnapshot();
}

bool ProfileSyncServiceHarness::EnableSyncForDatatype(
    syncer::ModelType datatype) {
  DVLOG(1) << GetClientInfoString(
      "EnableSyncForDatatype("
      + std::string(syncer::ModelTypeToString(datatype)) + ")");

  if (IsSyncDisabled())
    return SetupSync(syncer::ModelTypeSet(datatype));

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
  if (AwaitDataSyncCompletion()) {
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

  synced_datatypes.RetainAll(syncer::UserSelectableTypes());
  synced_datatypes.Remove(datatype);
  service()->OnUserChoseDatatypes(false, synced_datatypes);
  if (AwaitFullSyncCompletion()) {
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

  if (IsSyncDisabled())
    return SetupSync();

  if (service() == NULL) {
    LOG(ERROR) << "EnableSyncForAllDatatypes(): service() is null.";
    return false;
  }

  service()->OnUserChoseDatatypes(true, syncer::ModelTypeSet::All());
  if (AwaitFullSyncCompletion()) {
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
  return (it != markers_map.end()) ? it->second : std::string();
}

std::string ProfileSyncServiceHarness::GetClientInfoString(
    const std::string& message) const {
  std::stringstream os;
  os << profile_debug_name_ << ": " << message << ": ";
  if (service()) {
    const SyncSessionSnapshot& snap = GetLastSessionSnapshot();
    const ProfileSyncService::Status& status = GetStatus();
    // Capture select info from the sync session snapshot and syncer status.
    // TODO(rsimha): Audit the list of fields below, and eventually eliminate
    // the use of the sync session snapshot. See crbug.com/323380.
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

bool ProfileSyncServiceHarness::EnableEncryption() {
  if (IsEncryptionComplete())
    return true;
  service()->EnableEncryptEverything();

  // In order to kick off the encryption we have to reconfigure. Just grab the
  // currently synced types and use them.
  const syncer::ModelTypeSet synced_datatypes =
      service()->GetPreferredDataTypes();
  bool sync_everything =
      synced_datatypes.Equals(syncer::ModelTypeSet::All());
  service()->OnUserChoseDatatypes(sync_everything, synced_datatypes);

  // Wait some time to let the enryption finish.
  return WaitForEncryption();
}

bool ProfileSyncServiceHarness::WaitForEncryption() {
  if (IsEncryptionComplete()) {
    // Encryption is already complete; do not wait.
    return true;
  }

  StatusChangeChecker encryption_complete_checker(
      base::Bind(&ProfileSyncServiceHarness::IsEncryptionComplete,
                 base::Unretained(this)),
      "IsEncryptionComplete");
  return AwaitStatusChange(&encryption_complete_checker, "WaitForEncryption");
}

bool ProfileSyncServiceHarness::IsEncryptionComplete() const {
  bool is_encryption_complete = service()->EncryptEverythingEnabled() &&
                                !service()->encryption_pending();
  DVLOG(2) << "Encryption is "
           << (is_encryption_complete ? "" : "not ")
           << "complete; Encrypted types = "
           << syncer::ModelTypeSetToString(service()->GetEncryptedDataTypes());
  return is_encryption_complete;
}

bool ProfileSyncServiceHarness::IsTypeRunning(syncer::ModelType type) {
  browser_sync::DataTypeController::StateMap state_map;
  service()->GetDataTypeControllerStates(&state_map);
  return (state_map.count(type) != 0 &&
          state_map[type] == browser_sync::DataTypeController::RUNNING);
}

bool ProfileSyncServiceHarness::IsTypePreferred(syncer::ModelType type) {
  return service()->GetPreferredDataTypes().Has(type);
}

size_t ProfileSyncServiceHarness::GetNumEntries() const {
  return GetLastSessionSnapshot().num_entries();
}

size_t ProfileSyncServiceHarness::GetNumDatatypes() const {
  browser_sync::DataTypeController::StateMap state_map;
  service()->GetDataTypeControllerStates(&state_map);
  return state_map.size();
}

std::string ProfileSyncServiceHarness::GetServiceStatus() {
  scoped_ptr<DictionaryValue> value(
      sync_ui_util::ConstructAboutInformation(service()));
  std::string service_status;
  base::JSONWriter::WriteWithOptions(value.get(),
                                     base::JSONWriter::OPTIONS_PRETTY_PRINT,
                                     &service_status);
  return service_status;
}
