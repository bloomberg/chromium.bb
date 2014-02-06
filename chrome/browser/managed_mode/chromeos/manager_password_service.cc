// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/managed_mode/chromeos/manager_password_service.h"

#include "base/bind.h"
#include "base/values.h"
#include "chrome/browser/chromeos/login/managed/supervised_user_authentication.h"
#include "chrome/browser/chromeos/login/supervised_user_manager.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/managed_mode/managed_user_constants.h"
#include "chrome/browser/managed_mode/managed_user_sync_service.h"

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

  chromeos::UserManager* user_manager = chromeos::UserManager::Get();

  chromeos::SupervisedUserManager* supervised_user_manager =
      user_manager->GetSupervisedUserManager();

  const chromeos::UserList& users = user_manager->GetUsers();

  for (chromeos::UserList::const_iterator it = users.begin();
      it != users.end(); ++it) {
    if ((*it)->GetType() !=  chromeos::User::USER_TYPE_LOCALLY_MANAGED)
      continue;
    if (user_id != supervised_user_manager->GetManagerUserId((*it)->email()))
      continue;
    OnSharedSettingsChange(
        supervised_user_manager->GetUserSyncId((*it)->email()),
        managed_users::kUserPasswordRecord);
  }
}

void ManagerPasswordService::OnSharedSettingsChange(
    const std::string& mu_id,
    const std::string& key) {
  if (key != managed_users::kUserPasswordRecord)
    return;

  chromeos::SupervisedUserManager* supervised_user_manager =
      chromeos::UserManager::Get()->GetSupervisedUserManager();
  const chromeos::User* user = supervised_user_manager->FindBySyncId(mu_id);
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

  chromeos::SupervisedUserAuthentication* auth =
      supervised_user_manager->GetAuthentication();

  if (!auth->NeedPasswordChange(user->email(), dict))
    return;
  user_service_->GetManagedUsersAsync(
      base::Bind(&ManagerPasswordService::GetManagedUsersCallback,
                 weak_ptr_factory_.GetWeakPtr(),
                 mu_id,
                 user->email(),
                 dict));
}

void ManagerPasswordService::GetManagedUsersCallback(
    const std::string& sync_mu_id,
    const std::string& user_id,
    const base::DictionaryValue* password_data,
    const base::DictionaryValue* managed_users) {
  const base::DictionaryValue* managed_user = NULL;
  if (!managed_users->GetDictionary(sync_mu_id, &managed_user))
    return;
  std::string master_key;
  if (!managed_user->GetString(ManagedUserSyncService::kMasterKey, &master_key))
    return;
  chromeos::SupervisedUserAuthentication* auth = chromeos::UserManager::Get()->
      GetSupervisedUserManager()->GetAuthentication();
  auth->ChangeSupervisedUserPassword(
      user_id_,
      master_key,
      user_id,
      password_data);
}

void ManagerPasswordService::Shutdown() {
  settings_service_subscription_.reset();
}
