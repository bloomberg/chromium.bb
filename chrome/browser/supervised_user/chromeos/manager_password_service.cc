// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/chromeos/manager_password_service.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/values.h"
#include "chrome/browser/chromeos/login/supervised/supervised_user_authentication.h"
#include "chrome/browser/chromeos/login/supervised/supervised_user_constants.h"
#include "chrome/browser/chromeos/login/users/chrome_user_manager.h"
#include "chrome/browser/chromeos/login/users/supervised_user_manager.h"
#include "chrome/browser/supervised_user/supervised_user_constants.h"
#include "chrome/browser/supervised_user/supervised_user_sync_service.h"
#include "chromeos/login/auth/key.h"
#include "chromeos/login/auth/user_context.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_type.h"

namespace chromeos {

ManagerPasswordService::ManagerPasswordService() : weak_ptr_factory_(this) {}

ManagerPasswordService::~ManagerPasswordService() {}

void ManagerPasswordService::Init(
    const std::string& user_id,
    SupervisedUserSyncService* user_service,
    SupervisedUserSharedSettingsService* shared_settings_service) {
  user_id_ = user_id;
  user_service_ = user_service;
  settings_service_ = shared_settings_service;
  settings_service_subscription_ = settings_service_->Subscribe(
      base::Bind(&ManagerPasswordService::OnSharedSettingsChange,
                 weak_ptr_factory_.GetWeakPtr()));

  authenticator_ = new ExtendedAuthenticator(this);

  SupervisedUserManager* supervised_user_manager =
      ChromeUserManager::Get()->GetSupervisedUserManager();

  const user_manager::UserList& users =
      user_manager::UserManager::Get()->GetUsers();

  for (user_manager::UserList::const_iterator it = users.begin();
       it != users.end();
       ++it) {
    if ((*it)->GetType() != user_manager::USER_TYPE_SUPERVISED)
      continue;
    if (user_id != supervised_user_manager->GetManagerUserId((*it)->email()))
      continue;
    OnSharedSettingsChange(
        supervised_user_manager->GetUserSyncId((*it)->email()),
        supervised_users::kChromeOSPasswordData);
  }
}

void ManagerPasswordService::OnSharedSettingsChange(
    const std::string& su_id,
    const std::string& key) {
  if (key != supervised_users::kChromeOSPasswordData)
    return;

  SupervisedUserManager* supervised_user_manager =
      ChromeUserManager::Get()->GetSupervisedUserManager();
  const user_manager::User* user = supervised_user_manager->FindBySyncId(su_id);
  // No user on device.
  if (user == NULL)
    return;

  const base::Value* value = settings_service_->GetValue(su_id, key);

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
  user_service_->GetSupervisedUsersAsync(
      base::Bind(&ManagerPasswordService::GetSupervisedUsersCallback,
                 weak_ptr_factory_.GetWeakPtr(),
                 su_id,
                 user->email(),
                 Passed(&wrapper)));
}

void ManagerPasswordService::GetSupervisedUsersCallback(
    const std::string& sync_su_id,
    const std::string& user_id,
    scoped_ptr<base::DictionaryValue> password_data,
    const base::DictionaryValue* supervised_users) {
  const base::DictionaryValue* supervised_user = NULL;
  if (!supervised_users->GetDictionary(sync_su_id, &supervised_user))
    return;
  std::string master_key;
  std::string encryption_key;
  std::string signature_key;
  if (!supervised_user->GetString(SupervisedUserSyncService::kMasterKey,
                                  &master_key)) {
    LOG(WARNING) << "Can not apply password change to " << user_id
                 << ": no master key found";
    UMA_HISTOGRAM_ENUMERATION(
        "ManagedUsers.ChromeOS.PasswordChange",
        SupervisedUserAuthentication::PASSWORD_CHANGE_FAILED_NO_MASTER_KEY,
        SupervisedUserAuthentication::PASSWORD_CHANGE_RESULT_MAX_VALUE);
    return;
  }

  if (!supervised_user->GetString(
          SupervisedUserSyncService::kPasswordSignatureKey, &signature_key) ||
      !supervised_user->GetString(
          SupervisedUserSyncService::kPasswordEncryptionKey,
          &encryption_key)) {
    LOG(WARNING) << "Can not apply password change to " << user_id
                 << ": no signature / encryption keys.";
    UMA_HISTOGRAM_ENUMERATION(
        "ManagedUsers.ChromeOS.PasswordChange",
        SupervisedUserAuthentication::PASSWORD_CHANGE_FAILED_NO_SIGNATURE_KEY,
        SupervisedUserAuthentication::PASSWORD_CHANGE_RESULT_MAX_VALUE);
    return;
  }

  UserContext manager_key(user_id);
  manager_key.SetKey(Key(master_key));
  manager_key.SetIsUsingOAuth(false);

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
      kCryptohomeSupervisedUserKeyLabel,
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
      ChromeUserManager::Get()->GetSupervisedUserManager()->GetAuthentication();
  int old_schema = auth->GetPasswordSchema(user_id);
  auth->StorePasswordData(user_id, *password_data.get());

  if (auth->HasIncompleteKey(user_id))
    auth->MarkKeyIncomplete(user_id, false /* key is complete now */);

  // Check if we have legacy labels for keys.
  // TODO(antrim): Migrate it to GetLabels call once wad@ implement it.
  if (old_schema == SupervisedUserAuthentication::SCHEMA_PLAIN) {
    // 1) Add new manager key (using old key).
    // 2) Remove old supervised user key.
    // 3) Remove old manager key.
    authenticator_->TransformKeyIfNeeded(
        master_key_context,
        base::Bind(&ManagerPasswordService::OnKeyTransformedIfNeeded,
                   weak_ptr_factory_.GetWeakPtr()));
  }
}

void ManagerPasswordService::OnKeyTransformedIfNeeded(
    const UserContext& master_key_context) {
  const Key* const key = master_key_context.GetKey();
  DCHECK_NE(Key::KEY_TYPE_PASSWORD_PLAIN, key->GetKeyType());
  cryptohome::KeyDefinition new_master_key(key->GetSecret(),
                                           kCryptohomeMasterKeyLabel,
                                           cryptohome::PRIV_DEFAULT);
  // Use new master key for further actions.
  UserContext new_master_key_context = master_key_context;
  new_master_key_context.GetKey()->SetLabel(kCryptohomeMasterKeyLabel);
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
  VLOG(1) << "Added new master key for " << master_key_context.GetUserID();
  authenticator_->RemoveKey(
      master_key_context,
      kLegacyCryptohomeSupervisedUserKeyLabel,
      base::Bind(&ManagerPasswordService::OnOldSupervisedUserKeyDeleted,
                 weak_ptr_factory_.GetWeakPtr(),
                 master_key_context));
}

void ManagerPasswordService::OnOldSupervisedUserKeyDeleted(
    const UserContext& master_key_context) {
  VLOG(1) << "Removed old supervised user key for "
          << master_key_context.GetUserID();
  authenticator_->RemoveKey(
      master_key_context,
      kLegacyCryptohomeMasterKeyLabel,
      base::Bind(&ManagerPasswordService::OnOldManagerKeyDeleted,
                 weak_ptr_factory_.GetWeakPtr(),
                 master_key_context));
}

void ManagerPasswordService::OnOldManagerKeyDeleted(
    const UserContext& master_key_context) {
  VLOG(1) << "Removed old master key for " << master_key_context.GetUserID();
}

void ManagerPasswordService::Shutdown() {
  settings_service_subscription_.reset();
}

}  // namespace chromeos
