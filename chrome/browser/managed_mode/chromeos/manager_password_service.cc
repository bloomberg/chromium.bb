// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/managed_mode/chromeos/manager_password_service.h"

#include "base/bind.h"
#include "base/metrics/histogram.h"
#include "base/values.h"
#include "chrome/browser/chromeos/login/managed/locally_managed_user_constants.h"
#include "chrome/browser/chromeos/login/managed/supervised_user_authentication.h"
#include "chrome/browser/chromeos/login/supervised_user_manager.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/managed_mode/managed_user_constants.h"
#include "chrome/browser/managed_mode/managed_user_sync_service.h"

namespace chromeos {

ManagerPasswordService::ManagerPasswordService() : weak_ptr_factory_(this) {}

ManagerPasswordService::~ManagerPasswordService() {}

void ManagerPasswordService::Init(
    const std::string& user_id,
    ManagedUserSyncService* user_service,
    ManagedUserSharedSettingsService* shared_settings_service) {
  user_id_ = user_id;
  user_service_ = user_service;
  settings_service_ = shared_settings_service;
  settings_service_subscription_ = settings_service_->Subscribe(
      base::Bind(&ManagerPasswordService::OnSharedSettingsChange,
                 weak_ptr_factory_.GetWeakPtr()));

  authenticator_ = new ExtendedAuthenticator(this);

  UserManager* user_manager = UserManager::Get();

  SupervisedUserManager* supervised_user_manager =
      user_manager->GetSupervisedUserManager();

  const UserList& users = user_manager->GetUsers();

  for (UserList::const_iterator it = users.begin(); it != users.end(); ++it) {
    if ((*it)->GetType() != User::USER_TYPE_LOCALLY_MANAGED)
      continue;
    if (user_id != supervised_user_manager->GetManagerUserId((*it)->email()))
      continue;
    OnSharedSettingsChange(
        supervised_user_manager->GetUserSyncId((*it)->email()),
        managed_users::kChromeOSPasswordData);
  }
}

void ManagerPasswordService::OnSharedSettingsChange(
    const std::string& mu_id,
    const std::string& key) {
  if (key != managed_users::kChromeOSPasswordData)
    return;

  SupervisedUserManager* supervised_user_manager =
      UserManager::Get()->GetSupervisedUserManager();
  const User* user = supervised_user_manager->FindBySyncId(mu_id);
  // No user on device.
  if (user == NULL)
    return;

  const base::Value* value = settings_service_->GetValue(mu_id, key);

  if (value == NULL) {
    LOG(WARNING) << "Got empty value from sync.";
    return;
  }
  const base::DictionaryValue* dict;
  if (!value->GetAsDictionary(&dict)) {
    LOG(WARNING) << "Got non-dictionary value from sync.";
    return;
  }

  SupervisedUserAuthentication* auth =
      supervised_user_manager->GetAuthentication();

  if (!auth->NeedPasswordChange(user->email(), dict) &&
      !auth->HasIncompleteKey(user->email())) {
    return;
  }
  scoped_ptr<base::DictionaryValue> wrapper(dict->DeepCopy());
  user_service_->GetManagedUsersAsync(
      base::Bind(&ManagerPasswordService::GetManagedUsersCallback,
                 weak_ptr_factory_.GetWeakPtr(),
                 mu_id,
                 user->email(),
                 Passed(&wrapper)));
}

void ManagerPasswordService::GetManagedUsersCallback(
    const std::string& sync_mu_id,
    const std::string& user_id,
    scoped_ptr<base::DictionaryValue> password_data,
    const base::DictionaryValue* managed_users) {
  const base::DictionaryValue* managed_user = NULL;
  if (!managed_users->GetDictionary(sync_mu_id, &managed_user))
    return;
  std::string master_key;
  std::string encryption_key;
  std::string signature_key;
  if (!managed_user->GetString(ManagedUserSyncService::kMasterKey,
                               &master_key)) {
    LOG(WARNING) << "Can not apply password change to " << user_id
                 << ": no master key found";
    UMA_HISTOGRAM_ENUMERATION(
        "ManagedUsers.ChromeOS.PasswordChange",
        SupervisedUserAuthentication::PASSWORD_CHANGE_FAILED_NO_MASTER_KEY,
        SupervisedUserAuthentication::PASSWORD_CHANGE_RESULT_MAX_VALUE);
    return;
  }

  if (!managed_user->GetString(ManagedUserSyncService::kPasswordSignatureKey,
                               &signature_key) ||
      !managed_user->GetString(ManagedUserSyncService::kPasswordEncryptionKey,
                               &encryption_key)) {
    LOG(WARNING) << "Can not apply password change to " << user_id
                 << ": no signature / encryption keys.";
    UMA_HISTOGRAM_ENUMERATION(
        "ManagedUsers.ChromeOS.PasswordChange",
        SupervisedUserAuthentication::PASSWORD_CHANGE_FAILED_NO_SIGNATURE_KEY,
        SupervisedUserAuthentication::PASSWORD_CHANGE_RESULT_MAX_VALUE);
    return;
  }

  UserContext manager_key(user_id, master_key, std::string());
  manager_key.using_oauth = false;

  // As master key can have old label, leave label field empty - it will work
  // as wildcard.

  std::string new_key;
  int revision;

  bool has_data = password_data->GetStringWithoutPathExpansion(
      kEncryptedPassword, &new_key);
  has_data &= password_data->GetIntegerWithoutPathExpansion(kPasswordRevision,
                                                            &revision);
  if (!has_data) {
    LOG(WARNING) << "Can not apply password change to " << user_id
                 << ": incomplete password data.";
    UMA_HISTOGRAM_ENUMERATION(
        "ManagedUsers.ChromeOS.PasswordChange",
        SupervisedUserAuthentication::PASSWORD_CHANGE_FAILED_NO_PASSWORD_DATA,
        SupervisedUserAuthentication::PASSWORD_CHANGE_RESULT_MAX_VALUE);
    return;
  }

  cryptohome::KeyDefinition new_key_definition(
      new_key,
      kCryptohomeManagedUserKeyLabel,
      cryptohome::PRIV_AUTHORIZED_UPDATE || cryptohome::PRIV_MOUNT);
  new_key_definition.revision = revision;

  new_key_definition.encryption_key = encryption_key;
  new_key_definition.signature_key = signature_key;

  authenticator_->AddKey(manager_key,
                         new_key_definition,
                         true /* replace existing */,
                         base::Bind(&ManagerPasswordService::OnAddKeySuccess,
                                    weak_ptr_factory_.GetWeakPtr(),
                                    manager_key,
                                    user_id,
                                    Passed(&password_data)));
}

void ManagerPasswordService::OnAuthenticationFailure(
    ExtendedAuthenticator::AuthState state) {
  UMA_HISTOGRAM_ENUMERATION(
      "ManagedUsers.ChromeOS.PasswordChange",
      SupervisedUserAuthentication::PASSWORD_CHANGE_FAILED_MASTER_KEY_FAILURE,
      SupervisedUserAuthentication::PASSWORD_CHANGE_RESULT_MAX_VALUE);
  LOG(ERROR) << "Can not apply password change, master key failure";
}

void ManagerPasswordService::OnAddKeySuccess(
    const UserContext& master_key_context,
    const std::string& user_id,
    scoped_ptr<base::DictionaryValue> password_data) {
  VLOG(0) << "Password changed for " << user_id;
  UMA_HISTOGRAM_ENUMERATION(
      "ManagedUsers.ChromeOS.PasswordChange",
      SupervisedUserAuthentication::PASSWORD_CHANGED_IN_MANAGER_SESSION,
      SupervisedUserAuthentication::PASSWORD_CHANGE_RESULT_MAX_VALUE);

  SupervisedUserAuthentication* auth =
      UserManager::Get()->GetSupervisedUserManager()->GetAuthentication();
  int old_schema = auth->GetPasswordSchema(user_id);
  auth->StorePasswordData(user_id, *password_data.get());
  if (old_schema == SupervisedUserAuthentication::SCHEMA_PLAIN) {
    // 1) Add new manager key (using old key).
    // 2) Remove old supervised user key.
    // 3) Remove old manager key.
    authenticator_->TransformContext(
        master_key_context,
        base::Bind(&ManagerPasswordService::OnContextTransformed,
                   weak_ptr_factory_.GetWeakPtr()));
  }
}

void ManagerPasswordService::OnContextTransformed(
    const UserContext& master_key_context) {
  DCHECK(!master_key_context.need_password_hashing);
  cryptohome::KeyDefinition new_master_key(master_key_context.password,
                                           kCryptohomeMasterKeyLabel,
                                           cryptohome::PRIV_DEFAULT);
  // Use new master key for further actions.
  UserContext new_master_key_context;
  new_master_key_context.CopyFrom(master_key_context);
  new_master_key_context.key_label = kCryptohomeMasterKeyLabel;
  authenticator_->AddKey(
      master_key_context,
      new_master_key,
      true /* replace existing */,
      base::Bind(&ManagerPasswordService::OnNewManagerKeySuccess,
                 weak_ptr_factory_.GetWeakPtr(),
                 new_master_key_context));
}

void ManagerPasswordService::OnNewManagerKeySuccess(
    const UserContext& master_key_context) {
  VLOG(1) << "Added new master key for " << master_key_context.username;
  authenticator_->RemoveKey(
      master_key_context,
      kLegacyCryptohomeManagedUserKeyLabel,
      base::Bind(&ManagerPasswordService::OnOldManagedUserKeyDeleted,
                 weak_ptr_factory_.GetWeakPtr(),
                 master_key_context));
}

void ManagerPasswordService::OnOldManagedUserKeyDeleted(
    const UserContext& master_key_context) {
  VLOG(1) << "Removed old managed user key for " << master_key_context.username;
  authenticator_->RemoveKey(
      master_key_context,
      kLegacyCryptohomeMasterKeyLabel,
      base::Bind(&ManagerPasswordService::OnOldManagerKeyDeleted,
                 weak_ptr_factory_.GetWeakPtr(),
                 master_key_context));
}

void ManagerPasswordService::OnOldManagerKeyDeleted(
    const UserContext& master_key_context) {
  VLOG(1) << "Removed old master key for " << master_key_context.username;
}

void ManagerPasswordService::Shutdown() {
  settings_service_subscription_.reset();
}

}  // namespace chromeos
