// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/kiosk_app_manager.h"

#include <map>

#include "base/path_service.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/stl_util.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_data.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager_observer.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_prefs_local_state.h"
#include "chrome/common/chrome_paths.h"

namespace chromeos {

namespace {

void CreateDirectory(const base::FilePath& dir) {
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
  return prefs_->GetAutoLaunchApp();
}

void KioskAppManager::SetAutoLaunchApp(const std::string& app_id) {
  prefs_->SetAutoLaunchApp(app_id);
}

bool KioskAppManager::GetSuppressAutoLaunch() const {
  return prefs_->GetSuppressAutoLaunch();
}

void KioskAppManager::SetSuppressAutoLaunch(bool suppress) {
  prefs_->SetSuppressAutoLaunch(suppress);
}

void KioskAppManager::AddApp(const std::string& app_id) {
  prefs_->AddApp(app_id);
}

void KioskAppManager::RemoveApp(const std::string& app_id) {
  prefs_->RemoveApp(app_id);
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

void KioskAppManager::AddObserver(KioskAppManagerObserver* observer) {
  observers_.AddObserver(observer);
}

void KioskAppManager::RemoveObserver(KioskAppManagerObserver* observer) {
  observers_.RemoveObserver(observer);
}

KioskAppManager::KioskAppManager()
    : prefs_(new KioskAppPrefsLocalState) {
  UpdateAppData();
  prefs_->AddObserver(this);
}

KioskAppManager::~KioskAppManager() {
  prefs_->RemoveObserver(this);
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

  // Re-populates |apps_| and reuses existing KioskAppData when possible.
  KioskAppPrefs::AppIds app_ids;
  prefs_->GetApps(&app_ids);
  for (size_t i = 0; i < app_ids.size(); ++i) {
    const std::string& id = app_ids[i];
    std::map<std::string, KioskAppData*>::iterator it = old_apps.find(id);
    if (it != old_apps.end()) {
      apps_.push_back(it->second);
      old_apps.erase(it);
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
  }
  STLDeleteValues(&old_apps);
}

void KioskAppManager::OnKioskAutoLaunchAppChanged()  {
  FOR_EACH_OBSERVER(KioskAppManagerObserver,
                    observers_,
                    OnKioskAutoLaunchAppChanged());
}

void KioskAppManager::OnKioskAppsChanged() {
  UpdateAppData();

  FOR_EACH_OBSERVER(KioskAppManagerObserver,
                    observers_,
                    OnKioskAppsChanged());
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
