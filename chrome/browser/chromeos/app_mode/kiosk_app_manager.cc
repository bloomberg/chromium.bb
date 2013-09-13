// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/kiosk_app_manager.h"

#include <map>
#include <set>

#include "base/bind.h"
#include "base/chromeos/chromeos_version.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/stl_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_data.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager_observer.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/policy/device_local_account.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/cros_settings_names.h"
#include "chrome/browser/chromeos/settings/owner_key_util.h"
#include "chrome/browser/policy/browser_policy_connector.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/common/chrome_paths.h"
#include "chromeos/cryptohome/async_method_caller.h"
#include "chromeos/cryptohome/cryptohome_library.h"
#include "content/public/browser/browser_thread.h"

namespace chromeos {

namespace {

// Domain that is used for kiosk-app account IDs.
const char kKioskAppAccountDomain[] = "kiosk-apps";

std::string GenerateKioskAppAccountId(const std::string& app_id) {
  return app_id + '@' + kKioskAppAccountDomain;
}

void OnRemoveAppCryptohomeComplete(const std::string& app,
                                   bool success,
                                   cryptohome::MountError return_code) {
  if (!success) {
    LOG(ERROR) << "Remove cryptohome for " << app
        << " failed, return code: " << return_code;
  }
}

// Check for presence of machine owner public key file.
void CheckOwnerFilePresence(bool *present) {
  scoped_refptr<OwnerKeyUtil> util = OwnerKeyUtil::Create();
  *present = util->IsPublicKeyPresent();
}

}  // namespace

// static
const char KioskAppManager::kKioskDictionaryName[] = "kiosk";
const char KioskAppManager::kKeyApps[] = "apps";
const char KioskAppManager::kKeyAutoLoginState[] = "auto_login_state";
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
    : app_id(data.app_id()),
      user_id(data.user_id()),
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
  SetAutoLoginState(AUTOLOGIN_REQUESTED);
  // Clean first, so the proper change notifications are triggered even
  // if we are only changing AutoLoginState here.
  if (!auto_launch_app_id_.empty()) {
    CrosSettings::Get()->SetString(kAccountsPrefDeviceLocalAccountAutoLoginId,
                                   std::string());
  }

  CrosSettings::Get()->SetString(
      kAccountsPrefDeviceLocalAccountAutoLoginId,
      app_id.empty() ? std::string() : GenerateKioskAppAccountId(app_id));
  CrosSettings::Get()->SetInteger(
      kAccountsPrefDeviceLocalAccountAutoLoginDelay, 0);
}

void KioskAppManager::EnableConsumerModeKiosk(
    const KioskAppManager::EnableKioskModeCallback& callback) {
  g_browser_process->browser_policy_connector()->GetInstallAttributes()->
      LockDevice(std::string(),  // user
                 policy::DEVICE_MODE_CONSUMER_KIOSK,
                 std::string(),  // device_id
                 base::Bind(&KioskAppManager::OnLockDevice,
                            base::Unretained(this),
                            callback));
}

void KioskAppManager::GetConsumerKioskModeStatus(
    const KioskAppManager::GetConsumerKioskModeStatusCallback& callback) {
  g_browser_process->browser_policy_connector()->GetInstallAttributes()->
      ReadImmutableAttributes(
          base::Bind(&KioskAppManager::OnReadImmutableAttributes,
                     base::Unretained(this),
                     callback));
}

void KioskAppManager::OnLockDevice(
    const KioskAppManager::EnableKioskModeCallback& callback,
    policy::EnterpriseInstallAttributes::LockResult result) {
  if (callback.is_null())
    return;

  callback.Run(result == policy::EnterpriseInstallAttributes::LOCK_SUCCESS);
}

void KioskAppManager::OnOwnerFileChecked(
    const KioskAppManager::GetConsumerKioskModeStatusCallback& callback,
    bool* owner_present) {
  ownership_established_ = *owner_present;

  if (callback.is_null())
    return;

  // If we have owner already established on the machine, don't let
  // consumer kiosk to be enabled.
  if (ownership_established_)
    callback.Run(CONSUMER_KIOSK_MODE_DISABLED);
  else
    callback.Run(CONSUMER_KIOSK_MODE_CONFIGURABLE);
}

void KioskAppManager::OnReadImmutableAttributes(
    const KioskAppManager::GetConsumerKioskModeStatusCallback& callback) {
  if (callback.is_null())
    return;

  ConsumerKioskModeStatus status = CONSUMER_KIOSK_MODE_DISABLED;
  policy::EnterpriseInstallAttributes* attributes =
      g_browser_process->browser_policy_connector()->GetInstallAttributes();
  switch (attributes->GetMode()) {
    case policy::DEVICE_MODE_NOT_SET: {
      if (!base::chromeos::IsRunningOnChromeOS()) {
        status = CONSUMER_KIOSK_MODE_CONFIGURABLE;
      } else if (!ownership_established_) {
        bool* owner_present = new bool(false);
        content::BrowserThread::PostBlockingPoolTaskAndReply(
            FROM_HERE,
            base::Bind(&CheckOwnerFilePresence,
                       owner_present),
            base::Bind(&KioskAppManager::OnOwnerFileChecked,
                       base::Unretained(this),
                       callback,
                       base::Owned(owner_present)));
        return;
      }
      break;
    }
    case policy::DEVICE_MODE_CONSUMER_KIOSK:
      status = CONSUMER_KIOSK_MODE_ENABLED;
      break;
    default:
      break;
  }

  callback.Run(status);
}

void KioskAppManager::SetEnableAutoLaunch(bool value) {
  SetAutoLoginState(value ? AUTOLOGIN_APPROVED : AUTOLOGIN_REJECTED);
}

bool KioskAppManager::IsAutoLaunchRequested() const {
  if (GetAutoLaunchApp().empty())
    return false;

  // Apps that were installed by the policy don't require machine owner
  // consent through UI.
  if (g_browser_process->browser_policy_connector()->IsEnterpriseManaged())
    return false;

  return GetAutoLoginState() == AUTOLOGIN_REQUESTED;
}

bool KioskAppManager::IsAutoLaunchEnabled() const {
  if (GetAutoLaunchApp().empty())
    return false;

  // Apps that were installed by the policy don't require machine owner
  // consent through UI.
  if (g_browser_process->browser_policy_connector()->IsEnterpriseManaged())
    return true;

  return GetAutoLoginState() == AUTOLOGIN_APPROVED;
}

void KioskAppManager::AddApp(const std::string& app_id) {
  std::vector<policy::DeviceLocalAccount> device_local_accounts =
      policy::GetDeviceLocalAccounts(CrosSettings::Get());

  // Don't insert the app if it's already in the list.
  for (std::vector<policy::DeviceLocalAccount>::const_iterator
           it = device_local_accounts.begin();
       it != device_local_accounts.end(); ++it) {
    if (it->type == policy::DeviceLocalAccount::TYPE_KIOSK_APP &&
        it->kiosk_app_id == app_id) {
      return;
    }
  }

  // Add the new account.
  device_local_accounts.push_back(policy::DeviceLocalAccount(
      policy::DeviceLocalAccount::TYPE_KIOSK_APP,
      GenerateKioskAppAccountId(app_id),
      app_id));

  policy::SetDeviceLocalAccounts(CrosSettings::Get(), device_local_accounts);
}

void KioskAppManager::RemoveApp(const std::string& app_id) {
  // Resets auto launch app if it is the removed app.
  if (auto_launch_app_id_ == app_id)
    SetAutoLaunchApp(std::string());

  std::vector<policy::DeviceLocalAccount> device_local_accounts =
      policy::GetDeviceLocalAccounts(CrosSettings::Get());
  if (device_local_accounts.empty())
    return;

  // Remove entries that match |app_id|.
  for (std::vector<policy::DeviceLocalAccount>::iterator
           it = device_local_accounts.begin();
       it != device_local_accounts.end(); ++it) {
    if (it->type == policy::DeviceLocalAccount::TYPE_KIOSK_APP &&
        it->kiosk_app_id == app_id) {
      device_local_accounts.erase(it);
      break;
    }
  }

  policy::SetDeviceLocalAccounts(CrosSettings::Get(), device_local_accounts);
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

KioskAppManager::KioskAppManager() : ownership_established_(false) {
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
    if (data->app_id() == app_id)
      return data;
  }

  return NULL;
}

void KioskAppManager::UpdateAppData() {
  // Gets app id to data mapping for existing apps.
  std::map<std::string, KioskAppData*> old_apps;
  for (size_t i = 0; i < apps_.size(); ++i)
    old_apps[apps_[i]->app_id()] = apps_[i];
  apps_.weak_clear();  // |old_apps| takes ownership

  auto_launch_app_id_.clear();
  std::string auto_login_account_id;
  CrosSettings::Get()->GetString(kAccountsPrefDeviceLocalAccountAutoLoginId,
                                 &auto_login_account_id);

  // Re-populates |apps_| and reuses existing KioskAppData when possible.
  const std::vector<policy::DeviceLocalAccount> device_local_accounts =
      policy::GetDeviceLocalAccounts(CrosSettings::Get());
  for (std::vector<policy::DeviceLocalAccount>::const_iterator
           it = device_local_accounts.begin();
       it != device_local_accounts.end(); ++it) {
    if (it->type != policy::DeviceLocalAccount::TYPE_KIOSK_APP)
      continue;

    if (it->account_id == auto_login_account_id)
      auto_launch_app_id_ = it->kiosk_app_id;

    // TODO(mnissler): Support non-CWS update URLs.

    std::map<std::string, KioskAppData*>::iterator old_it =
        old_apps.find(it->kiosk_app_id);
    if (old_it != old_apps.end()) {
      apps_.push_back(old_it->second);
      old_apps.erase(old_it);
    } else {
      KioskAppData* new_app =
          new KioskAppData(this, it->kiosk_app_id, it->user_id);
      apps_.push_back(new_app);  // Takes ownership of |new_app|.
      new_app->Load();
    }
  }

  // Clears cache and deletes the remaining old data.
  for (std::map<std::string, KioskAppData*>::iterator it = old_apps.begin();
       it != old_apps.end(); ++it) {
    it->second->ClearCache();
    cryptohome::AsyncMethodCaller::GetInstance()->AsyncRemove(
        it->second->user_id(),
        base::Bind(&OnRemoveAppCryptohomeComplete, it->first));
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

KioskAppManager::AutoLoginState KioskAppManager::GetAutoLoginState() const {
  PrefService* prefs = g_browser_process->local_state();
  const base::DictionaryValue* dict =
      prefs->GetDictionary(KioskAppManager::kKioskDictionaryName);
  int value;
  if (!dict->GetInteger(kKeyAutoLoginState, &value))
    return AUTOLOGIN_NONE;

  return static_cast<AutoLoginState>(value);
}

void KioskAppManager::SetAutoLoginState(AutoLoginState state) {
  PrefService* prefs = g_browser_process->local_state();
  DictionaryPrefUpdate dict_update(prefs,
                                   KioskAppManager::kKioskDictionaryName);
  dict_update->SetInteger(kKeyAutoLoginState, state);
  prefs->CommitPendingWrite();
}

}  // namespace chromeos
