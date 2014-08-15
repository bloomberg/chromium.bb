// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"

#include <cstddef>
#include <iterator>
#include <ostream>
#include <sstream>
#include <vector>

#include "base/compiler_specific.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/timer/timer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/sync/about_sync_util.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/test/integration/quiesce_status_change_checker.h"
#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/invalidation/p2p_invalidation_service.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager_base.h"
#include "components/sync_driver/data_type_controller.h"
#include "google_apis/gaia/gaia_constants.h"
#include "sync/internal_api/public/base/progress_marker_map.h"
#include "sync/internal_api/public/util/sync_string_conversions.h"

#if defined(ENABLE_MANAGED_USERS)
#include "chrome/browser/supervised_user/supervised_user_constants.h"
#endif

using syncer::sessions::SyncSessionSnapshot;

namespace {

bool HasAuthError(ProfileSyncService* service) {
  return service->GetAuthError().state() ==
             GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS ||
         service->GetAuthError().state() ==
             GoogleServiceAuthError::SERVICE_ERROR ||
         service->GetAuthError().state() ==
             GoogleServiceAuthError::REQUEST_CANCELED;
}

class BackendInitializeChecker : public SingleClientStatusChangeChecker {
 public:
  explicit BackendInitializeChecker(ProfileSyncService* service)
      : SingleClientStatusChangeChecker(service) {}

  virtual bool IsExitConditionSatisfied() OVERRIDE {
    if (service()->backend_mode() != ProfileSyncService::SYNC)
      return false;
    if (service()->sync_initialized())
      return true;
    // Backend initialization is blocked by an auth error.
    if (HasAuthError(service()))
      return true;
    // Backend initialization is blocked by a failure to fetch Oauth2 tokens.
    if (service()->IsRetryingAccessTokenFetchForTest())
      return true;
    // Still waiting on backend initialization.
    return false;
  }

  virtual std::string GetDebugMessage() const OVERRIDE {
    return "Backend Initialize";
  }
};

class SyncSetupChecker : public SingleClientStatusChangeChecker {
 public:
  explicit SyncSetupChecker(ProfileSyncService* service)
      : SingleClientStatusChangeChecker(service) {}

  virtual bool IsExitConditionSatisfied() OVERRIDE {
    // Sync setup is complete, and the client is ready to sync new changes.
    if (service()->ShouldPushChanges())
      return true;
    // Sync is blocked because a custom passphrase is required.
    if (service()->passphrase_required_reason() == syncer::REASON_DECRYPTION)
      return true;
    // Sync is blocked by an auth error.
    if (HasAuthError(service()))
      return true;
    // Still waiting on sync setup.
    return false;
  }

  virtual std::string GetDebugMessage() const OVERRIDE {
    return "Sync Setup";
  }
};

bool AwaitSyncSetupCompletion(ProfileSyncService* service) {
  SyncSetupChecker checker(service);
  checker.Wait();
  return !checker.TimedOut();
}

}  // namespace

// static
ProfileSyncServiceHarness* ProfileSyncServiceHarness::Create(
    Profile* profile,
    const std::string& username,
    const std::string& password) {
  return new ProfileSyncServiceHarness(profile, username, password);
}

ProfileSyncServiceHarness::ProfileSyncServiceHarness(
    Profile* profile,
    const std::string& username,
    const std::string& password)
    : profile_(profile),
      service_(ProfileSyncServiceFactory::GetForProfile(profile)),
      username_(username),
      password_(password),
      oauth2_refesh_token_number_(0),
      profile_debug_name_(profile->GetDebugName()) {
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
  service()->GoogleSigninSucceeded(username_, password_);

#if defined(ENABLE_MANAGED_USERS)
  std::string account_id = profile_->IsSupervised() ?
      supervised_users::kSupervisedUserPseudoEmail : username_;
#else
  std::string account_id = username_;
#endif
  DCHECK(!account_id.empty());
  ProfileOAuth2TokenServiceFactory::GetForProfile(profile_)->
      UpdateCredentials(account_id, GenerateFakeOAuth2RefreshTokenString());

  // Wait for the OnBackendInitialized() callback.
  BackendInitializeChecker checker(service());
  checker.Wait();

  if (checker.TimedOut()) {
    LOG(ERROR) << "OnBackendInitialized() timed out.";
    return false;
  }

  if (!service()->sync_initialized()) {
    return false;
  }

  // Make sure that initial sync wasn't blocked by a missing passphrase.
  if (service()->passphrase_required_reason() == syncer::REASON_DECRYPTION) {
    LOG(ERROR) << "A passphrase is required for decryption. Sync cannot proceed"
                  " until SetDecryptionPassphrase is called.";
    return false;
  }

  // Make sure that initial sync wasn't blocked by rejected credentials.
  if (HasAuthError(service())) {
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
  if (!AwaitSyncSetupCompletion(service())) {
    LOG(ERROR) << "Initial sync cycle timed out.";
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

bool ProfileSyncServiceHarness::AwaitMutualSyncCycleCompletion(
    ProfileSyncServiceHarness* partner) {
  std::vector<ProfileSyncServiceHarness*> harnesses;
  harnesses.push_back(this);
  harnesses.push_back(partner);
  return AwaitQuiescence(harnesses);
}

bool ProfileSyncServiceHarness::AwaitGroupSyncCycleCompletion(
    std::vector<ProfileSyncServiceHarness*>& partners) {
  return AwaitQuiescence(partners);
}

// static
bool ProfileSyncServiceHarness::AwaitQuiescence(
    std::vector<ProfileSyncServiceHarness*>& clients) {
  std::vector<ProfileSyncService*> services;
  if (clients.empty()) {
    return true;
  }

  for (std::vector<ProfileSyncServiceHarness*>::iterator it = clients.begin();
       it != clients.end(); ++it) {
    services.push_back((*it)->service());
  }
  QuiesceStatusChangeChecker checker(services);
  checker.Wait();
  return !checker.TimedOut();
}

std::string ProfileSyncServiceHarness::GenerateFakeOAuth2RefreshTokenString() {
  return base::StringPrintf("oauth2_refresh_token_%d",
                            ++oauth2_refesh_token_number_);
}

bool ProfileSyncServiceHarness::IsSyncDisabled() const {
  return !service()->setup_in_progress() &&
         !service()->HasSyncSetupCompleted();
}

void ProfileSyncServiceHarness::FinishSyncSetup() {
  service()->SetSetupInProgress(false);
  service()->SetSyncSetupCompleted();
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
  if (AwaitSyncSetupCompletion(service())) {
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
  if (AwaitSyncSetupCompletion(service())) {
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
  if (AwaitSyncSetupCompletion(service())) {
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

// TODO(sync): Clean up this method in a separate CL. Remove all snapshot fields
// and log shorter, more meaningful messages.
std::string ProfileSyncServiceHarness::GetClientInfoString(
    const std::string& message) const {
  std::stringstream os;
  os << profile_debug_name_ << ": " << message << ": ";
  if (service()) {
    const SyncSessionSnapshot& snap = GetLastSessionSnapshot();
    ProfileSyncService::Status status;
    service()->QueryDetailedSyncStatus(&status);
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
       << service()->ShouldPushChanges();
  } else {
    os << "Sync service not available";
  }
  return os.str();
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
