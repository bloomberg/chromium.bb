// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros_settings_provider_user.h"

#include "base/logging.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/login_library.h"
#include "chrome/browser/chromeos/cros_settings.h"
#include "chrome/browser/chromeos/cros_settings_names.h"
#include "chrome/browser/chromeos/login/user_manager.h"

namespace {

Value* CreateSettingsBooleanValue(bool value, bool managed) {
  DictionaryValue* dict = new DictionaryValue;
  dict->Set("value", Value::CreateBooleanValue(value));
  dict->Set("managed", Value::CreateBooleanValue(managed));
  return dict;
}

}  // namespace

namespace chromeos {

UserCrosSettingsProvider::UserCrosSettingsProvider()
    : cache_(new DictionaryValue) {
  current_user_is_owner_ = UserManager::Get()->current_user_is_owner();

  StartFetchingBoolSetting(kAccountsPrefAllowBWSI, true);
  StartFetchingBoolSetting(kAccountsPrefAllowGuest, true);
  StartFetchingBoolSetting(kAccountsPrefShowUserNamesOnSignIn, true);

  LoadUserWhitelist();
}

UserCrosSettingsProvider::~UserCrosSettingsProvider() {
  // Cancels all pending callbacks from us.
  SignedSettingsHelper::Get()->CancelCallback(this);
}

void UserCrosSettingsProvider::Set(const std::string& path, Value* in_value) {
  if (path == kAccountsPrefAllowBWSI ||
      path == kAccountsPrefAllowGuest ||
      path == kAccountsPrefShowUserNamesOnSignIn) {
    bool bool_value = false;
    if (in_value->GetAsBoolean(&bool_value)) {
      cache_->Set(path, in_value);

      std::string value = bool_value ? "true" : "false";
      SignedSettingsHelper::Get()->StartStorePropertyOp(path, value, this);

      LOG(INFO) << "Set cros setting " << path << "=" << value;
    }
  } else if (path == kAccountsPrefUsers) {
    LOG(INFO) << "Setting user whitelist is not implemented."
              << "Please use whitelist/unwhitelist instead.";
  } else {
    LOG(WARNING) << "Try to set unhandled cros setting " << path;
  }
}

bool UserCrosSettingsProvider::Get(const std::string& path,
                                   Value** out_value) const {
  Value* value = NULL;
  if (cache_->Get(path, &value) && value) {
    *out_value = value->DeepCopy();
    return true;
  }

  return false;
}

bool UserCrosSettingsProvider::HandlesSetting(const std::string& path) {
  return ::StartsWithASCII(path, "cros.accounts.", true);
}

void UserCrosSettingsProvider::OnWhitelistCompleted(bool success,
    const std::string& email) {
  LOG(INFO) << "Add " << email << " to whitelist, success=" << success;

  // Reload the whitelist on settings op failure.
  if (!success)
    LoadUserWhitelist();
}

void UserCrosSettingsProvider::OnUnwhitelistCompleted(bool success,
    const std::string& email) {
  LOG(INFO) << "Remove " << email << " from whitelist, success=" << success;

  // Reload the whitelist on settings op failure.
  if (!success)
    LoadUserWhitelist();
}

void UserCrosSettingsProvider::OnStorePropertyCompleted(
    bool success, const std::string& name, const std::string& value) {
  LOG(INFO) << "Store cros setting " << name << "=" << value
            << ", success=" << success;

  // Reload the setting if store op fails.
  if (!success)
    SignedSettingsHelper::Get()->StartRetrieveProperty(name, this);
}

void UserCrosSettingsProvider::OnRetrievePropertyCompleted(
    bool success, const std::string& name, const std::string& value) {
  if (!success) {
    LOG(WARNING) << "Failed to retrieve cros setting, name=" << name;
    return;
  }

  LOG(INFO) << "Retrieved cros setting " << name << "=" << value;

  bool bool_value = value == "true" ? true : false;
  cache_->Set(name,
      CreateSettingsBooleanValue(bool_value, !current_user_is_owner_));
  CrosSettings::Get()->FireObservers(name.c_str());
}

void UserCrosSettingsProvider::WhitelistUser(const std::string& email) {
  SignedSettingsHelper::Get()->StartWhitelistOp(email, true, this);
}

void UserCrosSettingsProvider::UnwhitelistUser(const std::string& email) {
  SignedSettingsHelper::Get()->StartWhitelistOp(email, false, this);
}

void UserCrosSettingsProvider::StartFetchingBoolSetting(
    const std::string& name,
    bool default_value) {
  if (!cache_->HasKey(name)) {
    cache_->Set(name,
        CreateSettingsBooleanValue(default_value, !current_user_is_owner_));
  }

  if (CrosLibrary::Get()->EnsureLoaded())
    SignedSettingsHelper::Get()->StartRetrieveProperty(name, this);
}

void UserCrosSettingsProvider::LoadUserWhitelist() {
  ListValue* user_list = new ListValue;

  std::vector<std::string> whitelist;
  if (!CrosLibrary::Get()->EnsureLoaded() ||
      !CrosLibrary::Get()->GetLoginLibrary()->EnumerateWhitelisted(
          &whitelist)) {
    LOG(WARNING) << "Failed to retrieve user whitelist.";
  } else {
    const UserManager::User& current_user =
        UserManager::Get()->logged_in_user();
    for (size_t i = 0; i < whitelist.size(); ++i) {
      const std::string& email = whitelist[i];

      DictionaryValue* user = new DictionaryValue;
      user->SetString("email", email);
      user->SetString("name", "");
      user->SetBoolean("owner",
          current_user_is_owner_ && email == current_user.email());

      user_list->Append(user);
    }
  }

  cache_->Set(kAccountsPrefUsers, user_list);
  CrosSettings::Get()->FireObservers(kAccountsPrefUsers);
}

}  // namespace chromeos
