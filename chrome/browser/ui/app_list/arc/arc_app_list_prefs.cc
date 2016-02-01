// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"

#include <stddef.h>

#include <string>

#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/task_runner_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs_factory.h"
#include "chrome/common/pref_names.h"
#include "components/crx_file/id_util.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "content/public/browser/browser_thread.h"

namespace {

const char kName[] = "name";
const char kPackageName[] = "package_name";
const char kActivity[] = "activity";

// Provider of write access to a dictionary storing ARC app prefs.
class ScopedArcAppListPrefUpdate : public DictionaryPrefUpdate {
 public:
  ScopedArcAppListPrefUpdate(PrefService* service, const std::string& id)
      : DictionaryPrefUpdate(service, prefs::kArcApps),
        id_(id) {}

  ~ScopedArcAppListPrefUpdate() override {}

  // DictionaryPrefUpdate overrides:
  base::DictionaryValue* Get() override {
    base::DictionaryValue* dict = DictionaryPrefUpdate::Get();
    base::DictionaryValue* app = nullptr;
    if (!dict->GetDictionary(id_, &app)) {
      app = new base::DictionaryValue();
      dict->SetWithoutPathExpansion(id_, app);
    }
    return app;
  }

 private:
  const std::string id_;

  DISALLOW_COPY_AND_ASSIGN(ScopedArcAppListPrefUpdate);
};

bool InstallIconFromFileThread(const std::string& app_id,
                               ui::ScaleFactor scale_factor,
                               const base::FilePath& icon_path,
                               const std::vector<uint8_t>& content_png) {
  DCHECK(content::BrowserThread::GetBlockingPool()->RunsTasksOnCurrentThread());
  DCHECK(!content_png.empty());

  base::CreateDirectory(icon_path.DirName());

  int wrote = base::WriteFile(icon_path,
                              reinterpret_cast<const char*>(&content_png[0]),
                              content_png.size());
  if (wrote != static_cast<int>(content_png.size())) {
    VLOG(2) << "Failed to write ARC icon file: " << icon_path.MaybeAsASCII()
            << ".";
    if (!base::DeleteFile(icon_path, false))
      VLOG(2) << "Couldn't delete broken icon file" << icon_path.MaybeAsASCII()
              << ".";
    return false;
  }

  return true;
}

void DeleteAppFolderFromFileThread(const base::FilePath& path) {
  DCHECK(content::BrowserThread::GetBlockingPool()->RunsTasksOnCurrentThread());
  DCHECK(path.DirName().BaseName().MaybeAsASCII() == prefs::kArcApps &&
         (!base::PathExists(path) || base::DirectoryExists(path)));
  const bool deleted = base::DeleteFile(path, true);
  DCHECK(deleted);
}

}  // namespace

// static
ArcAppListPrefs* ArcAppListPrefs::Create(const base::FilePath& base_path,
                                         PrefService* prefs) {
  return new ArcAppListPrefs(base_path, prefs);
}

// static
void ArcAppListPrefs::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(prefs::kArcApps);
}

// static
ArcAppListPrefs* ArcAppListPrefs::Get(content::BrowserContext* context) {
  return ArcAppListPrefsFactory::GetInstance()->GetForBrowserContext(context);
}

// static
std::string ArcAppListPrefs::GetAppId(const std::string& package_name,
                                      const std::string& activity) {
  std::string input = package_name + "#" + activity;
  return crx_file::id_util::GenerateId(input);
}

ArcAppListPrefs::ArcAppListPrefs(const base::FilePath& base_path,
                                 PrefService* prefs)
    : prefs_(prefs), bridge_service_(arc::ArcBridgeService::Get()),
      binding_(this), weak_ptr_factory_(this) {
  base_path_ = base_path.AppendASCII(prefs::kArcApps);

  if (!bridge_service_) {
    NOTREACHED();
    return;
  }

  bridge_service_->AddObserver(this);
  OnStateChanged(bridge_service_->state());
}

ArcAppListPrefs::~ArcAppListPrefs() {
  if (!bridge_service_) {
    NOTREACHED();
    return;
  }

  bridge_service_->RemoveObserver(this);
}

base::FilePath ArcAppListPrefs::GetAppPath(const std::string& app_id) const {
  return base_path_.AppendASCII(app_id);
}

base::FilePath ArcAppListPrefs::GetIconPath(
    const std::string& app_id,
    ui::ScaleFactor scale_factor) const {
  const base::FilePath app_path = GetAppPath(app_id);
  switch (scale_factor) {
  case ui::SCALE_FACTOR_100P:
    return app_path.AppendASCII("icon_100p.png");
  case ui::SCALE_FACTOR_125P:
    return app_path.AppendASCII("icon_125p.png");
  case ui::SCALE_FACTOR_133P:
    return app_path.AppendASCII("icon_133p.png");
  case ui::SCALE_FACTOR_140P:
    return app_path.AppendASCII("icon_140p.png");
  case ui::SCALE_FACTOR_150P:
    return app_path.AppendASCII("icon_150p.png");
  case ui::SCALE_FACTOR_180P:
    return app_path.AppendASCII("icon_180p.png");
  case ui::SCALE_FACTOR_200P:
    return app_path.AppendASCII("icon_200p.png");
  case ui::SCALE_FACTOR_250P:
    return app_path.AppendASCII("icon_250p.png");
  case ui::SCALE_FACTOR_300P:
    return app_path.AppendASCII("icon_300p.png");
  default:
    NOTREACHED();
    return base::FilePath();
  }
}

void ArcAppListPrefs::RequestIcon(const std::string& app_id,
                                  ui::ScaleFactor scale_factor) {
  if (!IsRegistered(app_id)) {
    VLOG(2) << "Request to load icon for non-registered app: "
            <<  app_id << ".";
    return;
  }

  // In case app is not ready, defer this request.
  if (!ready_apps_.count(app_id)) {
    request_icon_deferred_[app_id] =
        request_icon_deferred_[app_id] | 1 << scale_factor;
    return;
  }

  if (!bridge_service_) {
    NOTREACHED();
    return;
  }
  arc::AppInstance* app_instance = bridge_service_->app_instance();
  if (!app_instance) {
    VLOG(2) << "Request to load icon when bridge service is not ready: "
            <<  app_id << ".";
    return;
  }

  scoped_ptr<AppInfo> app_info = GetApp(app_id);
  if (!app_info) {
    VLOG(2) << "Failed to get app info: " <<  app_id << ".";
    return;
  }

  app_instance->RequestAppIcon(app_info->package_name, app_info->activity,
                               static_cast<arc::ScaleFactor>(scale_factor));
}

void ArcAppListPrefs::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void ArcAppListPrefs::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

std::vector<std::string> ArcAppListPrefs::GetAppIds() const {
  std::vector<std::string> ids;

  // crx_file::id_util is de-facto utility for id generation.
  const base::DictionaryValue* apps = prefs_->GetDictionary(prefs::kArcApps);
  for (base::DictionaryValue::Iterator app_id(*apps);
       !app_id.IsAtEnd(); app_id.Advance()) {
    if (!crx_file::id_util::IdIsValid(app_id.key()))
      continue;

    ids.push_back(app_id.key());
  }

  return ids;
}

scoped_ptr<ArcAppListPrefs::AppInfo> ArcAppListPrefs::GetApp(
    const std::string& app_id) const {
  const base::DictionaryValue* app = nullptr;
  const base::DictionaryValue* apps = prefs_->GetDictionary(prefs::kArcApps);
  if (!apps || !apps->GetDictionaryWithoutPathExpansion(app_id, &app))
    return scoped_ptr<AppInfo>();

  std::string name;
  std::string package_name;
  std::string activity;
  app->GetString(kName, &name);
  app->GetString(kPackageName, &package_name);
  app->GetString(kActivity, &activity);
  scoped_ptr<AppInfo> app_info(
      new AppInfo(name, package_name, activity, ready_apps_.count(app_id) > 0));

  return app_info;
}

bool ArcAppListPrefs::IsRegistered(const std::string& app_id) const {
  const base::DictionaryValue* app = nullptr;
  const base::DictionaryValue* apps = prefs_->GetDictionary(prefs::kArcApps);
  if (!apps || !apps->GetDictionary(app_id, &app))
    return false;

  return true;
}

void ArcAppListPrefs::DisableAllApps() {
  for (auto& app_id : ready_apps_)
    FOR_EACH_OBSERVER(Observer,
                      observer_list_,
                      OnAppReadyChanged(app_id, false));

  ready_apps_.clear();
}

void ArcAppListPrefs::OnStateChanged(arc::ArcBridgeService::State state) {
  if (state != arc::ArcBridgeService::State::READY)
    DisableAllApps();
}

void ArcAppListPrefs::OnAppInstanceReady() {
  if (!bridge_service_) {
    NOTREACHED();
    return;
  }
  arc::AppInstance* app_instance = bridge_service_->app_instance();
  if (!app_instance) {
    VLOG(2) << "Request to refresh app list when bridge service is not ready.";
    return;
  }

  app_instance->Init(binding_.CreateInterfacePtrAndBind());
  app_instance->RefreshAppList();
}

void ArcAppListPrefs::AddApp(const arc::AppInfo& app) {
  if (app.name.get().empty() || app.package_name.get().empty() ||
      app.activity.get().empty()) {
    VLOG(2) << "Name, package name, and activity cannot be empty.";
    return;
  }
  std::string app_id = GetAppId(app.package_name, app.activity);
  bool was_registered = IsRegistered(app_id);

  if (was_registered) {
    scoped_ptr<ArcAppListPrefs::AppInfo> app_old_info = GetApp(app_id);
    if (app.name != app_old_info->name) {
      FOR_EACH_OBSERVER(Observer, observer_list_,
                        OnAppNameUpdated(app_id, app.name));
    }
  }

  ScopedArcAppListPrefUpdate update(prefs_, app_id);
  base::DictionaryValue* app_dict = update.Get();
  app_dict->SetString(kName, app.name);
  app_dict->SetString(kPackageName, app.package_name);
  app_dict->SetString(kActivity, app.activity);

  // From now, app is available.
  if (!ready_apps_.count(app_id))
    ready_apps_.insert(app_id);

  if (was_registered) {
    FOR_EACH_OBSERVER(Observer,
                      observer_list_,
                      OnAppReadyChanged(app_id, true));
  } else {
    AppInfo app_info(app.name, app.package_name, app.activity, true);
    FOR_EACH_OBSERVER(Observer,
                      observer_list_,
                      OnAppRegistered(app_id, app_info));
  }

  std::map<std::string, uint32_t>::iterator deferred_icons =
      request_icon_deferred_.find(app_id);
  if (deferred_icons != request_icon_deferred_.end()) {
    for (uint32_t i = ui::SCALE_FACTOR_100P; i < ui::NUM_SCALE_FACTORS; ++i) {
      if (deferred_icons->second & (1 << i)) {
         RequestIcon(app_id, static_cast<ui::ScaleFactor>(i));
      }
    }
    request_icon_deferred_.erase(deferred_icons);
  }
}

void ArcAppListPrefs::RemoveApp(const std::string& app_id) {
  // From now, app is not available.
  ready_apps_.erase(app_id);
  FOR_EACH_OBSERVER(Observer,
                    observer_list_,
                    OnAppRemoved(app_id));

  // Remove from prefs.
  DictionaryPrefUpdate update(prefs_, prefs::kArcApps);
  base::DictionaryValue* apps = update.Get();
  const bool removed = apps->Remove(app_id, nullptr);
  DCHECK(removed);

  // Remove local data on file system.
  const base::FilePath app_path = GetAppPath(app_id);
  content::BrowserThread::GetBlockingPool()->PostTask(
      FROM_HERE,
      base::Bind(&DeleteAppFolderFromFileThread, app_path));
}

void ArcAppListPrefs::OnAppListRefreshed(mojo::Array<arc::AppInfoPtr> apps) {
  std::vector<std::string> old_apps = GetAppIds();

  ready_apps_.clear();
  for (size_t i = 0; i < apps.size(); ++i)
    AddApp(*apps[i]);

  // Detect removed ARC apps after current refresh.
  for (const auto& app_id : old_apps) {
    if (!ready_apps_.count(app_id))
      RemoveApp(app_id);
  }
}

void ArcAppListPrefs::OnAppAdded(arc::AppInfoPtr app) {
  AddApp(*app);
}

void ArcAppListPrefs::OnPackageRemoved(const mojo::String& package_name) {
  const base::DictionaryValue* apps = prefs_->GetDictionary(prefs::kArcApps);
  std::vector<std::string> apps_to_remove;
  for (base::DictionaryValue::Iterator app_it(*apps);
       !app_it.IsAtEnd(); app_it.Advance()) {
    const base::Value* value = &app_it.value();
    const base::DictionaryValue* app;
    if (!value->GetAsDictionary(&app)) {
      NOTREACHED();
      continue;
    }

    std::string app_package;
    if (!app->GetString(kPackageName, &app_package)) {
      NOTREACHED();
      continue;
    }

    if (package_name != app_package)
      continue;

    apps_to_remove.push_back(app_it.key());
  }

  for (auto& app_id : apps_to_remove)
    RemoveApp(app_id);
}

void ArcAppListPrefs::OnAppIcon(const mojo::String& package_name,
                                const mojo::String& activity,
                                arc::ScaleFactor scale_factor,
                                mojo::Array<uint8_t> icon_png_data) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_NE(0u, icon_png_data.size());

  std::string app_id = GetAppId(package_name, activity);
  if (!IsRegistered(app_id)) {
    VLOG(2) << "Request to update icon for non-registered app: " << app_id;
    return;
  }

  InstallIcon(app_id, static_cast<ui::ScaleFactor>(scale_factor),
              icon_png_data.To<std::vector<uint8_t>>());
}

void ArcAppListPrefs::InstallIcon(const std::string& app_id,
                                  ui::ScaleFactor scale_factor,
                                  const std::vector<uint8_t>& content_png) {
  base::FilePath icon_path = GetIconPath(app_id, scale_factor);
  base::PostTaskAndReplyWithResult(content::BrowserThread::GetBlockingPool(),
                                   FROM_HERE,
                                   base::Bind(&InstallIconFromFileThread,
                                              app_id,
                                              scale_factor,
                                              icon_path,
                                              content_png),
                                   base::Bind(&ArcAppListPrefs::OnIconInstalled,
                                              weak_ptr_factory_.GetWeakPtr(),
                                              app_id,
                                              scale_factor));
}

void ArcAppListPrefs::OnIconInstalled(const std::string& app_id,
                                      ui::ScaleFactor scale_factor,
                                      bool install_succeed) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!install_succeed)
    return;

  FOR_EACH_OBSERVER(Observer,
                    observer_list_,
                    OnAppIconUpdated(app_id, scale_factor));
}

ArcAppListPrefs::AppInfo::AppInfo(const std::string& name,
                                  const std::string& package_name,
                                  const std::string& activity,
                                  bool ready)
    : name(name),
      package_name(package_name),
      activity(activity),
      ready(ready) {}
