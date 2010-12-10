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
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"

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

class UserCrosSettingsTrust : public SignedSettingsHelper::Callback,
                              public NotificationObserver {
 public:
  static UserCrosSettingsTrust* GetSharedInstance() {
    return Singleton<UserCrosSettingsTrust>::get();
  }

  // Working horse for UserCrosSettingsProvider::RequestTrusted* family.
  bool RequestTrustedEntity(const std::string& name, Task* callback) {
    if (GetOwnershipStatus() == OWNERSHIP_NONE)
      return true;
    if (GetOwnershipStatus() == OWNERSHIP_TAKEN) {
      DCHECK(g_browser_process);
      PrefService* prefs = g_browser_process->local_state();
      DCHECK(prefs);
      if (prefs->GetBoolean((name + kTrustedSuffix).c_str()))
        return true;
    }
    if (callback)
      callbacks_[name].push_back(callback);
    return false;
  }

  void Set(const std::string& path, Value* in_value) {
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
  // Listed in upgrade order.
  enum OwnershipStatus {
    OWNERSHIP_UNKNOWN = 0,
    OWNERSHIP_NONE,
    OWNERSHIP_TAKEN
  };

  // Used to discriminate different sources of ownership status info.
  enum OwnershipSource {
    SOURCE_FETCH,  // Info comes from FetchOwnershipStatus method.
    SOURCE_OBSERVE // Info comes from Observe method.
  };

  // upper bound for number of retries to fetch a signed setting.
  static const int kNumRetriesLimit = 9;

  UserCrosSettingsTrust() : ownership_status_(OWNERSHIP_UNKNOWN),
                            retries_left_(kNumRetriesLimit) {
    notification_registrar_.Add(this,
                                NotificationType::OWNERSHIP_TAKEN,
                                NotificationService::AllSources());
    // Start getting ownership status.
    BrowserThread::PostTask(
        BrowserThread::FILE,
        FROM_HERE,
        NewRunnableMethod(this, &UserCrosSettingsTrust::FetchOwnershipStatus));
  }

  ~UserCrosSettingsTrust() {
    if (BrowserThread::CurrentlyOn(BrowserThread::UI) &&
        CrosLibrary::Get()->EnsureLoaded()) {
      // Cancels all pending callbacks from us.
      SignedSettingsHelper::Get()->CancelCallback(this);
    }
  }

  void FetchOwnershipStatus() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
    OwnershipStatus status =
        OwnershipService::GetSharedInstance()->IsAlreadyOwned() ?
        OWNERSHIP_TAKEN : OWNERSHIP_NONE;
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        NewRunnableMethod(this, &UserCrosSettingsTrust::SetOwnershipStatus,
            status, SOURCE_FETCH));
  }

  void SetOwnershipStatus(OwnershipStatus new_status,
                          OwnershipSource source) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    DCHECK(new_status == OWNERSHIP_TAKEN || new_status == OWNERSHIP_NONE);
    if (source == SOURCE_FETCH) {
      DCHECK(ownership_status_ != OWNERSHIP_NONE);
      if (ownership_status_ == OWNERSHIP_TAKEN) {
        // OWNERSHIP_TAKEN notification was observed earlier.
        return;
      }
    }
    ownership_status_ = new_status;
    if (source == SOURCE_FETCH) {
      // Start prefetching Boolean and String preferences.
      for (size_t i = 0; i < arraysize(kBooleanSettings); ++i)
        StartFetchingSetting(kBooleanSettings[i]);
      for (size_t i = 0; i < arraysize(kStringSettings); ++i)
        StartFetchingSetting(kStringSettings[i]);
    } else if (source == SOURCE_OBSERVE) {
      DCHECK(new_status == OWNERSHIP_TAKEN);
      if (CrosLibrary::Get()->EnsureLoaded()) {
        // TODO(dilmah,cmasone): We would not need following piece of code as
        // long as failure callback from SignedSettings will allow us to
        // discriminate missing settings from failed signature verification.
        // Otherwise we must set default values for boolean settings.
        UserManager::Get()->set_current_user_is_owner(true);
        for (size_t i = 0; i < arraysize(kBooleanSettings); ++i)
          Set(kBooleanSettings[i], Value::CreateBooleanValue(true));
      }
    }
  }

  // Returns ownership status.
  // Called on UI thread unlike OwnershipService::IsAlreadyOwned.
  OwnershipStatus GetOwnershipStatus() {
    return ownership_status_;
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
    DCHECK(GetOwnershipStatus() != OWNERSHIP_UNKNOWN);

    bool success = code == SignedSettings::SUCCESS;
    PrefService* prefs = g_browser_process->local_state();
    if (!success && GetOwnershipStatus() == OWNERSHIP_TAKEN) {
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
        // TODO(dilmah): after http://crosbug.com/9666 is fixed:
        // replace it to USE_VALUE_SUPPLIED.
        // Until that we use default value as a quick "fix" for:
        // http://crosbug.com/9876 and http://crosbug.com/9818
        // Since we cannot discriminate missing and spoofed setting ATM: assume
        // that it is missing (common case for old images).
        UpdateCacheBool(name, false, USE_VALUE_DEFAULT);
      } else {
        prefs->ClearPref((name + kTrustedSuffix).c_str());
        return;
      }
    } else {
      DCHECK(success || GetOwnershipStatus() == OWNERSHIP_NONE);
      if (success)
        VLOG(1) << "Retrieved cros setting " << name << "=" << value;
      else
        VLOG(1) << "We are disowned. Going default for cros setting " << name;
      if (IsControlledBooleanSetting(name)) {
        // Our boolean settings are true by default (if not explicitly present).
        UpdateCacheBool(name, (value == kTrueIncantation),
                        success ? USE_VALUE_SUPPLIED : USE_VALUE_DEFAULT);
      } else if (IsControlledStringSetting(name)) {
        UpdateCacheString(name, value,
                          success ? USE_VALUE_SUPPLIED : USE_VALUE_DEFAULT);
      }
    }
    prefs->SetBoolean((name + kTrustedSuffix).c_str(), true);
    {
      DCHECK(GetOwnershipStatus() != OWNERSHIP_UNKNOWN);
      std::vector<Task*>& callbacks_vector = callbacks_[name];
      for (size_t i = 0; i < callbacks_vector.size(); ++i)
        MessageLoop::current()->PostTask(FROM_HERE, callbacks_vector[i]);
      callbacks_vector.clear();
    }
    if (success)
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

  // NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    if (type.value == NotificationType::OWNERSHIP_TAKEN) {
      SetOwnershipStatus(OWNERSHIP_TAKEN, SOURCE_OBSERVE);
      notification_registrar_.RemoveAll();
    } else {
      NOTREACHED();
    }
  }

  // Pending callbacks that need to be invoked after settings verification.
  base::hash_map< std::string, std::vector< Task* > > callbacks_;

  NotificationRegistrar notification_registrar_;
  OwnershipStatus ownership_status_;

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
  UserCrosSettingsTrust::GetSharedInstance();
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
  return UserCrosSettingsTrust::GetSharedInstance()->RequestTrustedEntity(
      kAccountsPrefAllowGuest, callback);
}

bool UserCrosSettingsProvider::RequestTrustedAllowNewUser(Task* callback) {
  return UserCrosSettingsTrust::GetSharedInstance()->RequestTrustedEntity(
      kAccountsPrefAllowNewUser, callback);
}

bool UserCrosSettingsProvider::RequestTrustedShowUsersOnSignin(Task* callback) {
  return UserCrosSettingsTrust::GetSharedInstance()->RequestTrustedEntity(
      kAccountsPrefShowUserNamesOnSignIn, callback);
}

bool UserCrosSettingsProvider::RequestTrustedOwner(Task* callback) {
  return UserCrosSettingsTrust::GetSharedInstance()->RequestTrustedEntity(
      kDeviceOwner, callback);
}

// static
bool UserCrosSettingsProvider::cached_allow_guest() {
  // Trigger prefetching if singleton object still does not exist.
  UserCrosSettingsTrust::GetSharedInstance();
  return g_browser_process->local_state()->GetBoolean(kAccountsPrefAllowGuest);
}

// static
bool UserCrosSettingsProvider::cached_allow_new_user() {
  // Trigger prefetching if singleton object still does not exist.
  UserCrosSettingsTrust::GetSharedInstance();
  return g_browser_process->local_state()->GetBoolean(
    kAccountsPrefAllowNewUser);
}

// static
bool UserCrosSettingsProvider::cached_show_users_on_signin() {
  // Trigger prefetching if singleton object still does not exist.
  UserCrosSettingsTrust::GetSharedInstance();
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
  // Trigger prefetching if singleton object still does not exist.
  UserCrosSettingsTrust::GetSharedInstance();
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
  UserCrosSettingsTrust::GetSharedInstance()->Set(path, in_value);
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
      email, true, UserCrosSettingsTrust::GetSharedInstance());
  PrefService* prefs = g_browser_process->local_state();
  ListValue* cached_whitelist = prefs->GetMutableList(kAccountsPrefUsers);
  cached_whitelist->Append(Value::CreateStringValue(email));
  prefs->ScheduleSavePersistentPrefs();
}

void UserCrosSettingsProvider::UnwhitelistUser(const std::string& email) {
  SignedSettingsHelper::Get()->StartWhitelistOp(
      email, false, UserCrosSettingsTrust::GetSharedInstance());

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
