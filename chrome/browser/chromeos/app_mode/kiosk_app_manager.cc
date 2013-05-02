// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/kiosk_app_manager.h"

#include <map>
#include <set>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_data.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager_observer.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_paths.h"
#include "chromeos/cryptohome/async_method_caller.h"

namespace chromeos {

namespace {

std::string FormatKioskAppUserId(const std::string& app_id) {
  return app_id + '@' + UserManager::kKioskAppUserDomain;
}

void OnRemoveAppCryptohomeComplete(const std::string& app,
                                   bool success,
                                   cryptohome::MountError return_code) {
  if (!success) {
    LOG(ERROR) << "Remove cryptohome for " << app
        << " failed, return code: " << return_code;
  }
}

// Decodes a device-local account dictionary and extracts the |account_id| and
// |app_id| if decoding is successful and the entry refers to a Kiosk App.
bool DecodeDeviceLocalAccount(const base::Value* account_spec,
                              std::string* account_id,
                              std::string* app_id) {
  const base::DictionaryValue* account_dict = NULL;
  if (!account_spec->GetAsDictionary(&account_dict)) {
    NOTREACHED();
    return false;
  }

  if (!account_dict->GetStringWithoutPathExpansion(
          kAccountsPrefDeviceLocalAccountsKeyId, account_id)) {
    LOG(ERROR) << "Account ID missing";
    return false;
  }

  int type;
  if (!account_dict->GetIntegerWithoutPathExpansion(
          kAccountsPrefDeviceLocalAccountsKeyType, &type) ||
      type != DEVICE_LOCAL_ACCOUNT_TYPE_KIOSK_APP) {
    // Not a kiosk app.
    return false;
  }

  if (!account_dict->GetStringWithoutPathExpansion(
          kAccountsPrefDeviceLocalAccountsKeyKioskAppId, app_id)) {
    LOG(ERROR) << "Kiosk app id missing for " << *account_id;
    return false;
  }

  return true;
}

}  // namespace

// static
const char KioskAppManager::kKioskDictionaryName[] = "kiosk";
const char KioskAppManager::kKeyApps[] = "apps";
const char KioskAppManager::kIconCacheDir[] = "kiosk";

// static
static base::LazyInstance<KioskAppManager> instance = LAZY_INSTANCE_INITIALIZER;
KioskAppManager* KioskAppManager::Get() {
  return instance.Pointer();
}

// static
void KioskAppManager::Shutdown() {
  if (instance == NULL)
    return;

  instance.Pointer()->CleanUp();
}

// static
void KioskAppManager::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kKioskDictionaryName);
}

KioskAppManager::App::App(const KioskAppData& data)
    : id(data.id()),
      name(data.name()),
      icon(data.icon()),
      is_loading(data.IsLoading()) {
}

KioskAppManager::App::App() : is_loading(false) {}
KioskAppManager::App::~App() {}

std::string KioskAppManager::GetAutoLaunchApp() const {
  return auto_launch_app_id_;
}

void KioskAppManager::SetAutoLaunchApp(const std::string& app_id) {
  CrosSettings::Get()->SetString(
      kAccountsPrefDeviceLocalAccountAutoLoginId,
      app_id.empty() ? std::string() : FormatKioskAppUserId(app_id));
  CrosSettings::Get()->SetInteger(
      kAccountsPrefDeviceLocalAccountAutoLoginDelay, 0);
}

void KioskAppManager::AddApp(const std::string& app_id) {
  CrosSettings* cros_settings = CrosSettings::Get();
  const base::ListValue* accounts_list = NULL;
  cros_settings->GetList(kAccountsPrefDeviceLocalAccounts, &accounts_list);

  // Don't insert if the app if it's already in the list.
  base::ListValue new_accounts_list;
  if (accounts_list) {
    for (base::ListValue::const_iterator entry(accounts_list->begin());
         entry != accounts_list->end(); ++entry) {
      std::string account_id;
      std::string kiosk_app_id;
      if (DecodeDeviceLocalAccount(*entry, &account_id, &kiosk_app_id) &&
          kiosk_app_id == app_id) {
        return;
      }
      new_accounts_list.Append((*entry)->DeepCopy());
    }
  }

  // Add the new account.
  scoped_ptr<base::DictionaryValue> new_entry(new base::DictionaryValue());
  new_entry->SetStringWithoutPathExpansion(
      kAccountsPrefDeviceLocalAccountsKeyId, FormatKioskAppUserId(app_id));
  new_entry->SetIntegerWithoutPathExpansion(
      kAccountsPrefDeviceLocalAccountsKeyType,
      DEVICE_LOCAL_ACCOUNT_TYPE_KIOSK_APP);
  new_entry->SetStringWithoutPathExpansion(
      kAccountsPrefDeviceLocalAccountsKeyKioskAppId, app_id);
  new_accounts_list.Append(new_entry.release());
  cros_settings->Set(kAccountsPrefDeviceLocalAccounts, new_accounts_list);
}

void KioskAppManager::RemoveApp(const std::string& app_id) {
  CrosSettings* cros_settings = CrosSettings::Get();
  const base::ListValue* accounts_list = NULL;
  cros_settings->GetList(kAccountsPrefDeviceLocalAccounts, &accounts_list);
  if (!accounts_list)
    return;

  // Duplicate the list, filtering out entries that match |app_id|.
  base::ListValue new_accounts_list;
  for (base::ListValue::const_iterator entry(accounts_list->begin());
       entry != accounts_list->end(); ++entry) {
    std::string account_id;
    std::string kiosk_app_id;
    if (DecodeDeviceLocalAccount(*entry, &account_id, &kiosk_app_id) &&
        kiosk_app_id == app_id) {
      continue;
    }
    new_accounts_list.Append((*entry)->DeepCopy());
  }

  cros_settings->Set(kAccountsPrefDeviceLocalAccounts, new_accounts_list);
}

void KioskAppManager::GetApps(Apps* apps) const {
  apps->reserve(apps_.size());
  for (size_t i = 0; i < apps_.size(); ++i)
    apps->push_back(App(*apps_[i]));
}

bool KioskAppManager::GetApp(const std::string& app_id, App* app) const {
  const KioskAppData* data = GetAppData(app_id);
  if (!data)
    return false;

  *app = App(*data);
  return true;
}

const base::RefCountedString* KioskAppManager::GetAppRawIcon(
    const std::string& app_id) const {
  const KioskAppData* data = GetAppData(app_id);
  if (!data)
    return NULL;

  return data->raw_icon();
}

bool KioskAppManager::GetDisableBailoutShortcut() const {
  bool enable;
  if (CrosSettings::Get()->GetBoolean(
          kAccountsPrefDeviceLocalAccountAutoLoginBailoutEnabled, &enable)) {
    return !enable;
  }

  return false;
}

void KioskAppManager::AddObserver(KioskAppManagerObserver* observer) {
  observers_.AddObserver(observer);
}

void KioskAppManager::RemoveObserver(KioskAppManagerObserver* observer) {
  observers_.RemoveObserver(observer);
}

KioskAppManager::KioskAppManager() {
  UpdateAppData();
  CrosSettings::Get()->AddSettingsObserver(
      kAccountsPrefDeviceLocalAccounts, this);
  CrosSettings::Get()->AddSettingsObserver(
      kAccountsPrefDeviceLocalAccountAutoLoginId, this);
}

KioskAppManager::~KioskAppManager() {}

void KioskAppManager::CleanUp() {
  CrosSettings::Get()->RemoveSettingsObserver(
      kAccountsPrefDeviceLocalAccounts, this);
  CrosSettings::Get()->RemoveSettingsObserver(
      kAccountsPrefDeviceLocalAccountAutoLoginId, this);
  apps_.clear();
}

const KioskAppData* KioskAppManager::GetAppData(
    const std::string& app_id) const {
  for (size_t i = 0; i < apps_.size(); ++i) {
    const KioskAppData* data = apps_[i];
    if (data->id() == app_id)
      return data;
  }

  return NULL;
}

void KioskAppManager::UpdateAppData() {
  // Gets app id to data mapping for existing apps.
  std::map<std::string, KioskAppData*> old_apps;
  for (size_t i = 0; i < apps_.size(); ++i)
    old_apps[apps_[i]->id()] = apps_[i];
  apps_.weak_clear();  // |old_apps| takes ownership

  auto_launch_app_id_.clear();
  std::string auto_login_account_id;
  CrosSettings::Get()->GetString(kAccountsPrefDeviceLocalAccountAutoLoginId,
                                 &auto_login_account_id);

  const base::ListValue* local_accounts;
  if (CrosSettings::Get()->GetList(kAccountsPrefDeviceLocalAccounts,
                                   &local_accounts)) {
    // Re-populates |apps_| and reuses existing KioskAppData when possible.
    for (base::ListValue::const_iterator account(local_accounts->begin());
         account != local_accounts->end(); ++account) {
      std::string account_id;
      std::string kiosk_app_id;
      if (!DecodeDeviceLocalAccount(*account, &account_id, &kiosk_app_id))
        continue;

      if (account_id == auto_login_account_id)
        auto_launch_app_id_ = kiosk_app_id;

      // TODO(mnissler): Support non-CWS update URLs.

      std::map<std::string, KioskAppData*>::iterator old_it =
          old_apps.find(kiosk_app_id);
      if (old_it != old_apps.end()) {
        apps_.push_back(old_it->second);
        old_apps.erase(old_it);
      } else {
        KioskAppData* new_app = new KioskAppData(this, kiosk_app_id);
        apps_.push_back(new_app);  // Takes ownership of |new_app|.
        new_app->Load();
      }
    }
  }

  // Clears cache and deletes the remaining old data.
  for (std::map<std::string, KioskAppData*>::iterator it = old_apps.begin();
       it != old_apps.end(); ++it) {
    it->second->ClearCache();
    cryptohome::AsyncMethodCaller::GetInstance()->AsyncRemove(
        it->first, base::Bind(&OnRemoveAppCryptohomeComplete, it->first));
  }
  STLDeleteValues(&old_apps);

  FOR_EACH_OBSERVER(KioskAppManagerObserver, observers_,
                    OnKioskAppsSettingsChanged());
}

void KioskAppManager::Observe(int type,
                              const content::NotificationSource& source,
                              const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_SYSTEM_SETTING_CHANGED, type);
  UpdateAppData();
}

void KioskAppManager::GetKioskAppIconCacheDir(base::FilePath* cache_dir) {
  base::FilePath user_data_dir;
  CHECK(PathService::Get(chrome::DIR_USER_DATA, &user_data_dir));
  *cache_dir = user_data_dir.AppendASCII(kIconCacheDir);
}

void KioskAppManager::OnKioskAppDataChanged(const std::string& app_id) {
  FOR_EACH_OBSERVER(KioskAppManagerObserver,
                    observers_,
                    OnKioskAppDataChanged(app_id));
}

void KioskAppManager::OnKioskAppDataLoadFailure(const std::string& app_id) {
  FOR_EACH_OBSERVER(KioskAppManagerObserver,
                    observers_,
                    OnKioskAppDataLoadFailure(app_id));
  RemoveApp(app_id);
}

}  // namespace chromeos
