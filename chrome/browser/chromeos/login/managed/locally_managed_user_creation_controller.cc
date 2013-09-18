// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/managed/locally_managed_user_creation_controller.h"

#include "base/bind.h"
#include "base/chromeos/chromeos_version.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task_runner_util.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/values.h"
#include "chrome/browser/chromeos/login/managed/locally_managed_user_constants.h"
#include "chrome/browser/chromeos/login/mount_manager.h"
#include "chrome/browser/chromeos/login/user.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager_client.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/user_metrics.h"
#include "crypto/random.h"
#include "google_apis/gaia/google_service_auth_error.h"

namespace chromeos {

namespace {

const int kDummyAvatarIndex = -111;
const int kMasterKeySize = 32;
const int kUserCreationTimeoutSeconds = 30; // 30 seconds.

bool StoreManagedUserFiles(const std::string& token,
                           const base::FilePath& base_path) {
  if (!base::chromeos::IsRunningOnChromeOS()) {
    // If running on desktop, cryptohome stub does not create home directory.
    file_util::CreateDirectory(base_path);
  }
  base::FilePath token_file = base_path.Append(kManagedUserTokenFilename);
  int bytes = file_util::WriteFile(token_file, token.c_str(), token.length());
  return bytes >= 0;
}

} // namespace

LocallyManagedUserCreationController::StatusConsumer::~StatusConsumer() {}

LocallyManagedUserCreationController::UserCreationContext::UserCreationContext()
    : token_acquired(false),
      token_succesfully_written(false),
      manager_profile(NULL) {}

LocallyManagedUserCreationController::UserCreationContext::
    ~UserCreationContext() {}

// static
LocallyManagedUserCreationController*
    LocallyManagedUserCreationController::current_controller_ = NULL;

LocallyManagedUserCreationController::LocallyManagedUserCreationController(
    LocallyManagedUserCreationController::StatusConsumer* consumer,
    const std::string& manager_id)
    : consumer_(consumer),
      weak_factory_(this) {
  DCHECK(!current_controller_) << "More than one controller exist.";
  current_controller_ = this;
  creation_context_.reset(
      new LocallyManagedUserCreationController::UserCreationContext());
  creation_context_->manager_id = manager_id;
}

LocallyManagedUserCreationController::~LocallyManagedUserCreationController() {
  current_controller_ = NULL;
}

void LocallyManagedUserCreationController::SetUpCreation(string16 display_name,
                                                         std::string password) {
  DCHECK(creation_context_);
  creation_context_->display_name = display_name;
  creation_context_->password = password;
}

void LocallyManagedUserCreationController::SetManagerProfile(
    Profile* manager_profile) {
  creation_context_->manager_profile = manager_profile;
}

void LocallyManagedUserCreationController::StartCreation() {
  DCHECK(creation_context_);
  VLOG(1) << "Starting supervised user creation";

  ProfileSyncService* sync_service =
      ProfileSyncServiceFactory::GetInstance()->GetForProfile(
          creation_context_->manager_profile);
  ProfileSyncService::SyncStatusSummary status =
      sync_service->QuerySyncStatusSummary();

  if (status == ProfileSyncService::DATATYPES_NOT_INITIALIZED)
    consumer_->OnLongCreationWarning();

  timeout_timer_.Start(
      FROM_HERE, base::TimeDelta::FromSeconds(kUserCreationTimeoutSeconds),
      this,
      &LocallyManagedUserCreationController::CreationTimedOut);

  UserManager::Get()->StartLocallyManagedUserCreationTransaction(
      creation_context_->display_name);

  creation_context_->local_user_id =
        UserManager::Get()->GenerateUniqueLocallyManagedUserId();
  creation_context_->sync_user_id =
      ManagedUserRegistrationUtility::GenerateNewManagedUserId();

  UserManager::Get()->CreateLocallyManagedUserRecord(
      creation_context_->manager_id,
      creation_context_->local_user_id,
      creation_context_->sync_user_id,
      creation_context_->display_name);

  UserManager::Get()->SetLocallyManagedUserCreationTransactionUserId(
      creation_context_->local_user_id);
  VLOG(1) << "Creating cryptohome";
  authenticator_ = new ManagedUserAuthenticator(this);
  authenticator_->AuthenticateToCreate(creation_context_->local_user_id,
                                       creation_context_->password);
}

void LocallyManagedUserCreationController::OnAuthenticationFailure(
    ManagedUserAuthenticator::AuthState error) {
  timeout_timer_.Stop();
  ErrorCode code = NO_ERROR;
  switch (error) {
    case ManagedUserAuthenticator::NO_MOUNT:
      code = CRYPTOHOME_NO_MOUNT;
      break;
    case ManagedUserAuthenticator::FAILED_MOUNT:
      code = CRYPTOHOME_FAILED_MOUNT;
      break;
    case ManagedUserAuthenticator::FAILED_TPM:
      code = CRYPTOHOME_FAILED_TPM;
      break;
    default:
      NOTREACHED();
  }
  if (consumer_)
    consumer_->OnCreationError(code);
}

void LocallyManagedUserCreationController::OnMountSuccess(
    const std::string& mount_hash) {
  creation_context_->mount_hash = mount_hash;

  // Generate master password.
  char master_key_bytes[kMasterKeySize];
  crypto::RandBytes(&master_key_bytes, sizeof(master_key_bytes));
  creation_context_->master_key = StringToLowerASCII(base::HexEncode(
      reinterpret_cast<const void*>(master_key_bytes),
      sizeof(master_key_bytes)));
  VLOG(1) << "Adding master key";
  authenticator_->AddMasterKey(creation_context_->local_user_id,
                               creation_context_->password,
                               creation_context_->master_key);
}

void LocallyManagedUserCreationController::OnAddKeySuccess() {
  creation_context_->registration_utility =
      ManagedUserRegistrationUtility::Create(
          creation_context_->manager_profile);

  VLOG(1) << "Creating user on server";
  ManagedUserRegistrationInfo info(creation_context_->display_name,
                                   kDummyAvatarIndex);
  info.master_key = creation_context_->master_key;
  timeout_timer_.Stop();
  creation_context_->registration_utility->Register(
      creation_context_->sync_user_id,
      info,
      base::Bind(&LocallyManagedUserCreationController::RegistrationCallback,
                 weak_factory_.GetWeakPtr()));
}

void LocallyManagedUserCreationController::RegistrationCallback(
    const GoogleServiceAuthError& error,
    const std::string& token) {
  if (error.state() == GoogleServiceAuthError::NONE) {
    timeout_timer_.Start(
        FROM_HERE, base::TimeDelta::FromSeconds(kUserCreationTimeoutSeconds),
        this,
        &LocallyManagedUserCreationController::CreationTimedOut);
    TokenFetched(token);
  } else {
    LOG(ERROR) << "Managed user creation failed. Error code " << error.state();
    if (consumer_)
      consumer_->OnCreationError(CLOUD_SERVER_ERROR);
  }
}

void LocallyManagedUserCreationController::CreationTimedOut() {
  LOG(ERROR) << "Supervised user creation timed out.";
  if (consumer_)
    consumer_->OnCreationTimeout();
}

void LocallyManagedUserCreationController::FinishCreation() {
  chrome::AttemptUserExit();
}

void LocallyManagedUserCreationController::CancelCreation() {
  creation_context_->registration_utility.reset();
  chrome::AttemptUserExit();
}

std::string LocallyManagedUserCreationController::GetManagedUserId() {
  DCHECK(creation_context_);
  return creation_context_->local_user_id;
}

void LocallyManagedUserCreationController::TokenFetched(
    const std::string& token) {
  creation_context_->token_acquired = true;
  creation_context_->token = token;

  PostTaskAndReplyWithResult(
      content::BrowserThread::GetBlockingPool(),
      FROM_HERE,
      base::Bind(&StoreManagedUserFiles,
                 creation_context_->token,
                 MountManager::GetHomeDir(creation_context_->mount_hash)),
      base::Bind(
           &LocallyManagedUserCreationController::OnManagedUserFilesStored,
           weak_factory_.GetWeakPtr()));
}

void LocallyManagedUserCreationController::OnManagedUserFilesStored(
    bool success) {
  timeout_timer_.Stop();

  content::RecordAction(
      content::UserMetricsAction("ManagedMode_LocallyManagedUserCreated"));

  if (!success) {
    if (consumer_)
      consumer_->OnCreationError(TOKEN_WRITE_FAILED);
    return;
  }
  // Assume that new token is valid. It will be automatically invalidated if
  // sync service fails to use it.
  UserManager::Get()->SaveUserOAuthStatus(creation_context_->local_user_id,
                                          User::OAUTH2_TOKEN_STATUS_VALID);
  UserManager::Get()->CommitLocallyManagedUserCreationTransaction();
  if (consumer_)
    consumer_->OnCreationSuccess();
}

}  // namespace chromeos
