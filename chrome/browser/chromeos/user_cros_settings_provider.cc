// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/user_cros_settings_provider.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/hash_tables.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/cros_settings.h"
#include "chrome/browser/chromeos/cros_settings_names.h"
#include "chrome/browser/chromeos/login/ownership_service.h"
#include "chrome/browser/chromeos/login/ownership_status_checker.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/pref_value_map.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/ui/options/options_util.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/installer/util/google_update_settings.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"

using content::BrowserThread;

namespace chromeos {

namespace {

const char kTrueIncantation[] = "true";
const char kFalseIncantation[] = "false";
const char kTrustedSuffix[] = "/trusted";

// For all our boolean settings following is applicable:
// true is default permissive value and false is safe prohibitic value.
// Exception: kSignedDataRoamingEnabled which has default value of false.
const char* kBooleanSettings[] = {
  kAccountsPrefAllowNewUser,
  kAccountsPrefAllowGuest,
  kAccountsPrefShowUserNamesOnSignIn,
  kSignedDataRoamingEnabled,
  kStatsReportingPref
};

const char* kStringSettings[] = {
  kDeviceOwner,
  kReleaseChannel
};

const char* kListSettings[] = {
  kAccountsPrefUsers
};

// This class provides the means to migrate settings to the signed settings
// store. It does one of three things - store the settings in the policy blob
// immediately if the current user is the owner. Uses the
// SignedSettingsTempStorage if there is no owner yet, or waits for an
// OWNERSHIP_CHECKED notification to delay the storing until the owner has
// logged in.
class MigrationHelper : public content::NotificationObserver {
 public:
  explicit MigrationHelper() : callback_(NULL) {
    registrar_.Add(this, chrome::NOTIFICATION_OWNERSHIP_CHECKED,
                   content::NotificationService::AllSources());
  }

  void set_callback(SignedSettingsHelper::Callback* callback) {
    callback_ = callback;
  }

  void AddMigrationValue(const std::string& path, base::Value* value) {
    migration_values_.SetValue(path, value);
  }

  void MigrateValues(void) {
    ownership_checker_.reset(new OwnershipStatusChecker(
        base::Bind(&MigrationHelper::DoMigrateValues, base::Unretained(this))));
  }

  // NotificationObserver overrides:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE {
    if (type == chrome::NOTIFICATION_OWNERSHIP_CHECKED)
      MigrateValues();
  }

 private:
  void DoMigrateValues(OwnershipService::Status status,
                       bool current_user_is_owner) {
    ownership_checker_.reset(NULL);

    // We can call StartStorePropertyOp in two cases - either if the owner is
    // currently logged in and the policy can be updated immediately or if there
    // is no owner yet in which case the value will be temporarily stored in the
    // SignedSettingsTempStorage until the device is owned. If none of these
    // cases is met then we will wait for user change notification and retry.
    if (current_user_is_owner || status != OwnershipService::OWNERSHIP_TAKEN) {
      PrefValueMap::const_iterator i;
      for (i = migration_values_.begin(); i != migration_values_.end(); ++i) {
        // Queue all values for storing.
        SignedSettingsHelper::Get()->StartStorePropertyOp(i->first, *i->second,
                                                          callback_);
      }
      migration_values_.Clear();
    }
  }

  content::NotificationRegistrar registrar_;
  scoped_ptr<OwnershipStatusChecker> ownership_checker_;
  SignedSettingsHelper::Callback* callback_;
  PrefValueMap migration_values_;

  DISALLOW_COPY_AND_ASSIGN(MigrationHelper);
};

bool IsControlledBooleanSetting(const std::string& pref_path) {
  // TODO(nkostylev): Using std::find for 4 value array generates this warning
  // in chroot stl_algo.h:231: error: array subscript is above array bounds.
  // GCC 4.4.3
  return (pref_path == kAccountsPrefAllowNewUser) ||
         (pref_path == kAccountsPrefAllowGuest) ||
         (pref_path == kAccountsPrefShowUserNamesOnSignIn) ||
         (pref_path == kSignedDataRoamingEnabled) ||
         (pref_path == kStatsReportingPref);
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

bool IsControlledSetting(const std::string& pref_path) {
  return (IsControlledBooleanSetting(pref_path) ||
          IsControlledStringSetting(pref_path) ||
          IsControlledListSetting(pref_path));
}

void RegisterSetting(PrefService* local_state, const std::string& pref_path) {
  local_state->RegisterBooleanPref((pref_path + kTrustedSuffix).c_str(),
                                   false,
                                   PrefService::UNSYNCABLE_PREF);
  if (IsControlledBooleanSetting(pref_path)) {
    if (pref_path == kSignedDataRoamingEnabled ||
        pref_path == kStatsReportingPref)
      local_state->RegisterBooleanPref(pref_path.c_str(),
                                       false,
                                       PrefService::UNSYNCABLE_PREF);
    else
      local_state->RegisterBooleanPref(pref_path.c_str(),
                                       true,
                                       PrefService::UNSYNCABLE_PREF);
  } else if (IsControlledStringSetting(pref_path)) {
    local_state->RegisterStringPref(pref_path.c_str(),
                                    "",
                                    PrefService::UNSYNCABLE_PREF);
  } else {
    DCHECK(IsControlledListSetting(pref_path));
    local_state->RegisterListPref(pref_path.c_str(),
                                  PrefService::UNSYNCABLE_PREF);
  }
}

enum UseValue {
  USE_VALUE_SUPPLIED,
  USE_VALUE_DEFAULT
};

void UpdateCache(const std::string& name,
                 const base::Value* value,
                 UseValue use_value) {
  PrefService* prefs = g_browser_process->local_state();
  if (use_value == USE_VALUE_DEFAULT)
    prefs->ClearPref(name.c_str());
  else
    prefs->Set(name.c_str(), *value);
  prefs->ScheduleSavePersistentPrefs();
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

  bool RequestTrustedEntity(const std::string& name,
                            const base::Closure& callback) {
    if (RequestTrustedEntity(name)) {
      return true;
    } else {
      if (!callback.is_null())
        callbacks_[name].push_back(callback);
      return false;
    }
  }

  void Reload() {
    for (size_t i = 0; i < arraysize(kBooleanSettings); ++i)
      StartFetchingSetting(kBooleanSettings[i]);
    for (size_t i = 0; i < arraysize(kStringSettings); ++i)
      StartFetchingSetting(kStringSettings[i]);
    for (size_t i = 0; i < arraysize(kListSettings); ++i)
      StartFetchingSetting(kListSettings[i]);
  }

  void Set(const std::string& path, const base::Value& in_value) {
    PrefService* prefs = g_browser_process->local_state();
    DCHECK(!prefs->IsManagedPreference(path.c_str()));

    if (!UserManager::Get()->current_user_is_owner()) {
      LOG(WARNING) << "Changing settings from non-owner, setting=" << path;

      // Revert UI change.
      CrosSettings::Get()->FireObservers(path.c_str());
      return;
    }
    if (IsControlledSetting(path)) {
      if (IsControlledBooleanSetting(path)) {
        bool bool_value = false;
        if (in_value.GetAsBoolean(&bool_value)) {
          OnBooleanPropertyChange(path, bool_value);
        }
      }
      SignedSettingsHelper::Get()->StartStorePropertyOp(
          path, in_value, this);
      UpdateCache(path, &in_value, USE_VALUE_SUPPLIED);

      LOG(ERROR) << "Set cros setting " << path;
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
    migration_helper_.set_callback(this);
    // Start prefetching Boolean and String preferences.
    Reload();
  }

  virtual ~UserCrosSettingsTrust() {
    if (BrowserThread::CurrentlyOn(BrowserThread::UI)) {
      // Cancels all pending callbacks from us.
      SignedSettingsHelper::Get()->CancelCallback(this);
    }
  }

  // Called right before boolean property is changed.
  void OnBooleanPropertyChange(const std::string& path, bool new_value) {
    if (path == kSignedDataRoamingEnabled) {
      NetworkLibrary* cros = CrosLibrary::Get()->GetNetworkLibrary();
      if (cros->IsCellularAlwaysInRoaming()) {
        // If operator requires roaming always enabled, ignore supplied value
        // and set data roaming allowed in true always.
        new_value = true;
      }
      cros->SetCellularDataRoamingAllowed(new_value);
    } else if (path == kStatsReportingPref) {
      // TODO(pastarmovj): Remove this once we don't need to regenerate the
      // consent file for the GUID anymore.
      OptionsUtil::ResolveMetricsReportingEnabled(new_value);
    }
  }

  // Called right after signed value was checked.
  void OnPropertyRetrieve(const std::string& path,
                          const base::Value* value,
                          UseValue use_value) {
    if (path == kSignedDataRoamingEnabled) {
      NetworkLibrary* cros = CrosLibrary::Get()->GetNetworkLibrary();
      const NetworkDevice* cellular = cros->FindCellularDevice();
      if (cellular) {
        bool device_value = cellular->data_roaming_allowed();
        if (!device_value && cros->IsCellularAlwaysInRoaming()) {
          // If operator requires roaming always enabled, ignore supplied value
          // and set data roaming allowed in true always.
          cros->SetCellularDataRoamingAllowed(true);
        } else {
          bool new_value = false;
          if (use_value == USE_VALUE_SUPPLIED)
            value->GetAsBoolean(&new_value);
          if (device_value != new_value)
            cros->SetCellularDataRoamingAllowed(new_value);
        }
      }
    } else if (path == kStatsReportingPref) {
      bool stats_consent = false;
      if (use_value == USE_VALUE_SUPPLIED)
        value->GetAsBoolean(&stats_consent);
      // TODO(pastarmovj): Remove this once migration is not needed anymore.
      // If the value is not set we should try to migrate legacy consent file.
      if (use_value == USE_VALUE_DEFAULT) {
        // Loading consent file state causes us to do blocking IO on UI thread.
        // Temporarily allow it until we fix http://crbug.com/62626
        base::ThreadRestrictions::ScopedAllowIO allow_io;
        stats_consent = GoogleUpdateSettings::GetCollectStatsConsent();
        // Make sure the values will get eventually written to the policy file.
        migration_helper_.AddMigrationValue(
            path, base::Value::CreateBooleanValue(stats_consent));
        migration_helper_.MigrateValues();
        base::FundamentalValue base_value(stats_consent);
        UpdateCache(path, &base_value, USE_VALUE_SUPPLIED);
        LOG(WARNING) << "No metrics policy set will revert to checking "
                     << "consent file which is "
                     << (stats_consent ? "on." : "off.");
      }
      // TODO(pastarmovj): Remove this once we don't need to regenerate the
      // consent file for the GUID anymore.
      VLOG(1) << "Metrics policy is being set to : " << stats_consent
              << "(reason : " << use_value << ")";
      OptionsUtil::ResolveMetricsReportingEnabled(stats_consent);
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
    SignedSettingsHelper::Get()->StartRetrieveProperty(name, this);
  }

  // Implementation of SignedSettingsHelper::Callback.
  virtual void OnRetrievePropertyCompleted(SignedSettings::ReturnCode code,
                                           const std::string& name,
                                           const base::Value* value) {
    if (!IsControlledSetting(name)) {
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
          VLOG(1) << "Retrieved cros setting " << name;
        UpdateCache(
            name, value,
            fallback_to_default ? USE_VALUE_DEFAULT : USE_VALUE_SUPPLIED);
        OnPropertyRetrieve(
            name, value,
            fallback_to_default ? USE_VALUE_DEFAULT : USE_VALUE_SUPPLIED);
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
          scoped_ptr<base::Value> false_value(
              base::Value::CreateBooleanValue(false));
          OnPropertyRetrieve(name, false_value.get(), USE_VALUE_SUPPLIED);
          UpdateCache(name, false_value.get(), USE_VALUE_SUPPLIED);
        } else {
          prefs->ClearPref((name + kTrustedSuffix).c_str());
          return;
        }
        break;
      }
    }
    prefs->SetBoolean((name + kTrustedSuffix).c_str(), true);
    {
      std::vector<base::Closure>& callbacks_vector = callbacks_[name];
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
                                        const base::Value& value) {
    VLOG(1) << "Store cros setting " << name << ", code=" << code;

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
  base::hash_map<std::string, std::vector<base::Closure> > callbacks_;

  OwnershipService* ownership_service_;
  MigrationHelper migration_helper_;

  // In order to guard against occasional failure to fetch a property
  // we allow for some number of retries.
  int retries_left_;

  friend class SignedSettingsHelper;
  friend struct DefaultSingletonTraits<UserCrosSettingsTrust>;

  DISALLOW_COPY_AND_ASSIGN(UserCrosSettingsTrust);
};

}  // namespace

}  // namespace chromeos

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

void UserCrosSettingsProvider::Reload() {
  UserCrosSettingsTrust::GetInstance()->Reload();
}

void UserCrosSettingsProvider::DoSet(const std::string& path,
                                     const base::Value& in_value) {
  UserCrosSettingsTrust::GetInstance()->Set(path, in_value);
}

const base::Value* UserCrosSettingsProvider::Get(
    const std::string& path) const {
  if (HandlesSetting(path)) {
    const PrefService* prefs = g_browser_process->local_state();
    const PrefService::Preference* pref = prefs->FindPreference(path.c_str());
    return pref->GetValue();
  }
  return NULL;
}

bool UserCrosSettingsProvider::GetTrusted(const std::string& path,
                                          const base::Closure& callback) const {
  return UserCrosSettingsTrust::GetInstance()->RequestTrustedEntity(
      path, callback);
}

bool UserCrosSettingsProvider::HandlesSetting(const std::string& path) const {
  return ::StartsWithASCII(path, "cros.accounts.", true) ||
      ::StartsWithASCII(path, "cros.signed.", true) ||
      ::StartsWithASCII(path, "cros.metrics.", true) ||
      path == kDeviceOwner ||
      path == kReleaseChannel;
}

}  // namespace chromeos
