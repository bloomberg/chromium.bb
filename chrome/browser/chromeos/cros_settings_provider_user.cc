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

namespace chromeos {

namespace {

Value* CreateSettingsBooleanValue(bool value, bool managed) {
  DictionaryValue* dict = new DictionaryValue;
  dict->Set("value", Value::CreateBooleanValue(value));
  dict->Set("managed", Value::CreateBooleanValue(managed));
  return dict;
}

// static
void UpdateCacheBool(const char* name, bool value) {
  PrefService* prefs = g_browser_process->local_state();
  prefs->SetBoolean(name, value);
  prefs->ScheduleSavePersistentPrefs();
}

void UpdateCacheString(const char* name, const std::string& value) {
  PrefService* prefs = g_browser_process->local_state();
  prefs->SetString(name, value);
  prefs->ScheduleSavePersistentPrefs();
}

bool GetUserWhitelist(ListValue* user_list) {
  std::vector<std::string> whitelist;
  if (!CrosLibrary::Get()->EnsureLoaded() ||
      !CrosLibrary::Get()->GetLoginLibrary()->EnumerateWhitelisted(
          &whitelist)) {
    LOG(WARNING) << "Failed to retrieve user whitelist.";
    return false;
  }

  PrefService* prefs = g_browser_process->local_state();
  ListValue* cached_whitelist = prefs->GetMutableList(kAccountsPrefUsers);
  cached_whitelist->Clear();

  const UserManager::User& self = UserManager::Get()->logged_in_user();
  bool is_owner = UserManager::Get()->current_user_is_owner();

  for (size_t i = 0; i < whitelist.size(); ++i) {
    const std::string& email = whitelist[i];

    if (user_list) {
      DictionaryValue* user = new DictionaryValue;
      user->SetString("email", email);
      user->SetString("name", "");
      user->SetBoolean("owner", is_owner && email == self.email());
      user_list->Append(user);
    }

    cached_whitelist->Append(Value::CreateStringValue(email));
  }

  prefs->ScheduleSavePersistentPrefs();

  return true;
}

}  // namespace

UserCrosSettingsProvider::UserCrosSettingsProvider() {
  StartFetchingBoolSetting(kAccountsPrefAllowGuest);
  StartFetchingBoolSetting(kAccountsPrefAllowNewUser);
  StartFetchingBoolSetting(kAccountsPrefShowUserNamesOnSignIn);
  StartFetchingStringSetting(kDeviceOwner);
}

UserCrosSettingsProvider::~UserCrosSettingsProvider() {
  // Cancels all pending callbacks from us.
  SignedSettingsHelper::Get()->CancelCallback(this);
}

// static
void UserCrosSettingsProvider::RegisterPrefs(PrefService* local_state) {
  // Cached signed settings values
  local_state->RegisterBooleanPref(kAccountsPrefAllowGuest, true);
  local_state->RegisterBooleanPref(kAccountsPrefAllowNewUser, true);
  local_state->RegisterBooleanPref(kAccountsPrefShowUserNamesOnSignIn, true);
  local_state->RegisterListPref(kAccountsPrefUsers);
  local_state->RegisterStringPref(kDeviceOwner, "");
}

// static
bool UserCrosSettingsProvider::cached_allow_guest() {
  return g_browser_process->local_state()->GetBoolean(kAccountsPrefAllowGuest);
}

// static
bool UserCrosSettingsProvider::cached_allow_new_user() {
  return g_browser_process->local_state()->GetBoolean(
    kAccountsPrefAllowNewUser);
}

// static
bool UserCrosSettingsProvider::cached_show_users_on_signin() {
  return g_browser_process->local_state()->GetBoolean(
      kAccountsPrefShowUserNamesOnSignIn);
}

// static
const ListValue* UserCrosSettingsProvider::cached_whitelist() {
  PrefService* prefs = g_browser_process->local_state();
  const ListValue* cached_users = prefs->GetList(kAccountsPrefUsers);

  if (!cached_users) {
    // Update whitelist cache.
    GetUserWhitelist(NULL);

    cached_users = prefs->GetList(kAccountsPrefUsers);
  }

  return cached_users;
}

// static
std::string UserCrosSettingsProvider::cached_owner() {
  return g_browser_process->local_state()->GetString(kDeviceOwner);
}

// static
bool UserCrosSettingsProvider::IsEmailInCachedWhitelist(
    const std::string& email) {
  const ListValue* whitelist = cached_whitelist();
  if (whitelist) {
    StringValue email_value(email);
    for (ListValue::const_iterator i(whitelist->begin());
        i != whitelist->end(); ++i) {
      if ((*i)->Equals(&email_value))
        return true;
    }
  }
  return false;
}

void UserCrosSettingsProvider::DoSet(const std::string& path,
                                     Value* in_value) {
  if (!UserManager::Get()->current_user_is_owner()) {
    LOG(WARNING) << "Changing settings from non-owner, setting=" << path;

    // Revert UI change.
    CrosSettings::Get()->FireObservers(path.c_str());
    return;
  }

  if (path == kAccountsPrefAllowGuest ||
      path == kAccountsPrefAllowNewUser ||
      path == kAccountsPrefShowUserNamesOnSignIn) {
    bool bool_value = false;
    if (in_value->GetAsBoolean(&bool_value)) {
      std::string value = bool_value ? "true" : "false";
      SignedSettingsHelper::Get()->StartStorePropertyOp(path, value, this);
      UpdateCacheBool(path.c_str(), bool_value);

      VLOG(1) << "Set cros setting " << path << "=" << value;
    }
  } else if (path == kDeviceOwner) {
    VLOG(1) << "Setting owner is not supported. Please use 'UpdateCachedOwner' "
               "instead.";
  } else if (path == kAccountsPrefUsers) {
    VLOG(1) << "Setting user whitelist is not implemented.  Please use "
               "whitelist/unwhitelist instead.";
  } else {
    LOG(WARNING) << "Try to set unhandled cros setting " << path;
  }
}

bool UserCrosSettingsProvider::Get(const std::string& path,
                                   Value** out_value) const {
  if (path == kAccountsPrefAllowGuest ||
      path == kAccountsPrefAllowNewUser ||
      path == kAccountsPrefShowUserNamesOnSignIn) {
    *out_value = CreateSettingsBooleanValue(
        g_browser_process->local_state()->GetBoolean(path.c_str()),
        !UserManager::Get()->current_user_is_owner());
    return true;
  } else if (path == kAccountsPrefUsers) {
    ListValue* user_list = new ListValue;
    GetUserWhitelist(user_list);
    *out_value = user_list;
    return true;
  }

  return false;
}

bool UserCrosSettingsProvider::HandlesSetting(const std::string& path) {
  return ::StartsWithASCII(path, "cros.accounts.", true);
}

void UserCrosSettingsProvider::OnWhitelistCompleted(bool success,
    const std::string& email) {
  VLOG(1) << "Add " << email << " to whitelist, success=" << success;

  // Reload the whitelist on settings op failure.
  if (!success)
    CrosSettings::Get()->FireObservers(kAccountsPrefUsers);
}

void UserCrosSettingsProvider::OnUnwhitelistCompleted(bool success,
    const std::string& email) {
  VLOG(1) << "Remove " << email << " from whitelist, success=" << success;

  // Reload the whitelist on settings op failure.
  if (!success)
    CrosSettings::Get()->FireObservers(kAccountsPrefUsers);
}

void UserCrosSettingsProvider::OnStorePropertyCompleted(
    bool success, const std::string& name, const std::string& value) {
  VLOG(1) << "Store cros setting " << name << "=" << value << ", success="
          << success;

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

  VLOG(1) << "Retrieved cros setting " << name << "=" << value;

  if (bool_settings_.count(name)) {
    UpdateCacheBool(name.c_str(), value == "true" ? true : false);
  }

  if (string_settings_.count(name)) {
    UpdateCacheString(name.c_str(), value);
  }

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

// static
void UserCrosSettingsProvider::UpdateCachedOwner(const std::string& email) {
  UpdateCacheString(kDeviceOwner, email);
}

void UserCrosSettingsProvider::StartFetchingBoolSetting(
    const std::string& name) {
  bool_settings_.insert(name);
  StartFetchingSetting(name);
}

void UserCrosSettingsProvider::StartFetchingStringSetting(
    const std::string& name) {
  string_settings_.insert(name);
  StartFetchingSetting(name);
}

void UserCrosSettingsProvider::StartFetchingSetting(
    const std::string& name) {
  if (CrosLibrary::Get()->EnsureLoaded())
    SignedSettingsHelper::Get()->StartRetrieveProperty(name, this);
}

}  // namespace chromeos
