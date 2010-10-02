// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros_settings_provider_user.h"

#include "base/logging.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/login_library.h"
#include "chrome/browser/chromeos/cros_settings.h"
#include "chrome/browser/chromeos/cros_settings_names.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/prefs/pref_service.h"

namespace {

Value* CreateSettingsBooleanValue(bool value, bool managed) {
  DictionaryValue* dict = new DictionaryValue;
  dict->Set("value", Value::CreateBooleanValue(value));
  dict->Set("managed", Value::CreateBooleanValue(managed));
  return dict;
}

void UpdateCache(const char* name, bool value) {
  PrefService* prefs = g_browser_process->local_state();
  prefs->SetBoolean(name, value);
  prefs->ScheduleSavePersistentPrefs();
}

}  // namespace

namespace chromeos {

UserCrosSettingsProvider::UserCrosSettingsProvider() {
  current_user_is_owner_ = UserManager::Get()->current_user_is_owner();

  StartFetchingBoolSetting(kAccountsPrefAllowBWSI);
  StartFetchingBoolSetting(kAccountsPrefAllowNewUser);
  StartFetchingBoolSetting(kAccountsPrefShowUserNamesOnSignIn);
}

UserCrosSettingsProvider::~UserCrosSettingsProvider() {
  // Cancels all pending callbacks from us.
  SignedSettingsHelper::Get()->CancelCallback(this);
}

void UserCrosSettingsProvider::RegisterPrefs(PrefService* local_state) {
  // Cached signed settings values
  local_state->RegisterBooleanPref(kAccountsPrefAllowBWSI, true);
  local_state->RegisterBooleanPref(kAccountsPrefAllowNewUser, true);
  local_state->RegisterBooleanPref(kAccountsPrefShowUserNamesOnSignIn, true);
  local_state->RegisterListPref(kAccountsPrefUsers);
}

bool UserCrosSettingsProvider::cached_allow_bwsi() {
  return g_browser_process->local_state()->GetBoolean(kAccountsPrefAllowBWSI);
}

bool UserCrosSettingsProvider::cached_allow_new_user() {
  return g_browser_process->local_state()->GetBoolean(
    kAccountsPrefAllowNewUser);
}

bool UserCrosSettingsProvider::cached_show_users_on_signin() {
  return g_browser_process->local_state()->GetBoolean(
      kAccountsPrefShowUserNamesOnSignIn);
}

const ListValue* UserCrosSettingsProvider::cached_whitelist() {
  return g_browser_process->local_state()->GetList(kAccountsPrefUsers);
}

void UserCrosSettingsProvider::Set(const std::string& path, Value* in_value) {
  if (!current_user_is_owner_) {
    LOG(WARNING) << "Changing settings from non-owner, setting=" << path;

    // Revert UI change.
    CrosSettings::Get()->FireObservers(path.c_str());
    return;
  }

  if (path == kAccountsPrefAllowBWSI ||
      path == kAccountsPrefAllowNewUser ||
      path == kAccountsPrefShowUserNamesOnSignIn) {
    bool bool_value = false;
    if (in_value->GetAsBoolean(&bool_value)) {
      std::string value = bool_value ? "true" : "false";
      SignedSettingsHelper::Get()->StartStorePropertyOp(path, value, this);
      UpdateCache(path.c_str(), bool_value);

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
  if (path == kAccountsPrefAllowBWSI ||
      path == kAccountsPrefAllowNewUser ||
      path == kAccountsPrefShowUserNamesOnSignIn) {
    *out_value = CreateSettingsBooleanValue(
        g_browser_process->local_state()->GetBoolean(path.c_str()),
        !current_user_is_owner_);
    return true;
  } else if (path == kAccountsPrefUsers) {
    *out_value = GetUserWhitelist();
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
    CrosSettings::Get()->FireObservers(kAccountsPrefUsers);
}

void UserCrosSettingsProvider::OnUnwhitelistCompleted(bool success,
    const std::string& email) {
  LOG(INFO) << "Remove " << email << " from whitelist, success=" << success;

  // Reload the whitelist on settings op failure.
  if (!success)
    CrosSettings::Get()->FireObservers(kAccountsPrefUsers);
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

  UpdateCache(name.c_str(), value == "true" ? true : false);
  CrosSettings::Get()->FireObservers(name.c_str());
}

void UserCrosSettingsProvider::WhitelistUser(const std::string& email) {
  SignedSettingsHelper::Get()->StartWhitelistOp(email, true, this);

  PrefService* prefs = g_browser_process->local_state();
  ListValue* cached_whitelist = prefs->GetMutableList(kAccountsPrefUsers);
  cached_whitelist->Append(Value::CreateStringValue(email));
  prefs->ScheduleSavePersistentPrefs();
}

void UserCrosSettingsProvider::UnwhitelistUser(const std::string& email) {
  SignedSettingsHelper::Get()->StartWhitelistOp(email, false, this);

  PrefService* prefs = g_browser_process->local_state();
  ListValue* cached_whitelist = prefs->GetMutableList(kAccountsPrefUsers);
  StringValue email_value(email);
  if (cached_whitelist->Remove(email_value) != -1)
    prefs->ScheduleSavePersistentPrefs();
}

void UserCrosSettingsProvider::StartFetchingBoolSetting(
    const std::string& name) {
  if (CrosLibrary::Get()->EnsureLoaded())
    SignedSettingsHelper::Get()->StartRetrieveProperty(name, this);
}

ListValue* UserCrosSettingsProvider::GetUserWhitelist() const {
  ListValue* user_list = new ListValue;

  std::vector<std::string> whitelist;
  if (!CrosLibrary::Get()->EnsureLoaded() ||
      !CrosLibrary::Get()->GetLoginLibrary()->EnumerateWhitelisted(
          &whitelist)) {
    LOG(WARNING) << "Failed to retrieve user whitelist.";
  } else {
    PrefService* prefs = g_browser_process->local_state();
    ListValue* cached_whitelist = prefs->GetMutableList(kAccountsPrefUsers);
    cached_whitelist->Clear();

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
      cached_whitelist->Append(Value::CreateStringValue(email));
    }

    prefs->ScheduleSavePersistentPrefs();
  }

  return user_list;
}

}  // namespace chromeos
