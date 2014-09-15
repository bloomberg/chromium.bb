// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/kiosk_app_manager.h"

#include <map>
#include <set>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/stl_util.h"
#include "base/sys_info.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_data.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_external_loader.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager_observer.h"
#include "chrome/browser/chromeos/app_mode/kiosk_external_updater.h"
#include "chrome/browser/chromeos/ownership/owner_settings_service_chromeos_factory.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/device_local_account.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/extensions/external_loader.h"
#include "chrome/browser/extensions/external_provider_impl.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chromeos/chromeos_paths.h"
#include "chromeos/cryptohome/async_method_caller.h"
#include "chromeos/settings/cros_settings_names.h"
#include "components/ownership/owner_key_util.h"
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
  scoped_refptr<ownership::OwnerKeyUtil> util =
      OwnerSettingsServiceChromeOSFactory::GetInstance()->GetOwnerKeyUtil();
  *present = util.get() && util->IsPublicKeyPresent();
}

scoped_refptr<base::SequencedTaskRunner> GetBackgroundTaskRunner() {
  base::SequencedWorkerPool* pool = content::BrowserThread::GetBlockingPool();
  CHECK(pool);
  return pool->GetSequencedTaskRunnerWithShutdownBehavior(
      pool->GetSequenceToken(), base::SequencedWorkerPool::SKIP_ON_SHUTDOWN);
}

}  // namespace

// static
const char KioskAppManager::kKioskDictionaryName[] = "kiosk";
const char KioskAppManager::kKeyApps[] = "apps";
const char KioskAppManager::kKeyAutoLoginState[] = "auto_login_state";
const char KioskAppManager::kIconCacheDir[] = "kiosk/icon";
const char KioskAppManager::kCrxCacheDir[] = "kiosk/crx";
const char KioskAppManager::kCrxUnpackDir[] = "kiosk_unpack";

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

KioskAppManager::App::App(const KioskAppData& data, bool is_extension_pending)
    : app_id(data.app_id()),
      user_id(data.user_id()),
      name(data.name()),
      icon(data.icon()),
      is_loading(data.IsLoading() || is_extension_pending) {
}

KioskAppManager::App::App() : is_loading(false) {}
KioskAppManager::App::~App() {}

std::string KioskAppManager::GetAutoLaunchApp() const {
  return auto_launch_app_id_;
}

void KioskAppManager::SetAutoLaunchApp(const std::string& app_id) {
  SetAutoLoginState(AUTOLOGIN_REQUESTED);
  // Clean first, so the proper change callbacks are triggered even
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

void KioskAppManager::EnableConsumerKioskAutoLaunch(
    const KioskAppManager::EnableKioskAutoLaunchCallback& callback) {
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  connector->GetInstallAttributes()->LockDevice(
      std::string(),  // user
      policy::DEVICE_MODE_CONSUMER_KIOSK_AUTOLAUNCH,
      std::string(),  // device_id
      base::Bind(
          &KioskAppManager::OnLockDevice, base::Unretained(this), callback));
}

void KioskAppManager::GetConsumerKioskAutoLaunchStatus(
    const KioskAppManager::GetConsumerKioskAutoLaunchStatusCallback& callback) {
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  connector->GetInstallAttributes()->ReadImmutableAttributes(
      base::Bind(&KioskAppManager::OnReadImmutableAttributes,
                 base::Unretained(this),
                 callback));
}

bool KioskAppManager::IsConsumerKioskDeviceWithAutoLaunch() {
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  return connector->GetInstallAttributes() &&
         connector->GetInstallAttributes()
             ->IsConsumerKioskDeviceWithAutoLaunch();
}

void KioskAppManager::OnLockDevice(
    const KioskAppManager::EnableKioskAutoLaunchCallback& callback,
    policy::EnterpriseInstallAttributes::LockResult result) {
  if (callback.is_null())
    return;

  callback.Run(result == policy::EnterpriseInstallAttributes::LOCK_SUCCESS);
}

void KioskAppManager::OnOwnerFileChecked(
    const KioskAppManager::GetConsumerKioskAutoLaunchStatusCallback& callback,
    bool* owner_present) {
  ownership_established_ = *owner_present;

  if (callback.is_null())
    return;

  // If we have owner already established on the machine, don't let
  // consumer kiosk to be enabled.
  if (ownership_established_)
    callback.Run(CONSUMER_KIOSK_AUTO_LAUNCH_DISABLED);
  else
    callback.Run(CONSUMER_KIOSK_AUTO_LAUNCH_CONFIGURABLE);
}

void KioskAppManager::OnReadImmutableAttributes(
    const KioskAppManager::GetConsumerKioskAutoLaunchStatusCallback&
        callback) {
  if (callback.is_null())
    return;

  ConsumerKioskAutoLaunchStatus status =
      CONSUMER_KIOSK_AUTO_LAUNCH_DISABLED;
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  policy::EnterpriseInstallAttributes* attributes =
      connector->GetInstallAttributes();
  switch (attributes->GetMode()) {
    case policy::DEVICE_MODE_NOT_SET: {
      if (!base::SysInfo::IsRunningOnChromeOS()) {
        status = CONSUMER_KIOSK_AUTO_LAUNCH_CONFIGURABLE;
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
    case policy::DEVICE_MODE_CONSUMER_KIOSK_AUTOLAUNCH:
      status = CONSUMER_KIOSK_AUTO_LAUNCH_ENABLED;
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
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  if (connector->IsEnterpriseManaged())
    return false;

  return GetAutoLoginState() == AUTOLOGIN_REQUESTED;
}

bool KioskAppManager::IsAutoLaunchEnabled() const {
  if (GetAutoLaunchApp().empty())
    return false;

  // Apps that were installed by the policy don't require machine owner
  // consent through UI.
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  if (connector->IsEnterpriseManaged())
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
  apps->clear();
  apps->reserve(apps_.size());
  for (size_t i = 0; i < apps_.size(); ++i) {
    const KioskAppData& app_data = *apps_[i];
    if (app_data.status() != KioskAppData::STATUS_ERROR)
      apps->push_back(App(
          app_data, external_cache_->IsExtensionPending(app_data.app_id())));
  }
}

bool KioskAppManager::GetApp(const std::string& app_id, App* app) const {
  const KioskAppData* data = GetAppData(app_id);
  if (!data)
    return false;

  *app = App(*data, external_cache_->IsExtensionPending(app_id));
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

void KioskAppManager::ClearAppData(const std::string& app_id) {
  KioskAppData* app_data = GetAppDataMutable(app_id);
  if (!app_data)
    return;

  app_data->ClearCache();
}

void KioskAppManager::UpdateAppDataFromProfile(
    const std::string& app_id,
    Profile* profile,
    const extensions::Extension* app) {
  KioskAppData* app_data = GetAppDataMutable(app_id);
  if (!app_data)
    return;

  app_data->LoadFromInstalledApp(profile, app);
}

void KioskAppManager::RetryFailedAppDataFetch() {
  for (size_t i = 0; i < apps_.size(); ++i) {
    if (apps_[i]->status() == KioskAppData::STATUS_ERROR)
      apps_[i]->Load();
  }
}

bool KioskAppManager::HasCachedCrx(const std::string& app_id) const {
  base::FilePath crx_path;
  std::string version;
  return GetCachedCrx(app_id, &crx_path, &version);
}

bool KioskAppManager::GetCachedCrx(const std::string& app_id,
                                   base::FilePath* file_path,
                                   std::string* version) const {
  return external_cache_->GetExtension(app_id, file_path, version);
}

void KioskAppManager::AddObserver(KioskAppManagerObserver* observer) {
  observers_.AddObserver(observer);
}

void KioskAppManager::RemoveObserver(KioskAppManagerObserver* observer) {
  observers_.RemoveObserver(observer);
}

extensions::ExternalLoader* KioskAppManager::CreateExternalLoader() {
  if (external_loader_created_) {
    NOTREACHED();
    return NULL;
  }
  external_loader_created_ = true;
  KioskAppExternalLoader* loader = new KioskAppExternalLoader();
  external_loader_ = loader->AsWeakPtr();

  return loader;
}

void KioskAppManager::InstallFromCache(const std::string& id) {
  const base::DictionaryValue* extension = NULL;
  if (external_cache_->cached_extensions()->GetDictionary(id, &extension)) {
    scoped_ptr<base::DictionaryValue> prefs(new base::DictionaryValue);
    base::DictionaryValue* extension_copy = extension->DeepCopy();
    prefs->Set(id, extension_copy);
    external_loader_->SetCurrentAppExtensions(prefs.Pass());
  } else {
    LOG(ERROR) << "Can't find app in the cached externsions"
               << " id = " << id;
  }
}

void KioskAppManager::UpdateExternalCache() {
  UpdateAppData();
}

void KioskAppManager::OnKioskAppCacheUpdated(const std::string& app_id) {
  FOR_EACH_OBSERVER(
      KioskAppManagerObserver, observers_, OnKioskAppCacheUpdated(app_id));
}

void KioskAppManager::OnKioskAppExternalUpdateComplete(bool success) {
  FOR_EACH_OBSERVER(KioskAppManagerObserver,
                    observers_,
                    OnKioskAppExternalUpdateComplete(success));
}

void KioskAppManager::PutValidatedExternalExtension(
    const std::string& app_id,
    const base::FilePath& crx_path,
    const std::string& version,
    const ExternalCache::PutExternalExtensionCallback& callback) {
  external_cache_->PutExternalExtension(app_id, crx_path, version, callback);
}

KioskAppManager::KioskAppManager()
    : ownership_established_(false), external_loader_created_(false) {
  base::FilePath cache_dir;
  GetCrxCacheDir(&cache_dir);
  external_cache_.reset(
      new ExternalCache(cache_dir,
                        g_browser_process->system_request_context(),
                        GetBackgroundTaskRunner(),
                        this,
                        true /* always_check_updates */,
                        false /* wait_for_cache_initialization */));
  UpdateAppData();
  local_accounts_subscription_ =
      CrosSettings::Get()->AddSettingsObserver(
          kAccountsPrefDeviceLocalAccounts,
          base::Bind(&KioskAppManager::UpdateAppData, base::Unretained(this)));
  local_account_auto_login_id_subscription_ =
      CrosSettings::Get()->AddSettingsObserver(
          kAccountsPrefDeviceLocalAccountAutoLoginId,
          base::Bind(&KioskAppManager::UpdateAppData, base::Unretained(this)));
}

KioskAppManager::~KioskAppManager() {}

void KioskAppManager::MonitorKioskExternalUpdate() {
  base::FilePath cache_dir;
  GetCrxCacheDir(&cache_dir);
  base::FilePath unpack_dir;
  GetCrxUnpackDir(&unpack_dir);
  usb_stick_updater_.reset(new KioskExternalUpdater(
      GetBackgroundTaskRunner(), cache_dir, unpack_dir));
}

void KioskAppManager::CleanUp() {
  local_accounts_subscription_.reset();
  local_account_auto_login_id_subscription_.reset();
  apps_.clear();
  usb_stick_updater_.reset();
  external_cache_.reset();
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

KioskAppData* KioskAppManager::GetAppDataMutable(const std::string& app_id) {
  return const_cast<KioskAppData*>(GetAppData(app_id));
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
  std::vector<std::string> apps_to_remove;
  for (std::map<std::string, KioskAppData*>::iterator it = old_apps.begin();
       it != old_apps.end(); ++it) {
    it->second->ClearCache();
    cryptohome::AsyncMethodCaller::GetInstance()->AsyncRemove(
        it->second->user_id(),
        base::Bind(&OnRemoveAppCryptohomeComplete, it->first));
    apps_to_remove.push_back(it->second->app_id());
  }
  STLDeleteValues(&old_apps);
  external_cache_->RemoveExtensions(apps_to_remove);

  // Request external_cache_ to download new apps and update the existing
  // apps.
  scoped_ptr<base::DictionaryValue> prefs(new base::DictionaryValue);
  for (size_t i = 0; i < apps_.size(); ++i)
    prefs->Set(apps_[i]->app_id(), new base::DictionaryValue);
  external_cache_->UpdateExtensionsList(prefs.Pass());

  RetryFailedAppDataFetch();

  FOR_EACH_OBSERVER(KioskAppManagerObserver, observers_,
                    OnKioskAppsSettingsChanged());
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
}

void KioskAppManager::OnExtensionListsUpdated(
    const base::DictionaryValue* prefs) {
}

void KioskAppManager::OnExtensionLoadedInCache(const std::string& id) {
  KioskAppData* app_data = GetAppDataMutable(id);
  if (!app_data)
    return;
  FOR_EACH_OBSERVER(KioskAppManagerObserver,
                    observers_,
                    OnKioskExtensionLoadedInCache(id));

}

void KioskAppManager::OnExtensionDownloadFailed(
    const std::string& id,
    extensions::ExtensionDownloaderDelegate::Error error) {
  KioskAppData* app_data = GetAppDataMutable(id);
  if (!app_data)
    return;
  FOR_EACH_OBSERVER(KioskAppManagerObserver,
                    observers_,
                    OnKioskExtensionDownloadFailed(id));
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

void KioskAppManager::GetCrxCacheDir(base::FilePath* cache_dir) {
  base::FilePath user_data_dir;
  CHECK(PathService::Get(chrome::DIR_USER_DATA, &user_data_dir));
  *cache_dir = user_data_dir.AppendASCII(kCrxCacheDir);
}

void KioskAppManager::GetCrxUnpackDir(base::FilePath* unpack_dir) {
  base::FilePath temp_dir;
  base::GetTempDir(&temp_dir);
  *unpack_dir = temp_dir.AppendASCII(kCrxUnpackDir);
}

}  // namespace chromeos
