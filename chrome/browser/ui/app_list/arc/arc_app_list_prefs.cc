// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"

#include <stddef.h>

#include <string>
#include <unordered_map>

#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/task_runner_util.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs_factory.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/common/pref_names.h"
#include "components/crx_file/id_util.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/browser_thread.h"

namespace {

const char kName[] = "name";
const char kPackageName[] = "package_name";
const char kPackageVersion[] = "package_version";
const char kActivity[] = "activity";
const char kSticky[] = "sticky";
const char kNotificationsEnabled[] = "notifications_enabled";
const char kLastBackupAndroidId[] = "last_backup_android_id";
const char kLastBackupTime[] = "last_backup_time";
const char kLastLaunchTime[] = "lastlaunchtime";
const char kShouldSync[] = "should_sync";

// Provider of write access to a dictionary storing ARC prefs.
class ScopedArcPrefUpdate : public DictionaryPrefUpdate {
 public:
  ScopedArcPrefUpdate(PrefService* service,
                      const std::string& id,
                      const std::string& path)
      : DictionaryPrefUpdate(service, path), id_(id) {}

  ~ScopedArcPrefUpdate() override {}

  // DictionaryPrefUpdate overrides:
  base::DictionaryValue* Get() override {
    base::DictionaryValue* dict = DictionaryPrefUpdate::Get();
    base::DictionaryValue* dict_item = nullptr;
    if (!dict->GetDictionary(id_, &dict_item)) {
      dict_item = new base::DictionaryValue();
      dict->SetWithoutPathExpansion(id_, dict_item);
    }
    return dict_item;
  }

 private:
  const std::string id_;

  DISALLOW_COPY_AND_ASSIGN(ScopedArcPrefUpdate);
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

bool IsArcEnabled() {
  arc::ArcAuthService* auth_service = arc::ArcAuthService::Get();
  return auth_service &&
         auth_service->state() != arc::ArcAuthService::State::NOT_INITIALIZED &&
         auth_service->IsArcEnabled();
}

bool GetInt64FromPref(const base::DictionaryValue* dict,
                      const std::string& key,
                      int64_t* value) {
  DCHECK(dict);
  std::string value_str;
  if (!dict->GetStringWithoutPathExpansion(key, &value_str)) {
    VLOG(2) << "Can't find key in local pref dictionary. Invalid key: " << key
            << ".";
    return false;
  }

  if (!base::StringToInt64(value_str, value)) {
    VLOG(2) << "Can't change string to int64_t. Invalid string value: "
            << value_str << ".";
    return false;
  }

  return true;
}

// Add or update local pref for given package.
void AddOrUpdatePackagePrefs(PrefService* prefs,
                             const arc::mojom::ArcPackageInfo& package) {
  DCHECK(IsArcEnabled());
  const std::string& package_name = package.package_name;
  if (package_name.empty()) {
    VLOG(2) << "Package name cannot be empty.";
    return;
  }
  ScopedArcPrefUpdate update(prefs, package_name, prefs::kArcPackages);
  base::DictionaryValue* package_dict = update.Get();
  const std::string id_str =
      base::Int64ToString(package.last_backup_android_id);
  const std::string time_str = base::Int64ToString(package.last_backup_time);

  package_dict->SetBoolean(kShouldSync, package.sync);
  package_dict->SetInteger(kPackageVersion, package.package_version);
  package_dict->SetString(kLastBackupAndroidId, id_str);
  package_dict->SetString(kLastBackupTime, time_str);
}

// Remove given package from local pref.
void RemovePackageFromPrefs(PrefService* prefs,
                            const std::string& package_name) {
  DCHECK(IsArcEnabled());
  DictionaryPrefUpdate update(prefs, prefs::kArcPackages);
  base::DictionaryValue* packages = update.Get();
  const bool removed = packages->Remove(package_name, nullptr);
  DCHECK(removed);
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
  registry->RegisterDictionaryPref(prefs::kArcPackages);
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
    : prefs_(prefs), binding_(this), weak_ptr_factory_(this) {
  base_path_ = base_path.AppendASCII(prefs::kArcApps);

  arc::ArcAuthService* auth_service = arc::ArcAuthService::Get();
  if (!auth_service)
    return;

  if (auth_service->state() != arc::ArcAuthService::State::NOT_INITIALIZED)
    OnOptInEnabled(auth_service->IsArcEnabled());
  auth_service->AddObserver(this);

  arc::ArcBridgeService* bridge_service = arc::ArcBridgeService::Get();
  DCHECK(bridge_service);

  bridge_service->AddObserver(this);
  OnStateChanged(bridge_service->state());
}

ArcAppListPrefs::~ArcAppListPrefs() {
  arc::ArcBridgeService* bridge_service = arc::ArcBridgeService::Get();
  if (bridge_service)
    bridge_service->RemoveObserver(this);

  arc::ArcAuthService* auth_service = arc::ArcAuthService::Get();
  if (auth_service)
    auth_service->RemoveObserver(this);
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

  arc::ArcBridgeService* bridge_service = arc::ArcBridgeService::Get();
  if (!bridge_service) {
    NOTREACHED();
    return;
  }
  arc::mojom::AppInstance* app_instance = bridge_service->app_instance();
  if (!app_instance) {
    VLOG(2) << "Request to load icon when bridge service is not ready: "
            <<  app_id << ".";
    return;
  }

  std::unique_ptr<AppInfo> app_info = GetApp(app_id);
  if (!app_info) {
    VLOG(2) << "Failed to get app info: " <<  app_id << ".";
    return;
  }

  app_instance->RequestAppIcon(
      app_info->package_name, app_info->activity,
      static_cast<arc::mojom::ScaleFactor>(scale_factor));
}

void ArcAppListPrefs::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void ArcAppListPrefs::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

bool ArcAppListPrefs::HasObserver(Observer* observer) {
  return observer_list_.HasObserver(observer);
}

std::unique_ptr<ArcAppListPrefs::PackageInfo> ArcAppListPrefs::GetPackage(
    const std::string& package_name) const {
  if (!IsArcEnabled())
    return nullptr;

  const base::DictionaryValue* package = nullptr;
  const base::DictionaryValue* packages =
      prefs_->GetDictionary(prefs::kArcPackages);
  if (!packages ||
      !packages->GetDictionaryWithoutPathExpansion(package_name, &package))
    return std::unique_ptr<PackageInfo>();

  int32_t package_version = 0;
  int64_t last_backup_android_id = 0;
  int64_t last_backup_time = 0;
  bool should_sync = false;

  GetInt64FromPref(package, kLastBackupAndroidId, &last_backup_android_id);
  GetInt64FromPref(package, kLastBackupTime, &last_backup_time);
  package->GetInteger(kPackageVersion, &package_version);
  package->GetBoolean(kShouldSync, &should_sync);

  return base::MakeUnique<PackageInfo>(package_name, package_version,
                                       last_backup_android_id, last_backup_time,
                                       should_sync);
}

std::vector<std::string> ArcAppListPrefs::GetAppIds() const {
  if (!IsArcEnabled())
    return std::vector<std::string>();
  return GetAppIdsNoArcEnabledCheck();
}

std::vector<std::string> ArcAppListPrefs::GetAppIdsNoArcEnabledCheck() const {
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

std::unique_ptr<ArcAppListPrefs::AppInfo> ArcAppListPrefs::GetApp(
    const std::string& app_id) const {
  if (!IsArcEnabled())
    return std::unique_ptr<AppInfo>();

  const base::DictionaryValue* app = nullptr;
  const base::DictionaryValue* apps = prefs_->GetDictionary(prefs::kArcApps);
  if (!apps || !apps->GetDictionaryWithoutPathExpansion(app_id, &app))
    return std::unique_ptr<AppInfo>();

  std::string name;
  std::string package_name;
  std::string activity;
  bool sticky = false;
  bool notifications_enabled = true;
  app->GetString(kName, &name);
  app->GetString(kPackageName, &package_name);
  app->GetString(kActivity, &activity);
  app->GetBoolean(kSticky, &sticky);
  app->GetBoolean(kNotificationsEnabled, &notifications_enabled);

  int64_t last_launch_time_internal = 0;
  base::Time last_launch_time;
  if (GetInt64FromPref(app, kLastLaunchTime, &last_launch_time_internal)) {
    last_launch_time = base::Time::FromInternalValue(last_launch_time_internal);
  }

  std::unique_ptr<AppInfo> app_info(
      new AppInfo(name, package_name, activity, last_launch_time, sticky,
                  notifications_enabled, ready_apps_.count(app_id) > 0,
                  arc::ShouldShowInLauncher(app_id)));
  return app_info;
}

bool ArcAppListPrefs::IsRegistered(const std::string& app_id) const {
  if (!IsArcEnabled())
    return false;

  const base::DictionaryValue* app = nullptr;
  const base::DictionaryValue* apps = prefs_->GetDictionary(prefs::kArcApps);
  if (!apps || !apps->GetDictionary(app_id, &app))
    return false;

  return true;
}

void ArcAppListPrefs::SetLastLaunchTime(const std::string& app_id,
                                        const base::Time& time) {
  if (!IsRegistered(app_id)) {
    NOTREACHED();
    return;
  }

  // Usage time on hidden should not be tracked.
  if (!arc::ShouldShowInLauncher(app_id))
    return;

  ScopedArcPrefUpdate update(prefs_, app_id, prefs::kArcApps);
  base::DictionaryValue* app_dict = update.Get();
  const std::string string_value = base::Int64ToString(time.ToInternalValue());
  app_dict->SetString(kLastLaunchTime, string_value);
}

void ArcAppListPrefs::DisableAllApps() {
  std::set<std::string> old_ready_apps;
  old_ready_apps.swap(ready_apps_);
  for (auto& app_id : old_ready_apps) {
    FOR_EACH_OBSERVER(Observer,
                      observer_list_,
                      OnAppReadyChanged(app_id, false));
  }
}

void ArcAppListPrefs::NotifyRegisteredApps() {
  if (apps_restored_)
    return;

  DCHECK(ready_apps_.empty());
  std::vector<std::string> app_ids = GetAppIdsNoArcEnabledCheck();
  for (const auto& app_id : app_ids) {
    std::unique_ptr<AppInfo> app_info = GetApp(app_id);
    if (!app_info) {
      NOTREACHED();
      continue;
    }
    FOR_EACH_OBSERVER(Observer,
                      observer_list_,
                      OnAppRegistered(app_id, *app_info));
  }

  apps_restored_ = true;
}

void ArcAppListPrefs::RemoveAllApps() {
  std::vector<std::string> app_ids = GetAppIdsNoArcEnabledCheck();
  for (const auto& app_id : app_ids)
    RemoveApp(app_id);
}

void ArcAppListPrefs::OnOptInEnabled(bool enabled) {
  if (enabled)
    NotifyRegisteredApps();
  else
    RemoveAllApps();
}

void ArcAppListPrefs::OnStateChanged(arc::ArcBridgeService::State state) {
  if (state != arc::ArcBridgeService::State::READY)
    DisableAllApps();
}

void ArcAppListPrefs::OnAppInstanceReady() {
  arc::ArcBridgeService* bridge_service = arc::ArcBridgeService::Get();
  if (!bridge_service) {
    NOTREACHED();
    return;
  }
  arc::mojom::AppInstance* app_instance = bridge_service->app_instance();
  if (!app_instance) {
    VLOG(2) << "Request to refresh app list when bridge service is not ready.";
    return;
  }

  app_instance->Init(binding_.CreateInterfacePtrAndBind());
  app_instance->RefreshAppList();
}

void ArcAppListPrefs::AddApp(const arc::mojom::AppInfo& app) {
  if (app.name.get().empty() || app.package_name.get().empty() ||
      app.activity.get().empty()) {
    VLOG(2) << "Name, package name, and activity cannot be empty.";
    return;
  }
  std::string app_id = GetAppId(app.package_name, app.activity);
  bool was_registered = IsRegistered(app_id);

  if (was_registered) {
    std::unique_ptr<ArcAppListPrefs::AppInfo> app_old_info = GetApp(app_id);
    if (app.name != app_old_info->name) {
      FOR_EACH_OBSERVER(Observer, observer_list_,
                        OnAppNameUpdated(app_id, app.name));
    }
  }

  ScopedArcPrefUpdate update(prefs_, app_id, prefs::kArcApps);
  base::DictionaryValue* app_dict = update.Get();
  app_dict->SetString(kName, app.name);
  app_dict->SetString(kPackageName, app.package_name);
  app_dict->SetString(kActivity, app.activity);
  app_dict->SetBoolean(kSticky, app.sticky);
  app_dict->SetBoolean(kNotificationsEnabled, app.notifications_enabled);

  // From now, app is available.
  if (!ready_apps_.count(app_id))
    ready_apps_.insert(app_id);

  if (was_registered) {
    FOR_EACH_OBSERVER(Observer,
                      observer_list_,
                      OnAppReadyChanged(app_id, true));
  } else {
    AppInfo app_info(app.name,
                     app.package_name,
                     app.activity,
                     base::Time(),
                     app.sticky,
                     app.notifications_enabled,
                     true,
                     arc::ShouldShowInLauncher(app_id));
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

  // Remove from prefs.
  DictionaryPrefUpdate update(prefs_, prefs::kArcApps);
  base::DictionaryValue* apps = update.Get();
  const bool removed = apps->Remove(app_id, nullptr);
  DCHECK(removed);

  FOR_EACH_OBSERVER(Observer,
                    observer_list_,
                    OnAppRemoved(app_id));

  // Remove local data on file system.
  const base::FilePath app_path = GetAppPath(app_id);
  content::BrowserThread::GetBlockingPool()->PostTask(
      FROM_HERE,
      base::Bind(&DeleteAppFolderFromFileThread, app_path));
}

void ArcAppListPrefs::OnAppListRefreshed(
    mojo::Array<arc::mojom::AppInfoPtr> apps) {
  DCHECK(IsArcEnabled());
  std::vector<std::string> old_apps = GetAppIds();

  ready_apps_.clear();
  for (size_t i = 0; i < apps.size(); ++i)
    AddApp(*apps[i]);

  // Detect removed ARC apps after current refresh.
  for (const auto& app_id : old_apps) {
    if (!ready_apps_.count(app_id))
      RemoveApp(app_id);
  }

  if (!is_initialized_) {
    is_initialized_ = true;
    UMA_HISTOGRAM_COUNTS_1000("Arc.AppsInstalledAtStartup", ready_apps_.size());
  }
}

void ArcAppListPrefs::OnAppAdded(arc::mojom::AppInfoPtr app) {
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

  RemovePackageFromPrefs(prefs_, package_name);
}

void ArcAppListPrefs::OnAppIcon(const mojo::String& package_name,
                                const mojo::String& activity,
                                arc::mojom::ScaleFactor scale_factor,
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

void ArcAppListPrefs::OnTaskCreated(int32_t task_id,
                                    const mojo::String& package_name,
                                    const mojo::String& activity) {
  FOR_EACH_OBSERVER(Observer,
                    observer_list_,
                    OnTaskCreated(task_id, package_name, activity));
}

void ArcAppListPrefs::OnTaskDestroyed(int32_t task_id) {
  FOR_EACH_OBSERVER(Observer, observer_list_, OnTaskDestroyed(task_id));
}

void ArcAppListPrefs::OnTaskSetActive(int32_t task_id) {
  FOR_EACH_OBSERVER(Observer, observer_list_, OnTaskSetActive(task_id));
}

void ArcAppListPrefs::OnNotificationsEnabledChanged(
    const mojo::String& package_name, bool enabled) {
  const base::DictionaryValue* apps = prefs_->GetDictionary(prefs::kArcApps);
  for (base::DictionaryValue::Iterator app(*apps);
       !app.IsAtEnd(); app.Advance()) {
    const base::DictionaryValue* app_dict;
    std::string app_package_name;
    if (!app.value().GetAsDictionary(&app_dict) ||
        !app_dict->GetString(kPackageName, &app_package_name)) {
      NOTREACHED();
      continue;
    }
    if (app_package_name != package_name) {
      continue;
    }
    ScopedArcPrefUpdate update(prefs_, app.key(), prefs::kArcApps);
    base::DictionaryValue* updateing_app_dict = update.Get();
    updateing_app_dict->SetBoolean(kNotificationsEnabled, enabled);
  }
  FOR_EACH_OBSERVER(Observer, observer_list_,
                    OnNotificationsEnabledChanged(package_name, enabled));
}

void ArcAppListPrefs::OnPackageAdded(
    arc::mojom::ArcPackageInfoPtr package_info) {
  DCHECK(IsArcEnabled());
  AddOrUpdatePackagePrefs(prefs_, *package_info);
}

void ArcAppListPrefs::OnPackageModified(
    arc::mojom::ArcPackageInfoPtr package_info) {
  DCHECK(IsArcEnabled());
  AddOrUpdatePackagePrefs(prefs_, *package_info);
}

void ArcAppListPrefs::OnPackageListRefreshed(
    mojo::Array<arc::mojom::ArcPackageInfoPtr> packages) {
  DCHECK(IsArcEnabled());

  const std::vector<std::string> old_packages(GetPackagesFromPrefs());
  std::unordered_set<std::string> current_packages;

  for (const auto& package : packages) {
    AddOrUpdatePackagePrefs(prefs_, *package);
    current_packages.insert((*package).package_name);
  }

  for (const auto& package_name : old_packages) {
    if (!current_packages.count(package_name))
      RemovePackageFromPrefs(prefs_, package_name);
  }
}

std::vector<std::string> ArcAppListPrefs::GetPackagesFromPrefs() const {
  std::vector<std::string> packages;
  if (!IsArcEnabled())
    return packages;

  const base::DictionaryValue* package_prefs =
      prefs_->GetDictionary(prefs::kArcPackages);
  for (base::DictionaryValue::Iterator package(*package_prefs);
       !package.IsAtEnd(); package.Advance()) {
    packages.push_back(package.key());
  }

  return packages;
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
                                  const base::Time& last_launch_time,
                                  bool sticky,
                                  bool notifications_enabled,
                                  bool ready,
                                  bool showInLauncher)
    : name(name),
      package_name(package_name),
      activity(activity),
      last_launch_time(last_launch_time),
      sticky(sticky),
      notifications_enabled(notifications_enabled),
      ready(ready),
      showInLauncher(showInLauncher) {}

ArcAppListPrefs::PackageInfo::PackageInfo(const std::string& package_name,
                                          int32_t package_version,
                                          int64_t last_backup_android_id,
                                          int64_t last_backup_time,
                                          bool should_sync)
    : package_name(package_name),
      package_version(package_version),
      last_backup_android_id(last_backup_android_id),
      last_backup_time(last_backup_time),
      should_sync(should_sync) {}
