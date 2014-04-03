// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/managed_mode/chromeos/managed_user_password_service.h"

#include "base/bind.h"
#include "base/values.h"
#include "chrome/browser/chromeos/login/managed/supervised_user_authentication.h"
#include "chrome/browser/chromeos/login/supervised_user_manager.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/managed_mode/managed_user_constants.h"
#include "chrome/browser/managed_mode/managed_user_sync_service.h"

namespace chromeos {

ManagedUserPasswordService::ManagedUserPasswordService()
    : weak_ptr_factory_(this) {}

ManagedUserPasswordService::~ManagedUserPasswordService() {}

void ManagedUserPasswordService::Init(
    const std::string& user_id,
    ManagedUserSharedSettingsService* shared_settings_service) {
  user_id_ = user_id;
  settings_service_ = shared_settings_service;
  settings_service_subscription_ = settings_service_->Subscribe(
      base::Bind(&ManagedUserPasswordService::OnSharedSettingsChange,
                 weak_ptr_factory_.GetWeakPtr()));

  // Force value check in case we have missed some notification.

  chromeos::SupervisedUserManager* supervised_user_manager =
      chromeos::UserManager::Get()->GetSupervisedUserManager();

  OnSharedSettingsChange(supervised_user_manager->GetUserSyncId(user_id),
                         managed_users::kChromeOSPasswordData);
}

void ManagedUserPasswordService::OnSharedSettingsChange(
    const std::string& mu_id,
    const std::string& key) {
  if (key != managed_users::kChromeOSPasswordData)
    return;
  chromeos::SupervisedUserManager* supervised_user_manager =
      chromeos::UserManager::Get()->GetSupervisedUserManager();
  const chromeos::User* user = supervised_user_manager->FindBySyncId(mu_id);
  if (user == NULL) {
    LOG(WARNING) << "Got notification for user not on device.";
    return;
  }
  DCHECK(user_id_ == user->email());
  if (user_id_ != user->email())
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
  if (!auth->NeedPasswordChange(user_id_, dict))
    return;
  auth->ScheduleSupervisedPasswordChange(user_id_, dict);
}

void ManagedUserPasswordService::Shutdown() {
    settings_service_subscription_.reset();
}

}  // namespace chromeos
