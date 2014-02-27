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
#include "base/message_loop/message_loop.h"
#include "base/prefs/pref_service.h"
#include "base/strings/stringprintf.h"
#include "base/timer/timer.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/invalidation/p2p_invalidation_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/profile_oauth2_token_service.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_base.h"
#include "chrome/browser/sync/about_sync_util.h"
#include "chrome/browser/sync/backend_migrator.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/test/integration/p2p_invalidation_forwarder.h"
#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"
#include "chrome/browser/sync/test/integration/status_change_checker.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/sync_driver/data_type_controller.h"
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
// TODO(sync): This timeout must eventually be made less than the default 45
// second timeout for integration tests so that in case a sync operation times
// out, it is able to log a useful failure message before the test is killed.
static const int kSyncOperationTimeoutMs = 45000;

namespace {

// Checks if a desired change in the state of the sync engine has taken place by
// running the callback passed to it.
class CallbackStatusChecker : public SingleClientStatusChangeChecker {
 public:
  CallbackStatusChecker(ProfileSyncService* service,
                        base::Callback<bool()> callback,
                        const std::string& debug_message)
      : SingleClientStatusChangeChecker(service),
        callback_(callback),
        debug_message_(debug_message) {
  }

  virtual ~CallbackStatusChecker() {
  }

  virtual bool IsExitConditionSatisfied() OVERRIDE {
    return callback_.Run();
  }

  virtual std::string GetDebugMessage() const OVERRIDE {
    return debug_message_;
  }

 private:
  // Callback that evaluates whether the condition we are waiting on has been
  // satisfied.
  base::Callback<bool()> callback_;

  const std::string debug_message_;

  DISALLOW_COPY_AND_ASSIGN(CallbackStatusChecker);
};

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

// Helper function which returns true if sync setup is complete, or in case
// it is blocked for some reason.
bool DoneWaitingForSyncSetup(const ProfileSyncServiceHarness* harness) {
  DCHECK(harness);
  // Sync setup is complete, and the client is ready to sync new changes.
  if (harness->ServiceIsPushingChanges())
    return true;
  // Sync is blocked because a custom passphrase is required.
  if (harness->service()->passphrase_required_reason() ==
      syncer::REASON_DECRYPTION) {
    return true;
  }
  // Sync is blocked by an auth error.
  if (harness->HasAuthError())
    return true;
  // Still waiting on sync setup.
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
      progress_marker_partner_(NULL),
      username_(username),
      password_(password),
      oauth2_refesh_token_number_(0),
      profile_debug_name_(profile->GetDebugName()),
      status_change_checker_(NULL) {
  // Start listening for and emitting notifications of commits.
  p2p_invalidation_forwarder_.reset(
      new P2PInvalidationForwarder(service_, p2p_invalidation_service));
}

ProfileSyncServiceHarness::~ProfileSyncServiceHarness() { }

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
  if (!AwaitSyncSetupCompletion()) {
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

void ProfileSyncServiceHarness::QuitMessageLoop() {
  base::MessageLoop::current()->QuitWhenIdle();
}

void ProfileSyncServiceHarness::OnStateChanged() {
  if (!status_change_checker_)
    return;

  DVLOG(1) << GetClientInfoString(status_change_checker_->GetDebugMessage());
  if (status_change_checker_->IsExitConditionSatisfied())
    QuitMessageLoop();
}

void ProfileSyncServiceHarness::OnSyncCycleCompleted() {
  OnStateChanged();
}

bool ProfileSyncServiceHarness::AwaitPassphraseRequired() {
  DVLOG(1) << GetClientInfoString("AwaitPassphraseRequired");
  CallbackStatusChecker passphrase_required_checker(
      service(),
      base::Bind(&::IsPassphraseRequired, base::Unretained(this)),
      "IsPassphraseRequired");
  return AwaitStatusChange(&passphrase_required_checker);
}

bool ProfileSyncServiceHarness::AwaitPassphraseAccepted() {
  CallbackStatusChecker passphrase_accepted_checker(
      service(),
      base::Bind(&::IsPassphraseAccepted, base::Unretained(this)),
      "IsPassphraseAccepted");
  bool return_value = AwaitStatusChange(&passphrase_accepted_checker);
  if (return_value)
    FinishSyncSetup();
  return return_value;
}

bool ProfileSyncServiceHarness::AwaitBackendInitialized() {
  DVLOG(1) << GetClientInfoString("AwaitBackendInitialized");
  CallbackStatusChecker backend_initialized_checker(
      service(),
      base::Bind(&DoneWaitingForBackendInitialization,
                 base::Unretained(this)),
      "DoneWaitingForBackendInitialization");
  AwaitStatusChange(&backend_initialized_checker);
  return service()->sync_initialized();
}

// TODO(sync): As of today, we wait for a client to finish its commit activity
// by checking if its progress markers are up to date. In future, once we have
// an in-process C++ server, this function can be reimplemented without relying
// on progress markers.
bool ProfileSyncServiceHarness::AwaitCommitActivityCompletion() {
  DVLOG(1) << GetClientInfoString("AwaitCommitActivityCompletion");
  CallbackStatusChecker latest_progress_markers_checker(
      service(),
      base::Bind(&ProfileSyncServiceHarness::HasLatestProgressMarkers,
                 base::Unretained(this)),
      "HasLatestProgressMarkers");
  AwaitStatusChange(&latest_progress_markers_checker);
  return HasLatestProgressMarkers();
}

bool ProfileSyncServiceHarness::AwaitSyncDisabled() {
  DCHECK(service()->HasSyncSetupCompleted());
  DCHECK(!IsSyncDisabled());
  CallbackStatusChecker sync_disabled_checker(
      service(),
      base::Bind(&ProfileSyncServiceHarness::IsSyncDisabled,
                 base::Unretained(this)),
      "IsSyncDisabled");
  return AwaitStatusChange(&sync_disabled_checker);
}

bool ProfileSyncServiceHarness::AwaitSyncSetupCompletion() {
  CallbackStatusChecker sync_setup_complete_checker(
      service(),
      base::Bind(&DoneWaitingForSyncSetup, base::Unretained(this)),
      "DoneWaitingForSyncSetup");
  return AwaitStatusChange(&sync_setup_complete_checker);
}

bool ProfileSyncServiceHarness::AwaitMutualSyncCycleCompletion(
    ProfileSyncServiceHarness* partner) {
  DVLOG(1) << GetClientInfoString("AwaitMutualSyncCycleCompletion");
  if (!AwaitCommitActivityCompletion())
    return false;
  return partner->WaitUntilProgressMarkersMatch(this);
}

bool ProfileSyncServiceHarness::AwaitGroupSyncCycleCompletion(
    std::vector<ProfileSyncServiceHarness*>& partners) {
  DVLOG(1) << GetClientInfoString("AwaitGroupSyncCycleCompletion");
  if (!AwaitCommitActivityCompletion())
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
    CallbackStatusChecker matches_other_client_checker(
        service(),
        base::Bind(&ProfileSyncServiceHarness::MatchesPartnerClient,
                   base::Unretained(this)),
        "MatchesPartnerClient");
    return_value = AwaitStatusChange(&matches_other_client_checker);
    partner->service()->RemoveObserver(this);
  }
  progress_marker_partner_ = NULL;
  return return_value;
}

bool ProfileSyncServiceHarness::AwaitStatusChange(
    StatusChangeChecker* checker) {
  DVLOG(1) << GetClientInfoString("AwaitStatusChange");

  if (IsSyncDisabled()) {
    LOG(ERROR) << "Sync disabled for " << profile_debug_name_ << ".";
    return false;
  }

  DCHECK(checker);
  if (checker->IsExitConditionSatisfied()) {
    DVLOG(1) << GetClientInfoString("AwaitStatusChange exiting early because "
                                    "condition is already satisfied");
    return true;
  }

  DCHECK(status_change_checker_ == NULL);
  status_change_checker_ = checker;
  status_change_checker_->InitObserver(this);

  base::OneShotTimer<ProfileSyncServiceHarness> timer;
  timer.Start(FROM_HERE,
              base::TimeDelta::FromMilliseconds(kSyncOperationTimeoutMs),
              base::Bind(&ProfileSyncServiceHarness::QuitMessageLoop,
                         base::Unretained(this)));
  {
    base::MessageLoop* loop = base::MessageLoop::current();
    base::MessageLoop::ScopedNestableTaskAllower allow(loop);
    loop->Run();
  }

  status_change_checker_->UninitObserver(this);
  status_change_checker_ = NULL;

  if (timer.IsRunning()) {
    DVLOG(1) << GetClientInfoString("AwaitStatusChange succeeded");
    return true;
  } else {
    LOG(ERROR) << GetClientInfoString(base::StringPrintf(
        "AwaitStatusChange called from %s timed out",
        checker->GetDebugMessage().c_str()));
    CHECK(false) << "Ending test because of timeout.";
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

// TODO(sync): Remove this method once we stop relying on self notifications and
// comparing progress markers.
bool ProfileSyncServiceHarness::HasLatestProgressMarkers() const {
  const SyncSessionSnapshot& snap = GetLastSessionSnapshot();
  return snap.model_neutral_state().num_successful_commits == 0 &&
         !service()->HasUnsyncedItems();
}

void ProfileSyncServiceHarness::FinishSyncSetup() {
  service()->SetSetupInProgress(false);
  service()->SetSyncSetupCompleted();
}

bool ProfileSyncServiceHarness::AutoStartEnabled() {
  return service()->auto_start_enabled();
}

bool ProfileSyncServiceHarness::MatchesPartnerClient() const {
  DCHECK(progress_marker_partner_);

  // Only look for a match if we have at least one enabled datatype in
  // common with the partner client.
  const syncer::ModelTypeSet common_types =
      Intersection(service()->GetActiveDataTypes(),
                   progress_marker_partner_->service()->GetActiveDataTypes());

  DVLOG(2) << profile_debug_name_ << ", "
           << progress_marker_partner_->profile_debug_name_
           << ": common types are "
           << syncer::ModelTypeSetToString(common_types);

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
  if (AwaitSyncSetupCompletion()) {
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
  if (AwaitSyncSetupCompletion()) {
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
  if (AwaitSyncSetupCompletion()) {
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

// TODO(sync): Clean up this method in a separate CL. Remove all snapshot fields
// and log shorter, more meaningful messages.
std::string ProfileSyncServiceHarness::GetClientInfoString(
    const std::string& message) const {
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
       << ServiceIsPushingChanges();
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

  CallbackStatusChecker encryption_complete_checker(
      service(),
      base::Bind(&ProfileSyncServiceHarness::IsEncryptionComplete,
                 base::Unretained(this)),
      "IsEncryptionComplete");
  return AwaitStatusChange(&encryption_complete_checker);
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

std::string ProfileSyncServiceHarness::GetServiceStatus() {
  scoped_ptr<base::DictionaryValue> value(
      sync_ui_util::ConstructAboutInformation(service()));
  std::string service_status;
  base::JSONWriter::WriteWithOptions(value.get(),
                                     base::JSONWriter::OPTIONS_PRETTY_PRINT,
                                     &service_status);
  return service_status;
}
