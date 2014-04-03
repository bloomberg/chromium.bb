// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/managed/managed_user_creation_controller_old.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/sys_info.h"
#include "base/task_runner_util.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/values.h"
#include "chrome/browser/chromeos/login/managed/locally_managed_user_constants.h"
#include "chrome/browser/chromeos/login/managed/supervised_user_authentication.h"
#include "chrome/browser/chromeos/login/mount_manager.h"
#include "chrome/browser/chromeos/login/supervised_user_manager.h"
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

const int kUserCreationTimeoutSeconds = 30;  // 30 seconds.

bool StoreManagedUserFiles(const std::string& token,
                           const base::FilePath& base_path) {
  if (!base::SysInfo::IsRunningOnChromeOS()) {
    // If running on desktop, cryptohome stub does not create home directory.
    base::CreateDirectory(base_path);
  }
  base::FilePath token_file = base_path.Append(kManagedUserTokenFilename);
  int bytes = base::WriteFile(token_file, token.c_str(), token.length());
  return bytes >= 0;
}

}  // namespace

ManagedUserCreationControllerOld::UserCreationContext::UserCreationContext()
    : avatar_index(kDummyAvatarIndex),
      token_acquired(false),
      token_succesfully_written(false),
      creation_type(NEW_USER),
      manager_profile(NULL) {}

ManagedUserCreationControllerOld::UserCreationContext::~UserCreationContext() {}

ManagedUserCreationControllerOld::ManagedUserCreationControllerOld(
    ManagedUserCreationControllerOld::StatusConsumer* consumer,
    const std::string& manager_id)
    : ManagedUserCreationController(consumer), weak_factory_(this) {
  creation_context_.reset(
      new ManagedUserCreationControllerOld::UserCreationContext());
  creation_context_->manager_id = manager_id;
}

ManagedUserCreationControllerOld::~ManagedUserCreationControllerOld() {}

void ManagedUserCreationControllerOld::StartCreation(
    const base::string16& display_name,
    const std::string& password,
    int avatar_index) {
  DCHECK(creation_context_);
  creation_context_->display_name = display_name;
  creation_context_->password = password;
  creation_context_->avatar_index = avatar_index;
  StartCreation();
}

void ManagedUserCreationControllerOld::StartImport(
    const base::string16& display_name,
    const std::string& password,
    int avatar_index,
    const std::string& sync_id,
    const std::string& master_key) {
  DCHECK(creation_context_);
  creation_context_->creation_type = USER_IMPORT;
  creation_context_->display_name = display_name;
  creation_context_->password = password;
  creation_context_->avatar_index = avatar_index;
  creation_context_->sync_user_id = sync_id;
  creation_context_->master_key = master_key;
  StartCreation();
}

void ManagedUserCreationControllerOld::StartImport(
    const base::string16& display_name,
    int avatar_index,
    const std::string& sync_id,
    const std::string& master_key,
    const base::DictionaryValue* password_data,
    const std::string& encryption_key,
    const std::string& signature_key) {
  NOTREACHED();
}

void ManagedUserCreationControllerOld::SetManagerProfile(
    Profile* manager_profile) {
  creation_context_->manager_profile = manager_profile;
}

Profile* ManagedUserCreationControllerOld::GetManagerProfile() {
  return creation_context_->manager_profile;
}

void ManagedUserCreationControllerOld::StartCreation() {
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
      FROM_HERE,
      base::TimeDelta::FromSeconds(kUserCreationTimeoutSeconds),
      this,
      &ManagedUserCreationControllerOld::CreationTimedOut);
  SupervisedUserManager* manager =
      UserManager::Get()->GetSupervisedUserManager();
  manager->StartCreationTransaction(creation_context_->display_name);

  creation_context_->local_user_id = manager->GenerateUserId();

  if (creation_context_->creation_type == NEW_USER) {
    creation_context_->sync_user_id =
        ManagedUserRegistrationUtility::GenerateNewManagedUserId();
  }

  manager->CreateUserRecord(creation_context_->manager_id,
                            creation_context_->local_user_id,
                            creation_context_->sync_user_id,
                            creation_context_->display_name);

  manager->SetCreationTransactionUserId(creation_context_->local_user_id);
  SupervisedUserAuthentication* authentication = manager->GetAuthentication();
  base::DictionaryValue extra;
  if (authentication->FillDataForNewUser(creation_context_->local_user_id,
                                         creation_context_->password,
                                         &creation_context_->password_data,
                                         &extra)) {
    authentication->StorePasswordData(creation_context_->local_user_id,
                                      creation_context_->password_data);
  }
  VLOG(1) << "Creating cryptohome";
  authenticator_ = new ManagedUserAuthenticator(this);
  authenticator_->AuthenticateToCreate(
      creation_context_->local_user_id,
      authentication->TransformPassword(creation_context_->local_user_id,
                                        creation_context_->password));
}

void ManagedUserCreationControllerOld::OnAuthenticationFailure(
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

void ManagedUserCreationControllerOld::OnMountSuccess(
    const std::string& mount_hash) {
  creation_context_->mount_hash = mount_hash;

  SupervisedUserAuthentication* authentication =
      UserManager::Get()->GetSupervisedUserManager()->GetAuthentication();

  if (creation_context_->creation_type == NEW_USER) {
    // Generate master password.
    creation_context_->master_key = authentication->GenerateMasterKey();
  }

  VLOG(1) << "Adding master key";

  authenticator_->AddMasterKey(
      creation_context_->local_user_id,
      authentication->TransformPassword(creation_context_->local_user_id,
                                        creation_context_->password),
      creation_context_->master_key);
}

void ManagedUserCreationControllerOld::OnAddKeySuccess() {
  creation_context_->registration_utility =
      ManagedUserRegistrationUtility::Create(
          creation_context_->manager_profile);

  VLOG(1) << "Creating user on server";
  // TODO(antrim) : add password data to sync once API is ready.
  // http://crbug.com/316168
  ManagedUserRegistrationInfo info(creation_context_->display_name,
                                   creation_context_->avatar_index);
  info.master_key = creation_context_->master_key;
  timeout_timer_.Stop();
  creation_context_->registration_utility->Register(
      creation_context_->sync_user_id,
      info,
      base::Bind(&ManagedUserCreationControllerOld::RegistrationCallback,
                 weak_factory_.GetWeakPtr()));
}

void ManagedUserCreationControllerOld::RegistrationCallback(
    const GoogleServiceAuthError& error,
    const std::string& token) {
  if (error.state() == GoogleServiceAuthError::NONE) {
    timeout_timer_.Start(
        FROM_HERE,
        base::TimeDelta::FromSeconds(kUserCreationTimeoutSeconds),
        this,
        &ManagedUserCreationControllerOld::CreationTimedOut);
    TokenFetched(token);
  } else {
    LOG(ERROR) << "Managed user creation failed. Error code " << error.state();
    if (consumer_)
      consumer_->OnCreationError(CLOUD_SERVER_ERROR);
  }
}

void ManagedUserCreationControllerOld::CreationTimedOut() {
  LOG(ERROR) << "Supervised user creation timed out.";
  if (consumer_)
    consumer_->OnCreationTimeout();
}

void ManagedUserCreationControllerOld::FinishCreation() {
  chrome::AttemptUserExit();
}

void ManagedUserCreationControllerOld::CancelCreation() {
  creation_context_->registration_utility.reset();
  chrome::AttemptUserExit();
}

std::string ManagedUserCreationControllerOld::GetManagedUserId() {
  DCHECK(creation_context_);
  return creation_context_->local_user_id;
}

void ManagedUserCreationControllerOld::TokenFetched(const std::string& token) {
  creation_context_->token_acquired = true;
  creation_context_->token = token;

  PostTaskAndReplyWithResult(
      content::BrowserThread::GetBlockingPool(),
      FROM_HERE,
      base::Bind(&StoreManagedUserFiles,
                 creation_context_->token,
                 MountManager::GetHomeDir(creation_context_->mount_hash)),
      base::Bind(&ManagedUserCreationControllerOld::OnManagedUserFilesStored,
                 weak_factory_.GetWeakPtr()));
}

void ManagedUserCreationControllerOld::OnManagedUserFilesStored(bool success) {
  timeout_timer_.Stop();

  content::RecordAction(
      base::UserMetricsAction("ManagedMode_LocallyManagedUserCreated"));

  if (!success) {
    if (consumer_)
      consumer_->OnCreationError(TOKEN_WRITE_FAILED);
    return;
  }
  // Assume that new token is valid. It will be automatically invalidated if
  // sync service fails to use it.
  UserManager::Get()->SaveUserOAuthStatus(creation_context_->local_user_id,
                                          User::OAUTH2_TOKEN_STATUS_VALID);
  UserManager::Get()->GetSupervisedUserManager()->CommitCreationTransaction();
  if (consumer_)
    consumer_->OnCreationSuccess();
}

}  // namespace chromeos
