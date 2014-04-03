// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/managed/managed_user_creation_controller_new.h"

#include "base/base64.h"
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
#include "chromeos/cryptohome/cryptohome_parameters.h"
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

ManagedUserCreationControllerNew::ManagedUserCreationControllerNew(
    ManagedUserCreationControllerNew::StatusConsumer* consumer,
    const std::string& manager_id)
    : ManagedUserCreationController(consumer),
      stage_(STAGE_INITIAL),
      weak_factory_(this) {
  creation_context_.reset(
      new ManagedUserCreationControllerNew::UserCreationContext());
  creation_context_->manager_id = manager_id;
}

ManagedUserCreationControllerNew::~ManagedUserCreationControllerNew() {}

ManagedUserCreationControllerNew::UserCreationContext::UserCreationContext() {}

ManagedUserCreationControllerNew::UserCreationContext::~UserCreationContext() {}

void ManagedUserCreationControllerNew::SetManagerProfile(
    Profile* manager_profile) {
  creation_context_->manager_profile = manager_profile;
}

void ManagedUserCreationControllerNew::StartCreation(
    const base::string16& display_name,
    const std::string& password,
    int avatar_index) {
  DCHECK(creation_context_);
  creation_context_->creation_type = NEW_USER;
  creation_context_->display_name = display_name;
  creation_context_->password = password;
  creation_context_->avatar_index = avatar_index;
  StartCreationImpl();
}

void ManagedUserCreationControllerNew::StartImport(
    const base::string16& display_name,
    const std::string& password,
    int avatar_index,
    const std::string& sync_id,
    const std::string& master_key) {
  DCHECK(creation_context_);
  creation_context_->creation_type = USER_IMPORT_OLD;

  creation_context_->display_name = display_name;
  creation_context_->password = password;
  creation_context_->avatar_index = avatar_index;

  creation_context_->sync_user_id = sync_id;

  creation_context_->master_key = master_key;
  StartCreationImpl();
}

void ManagedUserCreationControllerNew::StartImport(
    const base::string16& display_name,
    const std::string& password,
    int avatar_index,
    const std::string& sync_id,
    const std::string& master_key,
    const base::DictionaryValue* password_data,
    const std::string& encryption_key,
    const std::string& signature_key) {
  DCHECK(creation_context_);
  creation_context_->creation_type = USER_IMPORT_OLD;

  creation_context_->display_name = display_name;
  creation_context_->password = password;
  creation_context_->avatar_index = avatar_index;

  creation_context_->sync_user_id = sync_id;

  creation_context_->master_key = master_key;

  password_data->GetStringWithoutPathExpansion(
      kEncryptedPassword, &creation_context_->salted_password);

  creation_context_->signature_key = signature_key;
  creation_context_->encryption_key = encryption_key;

  creation_context_->password_data.MergeDictionary(password_data);

  StartCreationImpl();
}

void ManagedUserCreationControllerNew::StartCreationImpl() {
  DCHECK(creation_context_);
  DCHECK_EQ(STAGE_INITIAL, stage_);
  VLOG(1) << "Starting supervised user creation";
  VLOG(1) << " Phase 1 : Prepare keys";

  SupervisedUserManager* manager =
      UserManager::Get()->GetSupervisedUserManager();
  manager->StartCreationTransaction(creation_context_->display_name);

  creation_context_->local_user_id = manager->GenerateUserId();
  if (creation_context_->creation_type == NEW_USER) {
    creation_context_->sync_user_id =
        ManagedUserRegistrationUtility::GenerateNewManagedUserId();
  }

  manager->SetCreationTransactionUserId(creation_context_->local_user_id);

  stage_ = TRANSACTION_STARTED;

  manager->CreateUserRecord(creation_context_->manager_id,
                            creation_context_->local_user_id,
                            creation_context_->sync_user_id,
                            creation_context_->display_name);

  SupervisedUserAuthentication* authentication =
      UserManager::Get()->GetSupervisedUserManager()->GetAuthentication();

  // When importing M35+ users we need only to store data, for all other cases
  // we need to create some keys.
  if (creation_context_->creation_type != USER_IMPORT_NEW) {
    // Of all required keys old imported users have only master key.
    // Otherwise they are the same as newly created users in terms of keys.
    if (creation_context_->creation_type == NEW_USER) {
      creation_context_->master_key = authentication->GenerateMasterKey();
    }

    base::DictionaryValue extra;
    authentication->FillDataForNewUser(creation_context_->local_user_id,
                                       creation_context_->password,
                                       &creation_context_->password_data,
                                       &extra);
    creation_context_->password_data.GetStringWithoutPathExpansion(
        kEncryptedPassword, &creation_context_->salted_password);
    extra.GetStringWithoutPathExpansion(kPasswordEncryptionKey,
                                        &creation_context_->encryption_key);
    extra.GetStringWithoutPathExpansion(kPasswordSignatureKey,
                                        &creation_context_->signature_key);
  }

  authentication->StorePasswordData(creation_context_->local_user_id,
                                    creation_context_->password_data);
  stage_ = KEYS_GENERATED;

  VLOG(1) << " Phase 2 : Create cryptohome";

  timeout_timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromSeconds(kUserCreationTimeoutSeconds),
      this,
      &ManagedUserCreationControllerNew::CreationTimedOut);
  authenticator_ = new ExtendedAuthenticator(this);
  authenticator_->HashPasswordWithSalt(
      creation_context_->master_key,
      base::Bind(&ManagedUserCreationControllerNew::OnPasswordHashingSuccess,
                 weak_factory_.GetWeakPtr()));
}

void ManagedUserCreationControllerNew::OnPasswordHashingSuccess(
    const std::string& password_hash) {
  VLOG(1) << " Phase 2.1 : Got hashed master key";
  creation_context_->salted_master_key = password_hash;

  // Create home dir with two keys.
  std::vector<cryptohome::KeyDefinition> keys;

  // Main key is the master key. Just as keys for plain GAIA users, it is salted
  // with system salt. It has all usual privileges.
  cryptohome::KeyDefinition master_key(creation_context_->salted_master_key,
                                       kCryptohomeMasterKeyLabel,
                                       cryptohome::PRIV_DEFAULT);

  keys.push_back(master_key);
  authenticator_->CreateMount(
      creation_context_->local_user_id,
      keys,
      base::Bind(&ManagedUserCreationControllerNew::OnMountSuccess,
                 weak_factory_.GetWeakPtr()));
}

void ManagedUserCreationControllerNew::OnAuthenticationFailure(
    ExtendedAuthenticator::AuthState error) {
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
  stage_ = STAGE_ERROR;
  if (consumer_)
    consumer_->OnCreationError(code);
}

void ManagedUserCreationControllerNew::OnMountSuccess(
    const std::string& mount_hash) {
  DCHECK(creation_context_);
  DCHECK_EQ(KEYS_GENERATED, stage_);
  VLOG(1) << " Phase 2.2 : Created home dir with master key";

  creation_context_->mount_hash = mount_hash;

  // Plain text password, hashed and salted with individual salt.
  // It can be used for mounting homedir, and can be replaced only when signed.
  cryptohome::KeyDefinition password_key(creation_context_->salted_password,
                                         kCryptohomeManagedUserKeyLabel,
                                         kCryptohomeManagedUserKeyPrivileges);
  base::Base64Decode(creation_context_->encryption_key,
                     &password_key.encryption_key);
  base::Base64Decode(creation_context_->signature_key,
                     &password_key.signature_key);

  UserContext context(creation_context_->local_user_id,
                      creation_context_->salted_master_key,
                      std::string());
  context.using_oauth = false;
  context.need_password_hashing = false;
  context.key_label = kCryptohomeMasterKeyLabel;

  authenticator_->AddKey(
      context,
      password_key,
      true,
      base::Bind(&ManagedUserCreationControllerNew::OnAddKeySuccess,
                 weak_factory_.GetWeakPtr()));
}

void ManagedUserCreationControllerNew::OnAddKeySuccess() {
  DCHECK(creation_context_);
  DCHECK_EQ(KEYS_GENERATED, stage_);
  stage_ = CRYPTOHOME_CREATED;

  VLOG(1) << " Phase 3 : Create/update user on chrome.com/manage";

  ProfileSyncService* sync_service =
      ProfileSyncServiceFactory::GetInstance()->GetForProfile(
          creation_context_->manager_profile);
  ProfileSyncService::SyncStatusSummary status =
      sync_service->QuerySyncStatusSummary();

  if (status == ProfileSyncService::DATATYPES_NOT_INITIALIZED)
    consumer_->OnLongCreationWarning();

  creation_context_->registration_utility =
      ManagedUserRegistrationUtility::Create(
          creation_context_->manager_profile);

  ManagedUserRegistrationInfo info(creation_context_->display_name,
                                   creation_context_->avatar_index);
  info.master_key = creation_context_->master_key;
  info.password_signature_key = creation_context_->signature_key;
  info.password_encryption_key = creation_context_->encryption_key;

  info.password_data.MergeDictionary(&creation_context_->password_data);

  // Registration utility will update user data if user already exist.
  creation_context_->registration_utility->Register(
      creation_context_->sync_user_id,
      info,
      base::Bind(&ManagedUserCreationControllerNew::RegistrationCallback,
                 weak_factory_.GetWeakPtr()));
}

void ManagedUserCreationControllerNew::RegistrationCallback(
    const GoogleServiceAuthError& error,
    const std::string& token) {
  DCHECK(creation_context_);
  DCHECK_EQ(CRYPTOHOME_CREATED, stage_);

  stage_ = DASHBOARD_CREATED;

  if (error.state() == GoogleServiceAuthError::NONE) {
    creation_context_->token = token;

    PostTaskAndReplyWithResult(
        content::BrowserThread::GetBlockingPool(),
        FROM_HERE,
        base::Bind(&StoreManagedUserFiles,
                   creation_context_->token,
                   MountManager::GetHomeDir(creation_context_->mount_hash)),
        base::Bind(&ManagedUserCreationControllerNew::OnManagedUserFilesStored,
                   weak_factory_.GetWeakPtr()));
  } else {
    stage_ = STAGE_ERROR;
    LOG(ERROR) << "Managed user creation failed. Error code " << error.state();
    if (consumer_)
      consumer_->OnCreationError(CLOUD_SERVER_ERROR);
  }
}

void ManagedUserCreationControllerNew::OnManagedUserFilesStored(bool success) {
  DCHECK(creation_context_);
  DCHECK_EQ(DASHBOARD_CREATED, stage_);

  if (!success) {
    stage_ = STAGE_ERROR;
    if (consumer_)
      consumer_->OnCreationError(TOKEN_WRITE_FAILED);
    return;
  }
  // Assume that new token is valid. It will be automatically invalidated if
  // sync service fails to use it.
  UserManager::Get()->SaveUserOAuthStatus(creation_context_->local_user_id,
                                          User::OAUTH2_TOKEN_STATUS_VALID);

  stage_ = TOKEN_WRITTEN;

  timeout_timer_.Stop();
  UserManager::Get()->GetSupervisedUserManager()->CommitCreationTransaction();
  content::RecordAction(
      base::UserMetricsAction("ManagedMode_LocallyManagedUserCreated"));

  stage_ = TRANSACTION_COMMITTED;

  if (consumer_)
    consumer_->OnCreationSuccess();
}

void ManagedUserCreationControllerNew::CreationTimedOut() {
  LOG(ERROR) << "Supervised user creation timed out. stage = " << stage_;
  if (consumer_)
    consumer_->OnCreationTimeout();
}

void ManagedUserCreationControllerNew::FinishCreation() {
  chrome::AttemptUserExit();
}

void ManagedUserCreationControllerNew::CancelCreation() {
  creation_context_->registration_utility.reset();
  chrome::AttemptUserExit();
}

std::string ManagedUserCreationControllerNew::GetManagedUserId() {
  DCHECK(creation_context_);
  return creation_context_->local_user_id;
}

}  // namespace chromeos
