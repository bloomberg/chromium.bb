// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/kiosk_app_manager.h"

#include <map>

#include "base/bind.h"
#include "base/path_service.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_data.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager_observer.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_paths.h"
#include "chromeos/cryptohome/async_method_caller.h"
#include "content/public/browser/notification_details.h"

namespace chromeos {

namespace {

void OnRemoveAppCryptohomeComplete(const std::string& app,
                                   bool success,
                                   cryptohome::MountError return_code) {
  if (!success) {
    LOG(ERROR) << "Remove cryptohome for " << app
        << " failed, return code: " << return_code;
  }
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
  std::string app_id;
  if (CrosSettings::Get()->GetString(kKioskAutoLaunch, &app_id))
    return app_id;

  return std::string();
}

void KioskAppManager::SetAutoLaunchApp(const std::string& app_id) {
  CrosSettings::Get()->SetString(kKioskAutoLaunch, app_id);
}

void KioskAppManager::AddApp(const std::string& app_id) {
  base::StringValue value(app_id);
  CrosSettings::Get()->AppendToList(kKioskApps, &value);
}

void KioskAppManager::RemoveApp(const std::string& app_id) {
  base::StringValue value(app_id);
  CrosSettings::Get()->RemoveFromList(kKioskApps, &value);
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
  bool disable;
  if (CrosSettings::Get()->GetBoolean(kKioskDisableBailoutShortcut, &disable))
    return disable;

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
  CrosSettings::Get()->AddSettingsObserver(kKioskApps, this);
}

KioskAppManager::~KioskAppManager() {}

void KioskAppManager::CleanUp() {
  CrosSettings::Get()->RemoveSettingsObserver(kKioskApps, this);
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

  const base::ListValue* new_apps;
  CHECK(CrosSettings::Get()->GetList(kKioskApps, &new_apps));

  // Re-populates |apps_| and reuses existing KioskAppData when possible.
  for (base::ListValue::const_iterator new_it = new_apps->begin();
       new_it != new_apps->end();
       ++new_it) {
    std::string id;
    CHECK((*new_it)->GetAsString(&id));

    std::map<std::string, KioskAppData*>::iterator old_it = old_apps.find(id);
    if (old_it != old_apps.end()) {
      apps_.push_back(old_it->second);
      old_apps.erase(old_it);
    } else {
      KioskAppData* new_app = new KioskAppData(this, id);
      apps_.push_back(new_app);  // Takes ownership of |new_app|.
      new_app->Load();
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
}

void KioskAppManager::Observe(int type,
                              const content::NotificationSource& source,
                              const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_SYSTEM_SETTING_CHANGED, type);
  DCHECK_EQ(kKioskApps,
            *content::Details<const std::string>(details).ptr());

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
