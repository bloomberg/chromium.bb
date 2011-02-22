// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/user_cros_settings_provider.h"

#include <map>
#include <set>

#include "base/hash_tables.h"
#include "base/logging.h"
#include "base/singleton.h"
#include "base/string_util.h"
#include "base/task.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/login_library.h"
#include "chrome/browser/chromeos/cros_settings.h"
#include "chrome/browser/chromeos/cros_settings_names.h"
#include "chrome/browser/chromeos/login/ownership_service.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/prefs/pref_service.h"

namespace chromeos {

namespace {

const char kTrueIncantation[] = "true";
const char kFalseIncantation[] = "false";
const char kTrustedSuffix[] = "/trusted";

// For all our boolean settings following is applicable:
// true is default permissive value and false is safe prohibitic value.
const char* kBooleanSettings[] = {
  kAccountsPrefAllowNewUser,
  kAccountsPrefAllowGuest,
  kAccountsPrefShowUserNamesOnSignIn
};

const char* kStringSettings[] = {
  kDeviceOwner
};

const char* kListSettings[] = {
  kAccountsPrefUsers
};

bool IsControlledBooleanSetting(const std::string& pref_path) {
  return std::find(kBooleanSettings,
                   kBooleanSettings + arraysize(kBooleanSettings),
                   pref_path) !=
      kBooleanSettings + arraysize(kBooleanSettings);
}

bool IsControlledStringSetting(const std::string& pref_path) {
  return std::find(kStringSettings,
                   kStringSettings + arraysize(kStringSettings),
                   pref_path) !=
      kStringSettings + arraysize(kStringSettings);
}

bool IsControlledListSetting(const std::string& pref_path) {
  return std::find(kListSettings,
                   kListSettings + arraysize(kListSettings),
                   pref_path) !=
      kListSettings + arraysize(kListSettings);
}

void RegisterSetting(PrefService* local_state, const std::string& pref_path) {
  local_state->RegisterBooleanPref((pref_path + kTrustedSuffix).c_str(),
                                   false);
  if (IsControlledBooleanSetting(pref_path)) {
    local_state->RegisterBooleanPref(pref_path.c_str(), true);
  } else if (IsControlledStringSetting(pref_path)) {
    local_state->RegisterStringPref(pref_path.c_str(), "");
  } else {
    DCHECK(IsControlledListSetting(pref_path));
    local_state->RegisterListPref(pref_path.c_str());
  }
}

Value* CreateSettingsBooleanValue(bool value, bool managed) {
  DictionaryValue* dict = new DictionaryValue;
  dict->Set("value", Value::CreateBooleanValue(value));
  dict->Set("managed", Value::CreateBooleanValue(managed));
  return dict;
}

enum UseValue {
  USE_VALUE_SUPPLIED,
  USE_VALUE_DEFAULT
};

void UpdateCacheBool(const std::string& name,
                     bool value,
                     UseValue use_value) {
  PrefService* prefs = g_browser_process->local_state();
  if (use_value == USE_VALUE_DEFAULT)
    prefs->ClearPref(name.c_str());
  else
    prefs->SetBoolean(name.c_str(), value);
  prefs->ScheduleSavePersistentPrefs();
}

void UpdateCacheString(const std::string& name,
                       const std::string& value,
                       UseValue use_value) {
  PrefService* prefs = g_browser_process->local_state();
  if (use_value == USE_VALUE_DEFAULT)
    prefs->ClearPref(name.c_str());
  else
    prefs->SetString(name.c_str(), value);
  prefs->ScheduleSavePersistentPrefs();
}

bool GetUserWhitelist(ListValue* user_list) {
  PrefService* prefs = g_browser_process->local_state();
  DCHECK(!prefs->IsManagedPreference(kAccountsPrefUsers));

  std::vector<std::string> whitelist;
  if (!CrosLibrary::Get()->EnsureLoaded() ||
      !CrosLibrary::Get()->GetLoginLibrary()->EnumerateWhitelisted(
          &whitelist)) {
    LOG(WARNING) << "Failed to retrieve user whitelist.";
    return false;
  }

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

class UserCrosSettingsTrust : public SignedSettingsHelper::Callback {
 public:
  static UserCrosSettingsTrust* GetInstance() {
    return Singleton<UserCrosSettingsTrust>::get();
  }

  // Working horse for UserCrosSettingsProvider::RequestTrusted* family.
  bool RequestTrustedEntity(const std::string& name) {
    OwnershipService::Status ownership_status =
        ownership_service_->GetStatus(false);
    if (ownership_status == OwnershipService::OWNERSHIP_NONE)
      return true;
    PrefService* prefs = g_browser_process->local_state();
    if (prefs->IsManagedPreference(name.c_str()))
      return true;
    if (ownership_status == OwnershipService::OWNERSHIP_TAKEN) {
      DCHECK(g_browser_process);
      PrefService* prefs = g_browser_process->local_state();
      DCHECK(prefs);
      if (prefs->GetBoolean((name + kTrustedSuffix).c_str()))
        return true;
    }
    return false;
  }

  bool RequestTrustedEntity(const std::string& name, Task* callback) {
    if (RequestTrustedEntity(name)) {
      delete callback;
      return true;
    } else {
      if (callback)
        callbacks_[name].push_back(callback);
      return false;
    }
  }

  void Set(const std::string& path, Value* in_value) {
    PrefService* prefs = g_browser_process->local_state();
    DCHECK(!prefs->IsManagedPreference(path.c_str()));

    if (!UserManager::Get()->current_user_is_owner()) {
      LOG(WARNING) << "Changing settings from non-owner, setting=" << path;

      // Revert UI change.
      CrosSettings::Get()->FireObservers(path.c_str());
      return;
    }

    if (IsControlledBooleanSetting(path)) {
      bool bool_value = false;
      if (in_value->GetAsBoolean(&bool_value)) {
        std::string value = bool_value ? kTrueIncantation : kFalseIncantation;
        SignedSettingsHelper::Get()->StartStorePropertyOp(path, value, this);
        UpdateCacheBool(path, bool_value, USE_VALUE_SUPPLIED);

        VLOG(1) << "Set cros setting " << path << "=" << value;
      }
    } else if (path == kDeviceOwner) {
      VLOG(1) << "Setting owner is not supported. Please use "
                 "'UpdateCachedOwner' instead.";
    } else if (path == kAccountsPrefUsers) {
      VLOG(1) << "Setting user whitelist is not implemented.  Please use "
                 "whitelist/unwhitelist instead.";
    } else {
      LOG(WARNING) << "Try to set unhandled cros setting " << path;
    }
  }

 private:
  // upper bound for number of retries to fetch a signed setting.
  static const int kNumRetriesLimit = 9;

  UserCrosSettingsTrust()
      : ownership_service_(OwnershipService::GetSharedInstance()),
        retries_left_(kNumRetriesLimit) {
    // Start prefetching Boolean and String preferences.
    for (size_t i = 0; i < arraysize(kBooleanSettings); ++i)
      StartFetchingSetting(kBooleanSettings[i]);
    for (size_t i = 0; i < arraysize(kStringSettings); ++i)
      StartFetchingSetting(kStringSettings[i]);
  }

  ~UserCrosSettingsTrust() {
    if (BrowserThread::CurrentlyOn(BrowserThread::UI) &&
        CrosLibrary::Get()->EnsureLoaded()) {
      // Cancels all pending callbacks from us.
      SignedSettingsHelper::Get()->CancelCallback(this);
    }
  }

  void StartFetchingSetting(const std::string& name) {
    DCHECK(g_browser_process);
    PrefService* prefs = g_browser_process->local_state();
    if (!prefs)
      return;
    // Do not trust before fetching complete.
    prefs->ClearPref((name + kTrustedSuffix).c_str());
    prefs->ScheduleSavePersistentPrefs();
    if (CrosLibrary::Get()->EnsureLoaded()) {
      SignedSettingsHelper::Get()->StartRetrieveProperty(name, this);
    }
  }

  // Implementation of SignedSettingsHelper::Callback.
  virtual void OnRetrievePropertyCompleted(SignedSettings::ReturnCode code,
                                           const std::string& name,
                                           const std::string& value) {
    if (!IsControlledBooleanSetting(name) && !IsControlledStringSetting(name)) {
      NOTREACHED();
      return;
    }

    bool is_owned = ownership_service_->GetStatus(true) ==
        OwnershipService::OWNERSHIP_TAKEN;
    PrefService* prefs = g_browser_process->local_state();
    switch (code) {
      case SignedSettings::SUCCESS:
      case SignedSettings::NOT_FOUND:
      case SignedSettings::KEY_UNAVAILABLE: {
        bool fallback_to_default = !is_owned
            || (code == SignedSettings::NOT_FOUND);
        DCHECK(fallback_to_default || code == SignedSettings::SUCCESS);
        if (fallback_to_default)
          VLOG(1) << "Going default for cros setting " << name;
        else
          VLOG(1) << "Retrieved cros setting " << name << "=" << value;
        if (IsControlledBooleanSetting(name)) {
          // We assume our boolean settings are true before explicitly set.
          UpdateCacheBool(name, (value == kTrueIncantation),
              fallback_to_default ? USE_VALUE_DEFAULT : USE_VALUE_SUPPLIED);
        } else if (IsControlledStringSetting(name)) {
          UpdateCacheString(name, value,
              fallback_to_default ? USE_VALUE_DEFAULT : USE_VALUE_SUPPLIED);
        }
        break;
      }
      case SignedSettings::OPERATION_FAILED:
      default: {
        DCHECK(code == SignedSettings::OPERATION_FAILED);
        DCHECK(is_owned);
        LOG(ERROR) << "On owned device: failed to retrieve cros "
                      "setting, name=" << name;
        if (retries_left_ > 0) {
          retries_left_ -= 1;
          StartFetchingSetting(name);
          return;
        }
        LOG(ERROR) << "No retries left";
        if (IsControlledBooleanSetting(name)) {
          // For boolean settings we can just set safe (false) values
          // and continue as trusted.
          UpdateCacheBool(name, false, USE_VALUE_SUPPLIED);
        } else {
          prefs->ClearPref((name + kTrustedSuffix).c_str());
          return;
        }
        break;
      }
    }
    prefs->SetBoolean((name + kTrustedSuffix).c_str(), true);
    {
      std::vector<Task*>& callbacks_vector = callbacks_[name];
      for (size_t i = 0; i < callbacks_vector.size(); ++i)
        MessageLoop::current()->PostTask(FROM_HERE, callbacks_vector[i]);
      callbacks_vector.clear();
    }
    if (code == SignedSettings::SUCCESS)
      CrosSettings::Get()->FireObservers(name.c_str());
  }

  // Implementation of SignedSettingsHelper::Callback.
  virtual void OnStorePropertyCompleted(SignedSettings::ReturnCode code,
                                        const std::string& name,
                                        const std::string& value) {
    VLOG(1) << "Store cros setting " << name << "=" << value << ", code="
            << code;

    // Reload the setting if store op fails.
    if (code != SignedSettings::SUCCESS)
      SignedSettingsHelper::Get()->StartRetrieveProperty(name, this);
  }

  // Implementation of SignedSettingsHelper::Callback.
  virtual void OnWhitelistCompleted(SignedSettings::ReturnCode code,
                                    const std::string& email) {
    VLOG(1) << "Add " << email << " to whitelist, code=" << code;

    // Reload the whitelist on settings op failure.
    if (code != SignedSettings::SUCCESS)
      CrosSettings::Get()->FireObservers(kAccountsPrefUsers);
  }

  // Implementation of SignedSettingsHelper::Callback.
  virtual void OnUnwhitelistCompleted(SignedSettings::ReturnCode code,
                                      const std::string& email) {
    VLOG(1) << "Remove " << email << " from whitelist, code=" << code;

    // Reload the whitelist on settings op failure.
    if (code != SignedSettings::SUCCESS)
      CrosSettings::Get()->FireObservers(kAccountsPrefUsers);
  }

  // Pending callbacks that need to be invoked after settings verification.
  base::hash_map< std::string, std::vector< Task* > > callbacks_;

  OwnershipService* ownership_service_;

  // In order to guard against occasional failure to fetch a property
  // we allow for some number of retries.
  int retries_left_;

  friend class SignedSettingsHelper;
  friend struct DefaultSingletonTraits<UserCrosSettingsTrust>;

  DISALLOW_COPY_AND_ASSIGN(UserCrosSettingsTrust);
};

}  // namespace

}  // namespace chromeos

// We want to use NewRunnableMethod with this class but need to disable
// reference counting since it is singleton.
DISABLE_RUNNABLE_METHOD_REFCOUNT(chromeos::UserCrosSettingsTrust);

namespace chromeos {

UserCrosSettingsProvider::UserCrosSettingsProvider() {
  // Trigger prefetching of settings.
  UserCrosSettingsTrust::GetInstance();
}

// static
void UserCrosSettingsProvider::RegisterPrefs(PrefService* local_state) {
  for (size_t i = 0; i < arraysize(kBooleanSettings); ++i)
    RegisterSetting(local_state, kBooleanSettings[i]);
  for (size_t i = 0; i < arraysize(kStringSettings); ++i)
    RegisterSetting(local_state, kStringSettings[i]);
  for (size_t i = 0; i < arraysize(kListSettings); ++i)
    RegisterSetting(local_state, kListSettings[i]);
}

bool UserCrosSettingsProvider::RequestTrustedAllowGuest(Task* callback) {
  return UserCrosSettingsTrust::GetInstance()->RequestTrustedEntity(
      kAccountsPrefAllowGuest, callback);
}

bool UserCrosSettingsProvider::RequestTrustedAllowNewUser(Task* callback) {
  return UserCrosSettingsTrust::GetInstance()->RequestTrustedEntity(
      kAccountsPrefAllowNewUser, callback);
}

bool UserCrosSettingsProvider::RequestTrustedShowUsersOnSignin(Task* callback) {
  return UserCrosSettingsTrust::GetInstance()->RequestTrustedEntity(
      kAccountsPrefShowUserNamesOnSignIn, callback);
}

bool UserCrosSettingsProvider::RequestTrustedOwner(Task* callback) {
  return UserCrosSettingsTrust::GetInstance()->RequestTrustedEntity(
      kDeviceOwner, callback);
}

// static
bool UserCrosSettingsProvider::cached_allow_guest() {
  // Trigger prefetching if singleton object still does not exist.
  UserCrosSettingsTrust::GetInstance();
  return g_browser_process->local_state()->GetBoolean(kAccountsPrefAllowGuest);
}

// static
bool UserCrosSettingsProvider::cached_allow_new_user() {
  // Trigger prefetching if singleton object still does not exist.
  UserCrosSettingsTrust::GetInstance();
  return g_browser_process->local_state()->GetBoolean(
    kAccountsPrefAllowNewUser);
}

// static
bool UserCrosSettingsProvider::cached_show_users_on_signin() {
  // Trigger prefetching if singleton object still does not exist.
  UserCrosSettingsTrust::GetInstance();
  return g_browser_process->local_state()->GetBoolean(
      kAccountsPrefShowUserNamesOnSignIn);
}

// static
const ListValue* UserCrosSettingsProvider::cached_whitelist() {
  PrefService* prefs = g_browser_process->local_state();
  const ListValue* cached_users = prefs->GetList(kAccountsPrefUsers);
  if (!prefs->IsManagedPreference(kAccountsPrefUsers)) {
    if (cached_users == NULL) {
      // Update whitelist cache.
      GetUserWhitelist(NULL);
      cached_users = prefs->GetList(kAccountsPrefUsers);
    }
  }
  if (cached_users == NULL) {
    NOTREACHED();
    cached_users = new ListValue;
  }
  return cached_users;
}

// static
std::string UserCrosSettingsProvider::cached_owner() {
  // Trigger prefetching if singleton object still does not exist.
  UserCrosSettingsTrust::GetInstance();
  if (!g_browser_process || !g_browser_process->local_state())
    return std::string();
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
  UserCrosSettingsTrust::GetInstance()->Set(path, in_value);
}

bool UserCrosSettingsProvider::Get(const std::string& path,
                                   Value** out_value) const {
  if (IsControlledBooleanSetting(path)) {
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

void UserCrosSettingsProvider::WhitelistUser(const std::string& email) {
  SignedSettingsHelper::Get()->StartWhitelistOp(
      email, true, UserCrosSettingsTrust::GetInstance());
  PrefService* prefs = g_browser_process->local_state();
  ListValue* cached_whitelist = prefs->GetMutableList(kAccountsPrefUsers);
  cached_whitelist->Append(Value::CreateStringValue(email));
  prefs->ScheduleSavePersistentPrefs();
}

void UserCrosSettingsProvider::UnwhitelistUser(const std::string& email) {
  SignedSettingsHelper::Get()->StartWhitelistOp(
      email, false, UserCrosSettingsTrust::GetInstance());

  PrefService* prefs = g_browser_process->local_state();
  ListValue* cached_whitelist = prefs->GetMutableList(kAccountsPrefUsers);
  StringValue email_value(email);
  if (cached_whitelist->Remove(email_value) != -1)
    prefs->ScheduleSavePersistentPrefs();
}

// static
void UserCrosSettingsProvider::UpdateCachedOwner(const std::string& email) {
  UpdateCacheString(kDeviceOwner, email, USE_VALUE_SUPPLIED);
}

}  // namespace chromeos
