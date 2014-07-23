// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/chromeos/supervised_user_password_service.h"

#include "base/bind.h"
#include "base/values.h"
#include "chrome/browser/chromeos/login/supervised/supervised_user_authentication.h"
#include "chrome/browser/chromeos/login/users/supervised_user_manager.h"
#include "chrome/browser/chromeos/login/users/user_manager.h"
#include "chrome/browser/supervised_user/supervised_user_constants.h"
#include "chrome/browser/supervised_user/supervised_user_sync_service.h"

namespace chromeos {

SupervisedUserPasswordService::SupervisedUserPasswordService()
    : weak_ptr_factory_(this) {}

SupervisedUserPasswordService::~SupervisedUserPasswordService() {}

void SupervisedUserPasswordService::Init(
    const std::string& user_id,
    SupervisedUserSharedSettingsService* shared_settings_service) {
  user_id_ = user_id;
  settings_service_ = shared_settings_service;
  settings_service_subscription_ = settings_service_->Subscribe(
      base::Bind(&SupervisedUserPasswordService::OnSharedSettingsChange,
                 weak_ptr_factory_.GetWeakPtr()));

  // Force value check in case we have missed some notification.

  chromeos::SupervisedUserManager* supervised_user_manager =
      chromeos::UserManager::Get()->GetSupervisedUserManager();

  OnSharedSettingsChange(supervised_user_manager->GetUserSyncId(user_id),
                         supervised_users::kChromeOSPasswordData);
}

void SupervisedUserPasswordService::OnSharedSettingsChange(
    const std::string& su_id,
    const std::string& key) {
  if (key != supervised_users::kChromeOSPasswordData)
    return;
  chromeos::SupervisedUserManager* supervised_user_manager =
      chromeos::UserManager::Get()->GetSupervisedUserManager();
  const user_manager::User* user = supervised_user_manager->FindBySyncId(su_id);
  if (user == NULL) {
    LOG(WARNING) << "Got notification for user not on device.";
    return;
  }
  DCHECK(user_id_ == user->email());
  if (user_id_ != user->email())
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
  chromeos::SupervisedUserAuthentication* auth =
      supervised_user_manager->GetAuthentication();
  if (!auth->NeedPasswordChange(user_id_, dict))
    return;
  auth->ScheduleSupervisedPasswordChange(user_id_, dict);
}

void SupervisedUserPasswordService::Shutdown() {
    settings_service_subscription_.reset();
}

}  // namespace chromeos
