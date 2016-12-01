// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"

#include <stddef.h>

#include <string>

#include "base/files/file_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/task_runner_util.h"
#include "chrome/browser/chromeos/app_mode/arc/arc_kiosk_app_service.h"
#include "chrome/browser/chromeos/arc/policy/arc_policy_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_list_service.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs_factory.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ui/app_list/arc/arc_package_syncable_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/arc/arc_bridge_service.h"
#include "components/crx_file/id_util.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

const char kActivity[] = "activity";
const char kIconResourceId[] = "icon_resource_id";
const char kInstallTime[] = "install_time";
const char kIntentUri[] = "intent_uri";
const char kLastBackupAndroidId[] = "last_backup_android_id";
const char kLastBackupTime[] = "last_backup_time";
const char kLastLaunchTime[] = "lastlaunchtime";
const char kLaunchable[] = "launchable";
const char kName[] = "name";
const char kNotificationsEnabled[] = "notifications_enabled";
const char kOrientationLock[] = "orientation_lock";
const char kPackageName[] = "package_name";
const char kPackageVersion[] = "package_version";
const char kSticky[] = "sticky";
const char kShortcut[] = "shortcut";
const char kShouldSync[] = "should_sync";
const char kSystem[] = "system";
const char kUninstalled[] = "uninstalled";

constexpr uint32_t kSetNotificationsEnabledMinVersion = 6;
constexpr uint32_t kRequestIconMinVersion = 9;

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
    if (!dict->GetDictionaryWithoutPathExpansion(id_, &dict_item)) {
      dict_item = new base::DictionaryValue();
      dict->SetWithoutPathExpansion(id_, dict_item);
    }
    return dict_item;
  }

 private:
  const std::string id_;

  DISALLOW_COPY_AND_ASSIGN(ScopedArcPrefUpdate);
};

// Accessor for deferred set notifications enabled requests in prefs.
class SetNotificationsEnabledDeferred {
 public:
  explicit SetNotificationsEnabledDeferred(PrefService* prefs)
      : prefs_(prefs) {}

  void Put(const std::string& app_id, bool enabled) {
    DictionaryPrefUpdate update(prefs_,
                                prefs::kArcSetNotificationsEnabledDeferred);
    base::DictionaryValue* const dict = update.Get();
    dict->SetBooleanWithoutPathExpansion(app_id, enabled);
  }

  bool Get(const std::string& app_id, bool* enabled) {
    const base::DictionaryValue* dict =
        prefs_->GetDictionary(prefs::kArcSetNotificationsEnabledDeferred);
    return dict->GetBoolean(app_id, enabled);
  }

  void Remove(const std::string& app_id) {
    DictionaryPrefUpdate update(prefs_,
                                prefs::kArcSetNotificationsEnabledDeferred);
    base::DictionaryValue* const dict = update.Get();
    dict->RemoveWithoutPathExpansion(app_id, /* out_value */ nullptr);
  }

 private:
  PrefService* const prefs_;
};

bool InstallIconFromFileThread(const std::string& app_id,
                               ui::ScaleFactor scale_factor,
                               const base::FilePath& icon_path,
                               const std::vector<uint8_t>& content_png) {
  DCHECK(content::BrowserThread::GetBlockingPool()->RunsTasksOnCurrentThread());
  DCHECK(!content_png.empty());

  base::CreateDirectory(icon_path.DirName());

  int wrote =
      base::WriteFile(icon_path, reinterpret_cast<const char*>(&content_png[0]),
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
  arc::ArcSessionManager* arc_session_manager = arc::ArcSessionManager::Get();
  return arc_session_manager &&
         arc_session_manager->state() !=
             arc::ArcSessionManager::State::NOT_INITIALIZED &&
         arc_session_manager->IsArcEnabled();
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

base::FilePath ToIconPath(const base::FilePath& app_path,
                          ui::ScaleFactor scale_factor) {
  DCHECK(!app_path.empty());
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

}  // namespace

// static
ArcAppListPrefs* ArcAppListPrefs::Create(
    Profile* profile,
    arc::InstanceHolder<arc::mojom::AppInstance>* app_instance_holder) {
  return new ArcAppListPrefs(profile, app_instance_holder);
}

// static
void ArcAppListPrefs::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(prefs::kArcApps);
  registry->RegisterDictionaryPref(prefs::kArcPackages);
  registry->RegisterDictionaryPref(prefs::kArcSetNotificationsEnabledDeferred);
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

ArcAppListPrefs::ArcAppListPrefs(
    Profile* profile,
    arc::InstanceHolder<arc::mojom::AppInstance>* app_instance_holder)
    : profile_(profile),
      prefs_(profile->GetPrefs()),
      app_instance_holder_(app_instance_holder),
      binding_(this),
      default_apps_(this, profile),
      weak_ptr_factory_(this) {
  DCHECK(profile);
  DCHECK(app_instance_holder);
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  const base::FilePath& base_path = profile->GetPath();
  base_path_ = base_path.AppendASCII(prefs::kArcApps);

  arc::ArcSessionManager* arc_session_manager = arc::ArcSessionManager::Get();
  if (!arc_session_manager)
    return;

  DCHECK(arc::ArcSessionManager::IsAllowedForProfile(profile));

  const std::vector<std::string> existing_app_ids = GetAppIds();
  tracked_apps_.insert(existing_app_ids.begin(), existing_app_ids.end());

  // Once default apps are ready OnDefaultAppsReady is called.
}

ArcAppListPrefs::~ArcAppListPrefs() {
  // A reference to ArcBridgeService is kept here so that it would crash the
  // tests where ArcBridgeService and ArcAppListPrefs are not destroyed in right
  // order.
  arc::ArcBridgeService* bridge_service = arc::ArcBridgeService::Get();
  if (bridge_service)
    app_instance_holder_->RemoveObserver(this);

  arc::ArcSessionManager* arc_session_manager = arc::ArcSessionManager::Get();
  if (arc_session_manager)
    arc_session_manager->RemoveObserver(this);
}

void ArcAppListPrefs::StartPrefs() {
  arc::ArcSessionManager* arc_session_manager = arc::ArcSessionManager::Get();
  CHECK(arc_session_manager);

  if (arc_session_manager->state() !=
      arc::ArcSessionManager::State::NOT_INITIALIZED)
    OnOptInEnabled(arc_session_manager->IsArcEnabled());
  arc_session_manager->AddObserver(this);

  app_instance_holder_->AddObserver(this);
  if (!app_instance_holder_->has_instance())
    OnInstanceClosed();
}

base::FilePath ArcAppListPrefs::GetAppPath(const std::string& app_id) const {
  return base_path_.AppendASCII(app_id);
}

base::FilePath ArcAppListPrefs::MaybeGetIconPathForDefaultApp(
    const std::string& app_id,
    ui::ScaleFactor scale_factor) const {
  const ArcDefaultAppList::AppInfo* default_app = default_apps_.GetApp(app_id);
  if (!default_app || default_app->app_path.empty())
    return base::FilePath();

  return ToIconPath(default_app->app_path, scale_factor);
}

base::FilePath ArcAppListPrefs::GetIconPath(
    const std::string& app_id,
    ui::ScaleFactor scale_factor) const {
  return ToIconPath(GetAppPath(app_id), scale_factor);
}

void ArcAppListPrefs::RequestIcon(const std::string& app_id,
                                  ui::ScaleFactor scale_factor) {
  if (!IsRegistered(app_id)) {
    VLOG(2) << "Request to load icon for non-registered app: " << app_id << ".";
    return;
  }

  // In case app is not ready, defer this request.
  if (!ready_apps_.count(app_id)) {
    request_icon_deferred_[app_id] =
        request_icon_deferred_[app_id] | 1 << scale_factor;
    return;
  }

  if (!app_instance_holder_->has_instance()) {
    // AppInstance should be ready since we have app_id in ready_apps_. This
    // can happen in browser_tests.
    return;
  }

  std::unique_ptr<AppInfo> app_info = GetApp(app_id);
  if (!app_info) {
    VLOG(2) << "Failed to get app info: " << app_id << ".";
    return;
  }

  if (app_info->icon_resource_id.empty()) {
    auto* app_instance =
        app_instance_holder_->GetInstanceForMethod("RequestAppIcon");
    // Version 0 instance should always be available here because has_instance()
    // returned true above.
    DCHECK(app_instance);
    app_instance->RequestAppIcon(
        app_info->package_name, app_info->activity,
        static_cast<arc::mojom::ScaleFactor>(scale_factor));
  } else {
    auto* app_instance = app_instance_holder_->GetInstanceForMethod(
        "RequestIcon", kRequestIconMinVersion);
    if (!app_instance)
      return;  // The instance version on ARC side was too old.
    app_instance->RequestIcon(
        app_info->icon_resource_id,
        static_cast<arc::mojom::ScaleFactor>(scale_factor),
        base::Bind(&ArcAppListPrefs::OnIcon, base::Unretained(this), app_id,
                   static_cast<arc::mojom::ScaleFactor>(scale_factor)));
  }
}

void ArcAppListPrefs::SetNotificationsEnabled(const std::string& app_id,
                                              bool enabled) {
  if (!IsRegistered(app_id)) {
    VLOG(2) << "Request to set notifications enabled flag for non-registered "
            << "app:" << app_id << ".";
    return;
  }

  std::unique_ptr<AppInfo> app_info = GetApp(app_id);
  if (!app_info) {
    VLOG(2) << "Failed to get app info: " << app_id << ".";
    return;
  }

  // In case app is not ready, defer this request.
  if (!ready_apps_.count(app_id)) {
    SetNotificationsEnabledDeferred(prefs_).Put(app_id, enabled);
    for (auto& observer : observer_list_)
      observer.OnNotificationsEnabledChanged(app_info->package_name, enabled);
    return;
  }

  auto* app_instance = app_instance_holder_->GetInstanceForMethod(
      "SetNotificationsEnabled", kSetNotificationsEnabledMinVersion);
  if (!app_instance)
    return;

  SetNotificationsEnabledDeferred(prefs_).Remove(app_id);
  app_instance->SetNotificationsEnabled(app_info->package_name, enabled);
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
  bool system = false;

  GetInt64FromPref(package, kLastBackupAndroidId, &last_backup_android_id);
  GetInt64FromPref(package, kLastBackupTime, &last_backup_time);
  package->GetInteger(kPackageVersion, &package_version);
  package->GetBoolean(kShouldSync, &should_sync);
  package->GetBoolean(kSystem, &system);

  return base::MakeUnique<PackageInfo>(package_name, package_version,
                                       last_backup_android_id, last_backup_time,
                                       should_sync, system);
}

std::vector<std::string> ArcAppListPrefs::GetAppIds() const {
  if (!IsArcEnabled()) {
    // Default Arc apps available before OptIn.
    std::vector<std::string> ids;
    for (const auto& default_app : default_apps_.app_map()) {
      if (default_apps_.HasApp(default_app.first))
        ids.push_back(default_app.first);
    }
    return ids;
  }
  return GetAppIdsNoArcEnabledCheck();
}

std::vector<std::string> ArcAppListPrefs::GetAppIdsNoArcEnabledCheck() const {
  std::vector<std::string> ids;

  const base::DictionaryValue* apps = prefs_->GetDictionary(prefs::kArcApps);
  DCHECK(apps);

  // crx_file::id_util is de-facto utility for id generation.
  for (base::DictionaryValue::Iterator app_id(*apps); !app_id.IsAtEnd();
       app_id.Advance()) {
    if (!crx_file::id_util::IdIsValid(app_id.key()))
      continue;

    ids.push_back(app_id.key());
  }

  return ids;
}

std::unique_ptr<ArcAppListPrefs::AppInfo> ArcAppListPrefs::GetApp(
    const std::string& app_id) const {
  // Information for default app is available before Arc enabled.
  if (!IsArcEnabled() && !default_apps_.HasApp(app_id))
    return std::unique_ptr<AppInfo>();

  const base::DictionaryValue* app = nullptr;
  const base::DictionaryValue* apps = prefs_->GetDictionary(prefs::kArcApps);
  if (!apps || !apps->GetDictionaryWithoutPathExpansion(app_id, &app))
    return std::unique_ptr<AppInfo>();

  std::string name;
  std::string package_name;
  std::string activity;
  std::string intent_uri;
  std::string icon_resource_id;
  bool sticky = false;
  bool notifications_enabled = true;
  bool shortcut = false;
  bool launchable = true;
  int orientation_lock_value = 0;
  app->GetString(kName, &name);
  app->GetString(kPackageName, &package_name);
  app->GetString(kActivity, &activity);
  app->GetString(kIntentUri, &intent_uri);
  app->GetString(kIconResourceId, &icon_resource_id);
  app->GetBoolean(kSticky, &sticky);
  app->GetBoolean(kNotificationsEnabled, &notifications_enabled);
  app->GetBoolean(kShortcut, &shortcut);
  app->GetBoolean(kLaunchable, &launchable);
  app->GetInteger(kOrientationLock, &orientation_lock_value);

  DCHECK(!name.empty());
  DCHECK(!shortcut || activity.empty());
  DCHECK(!shortcut || !intent_uri.empty());

  int64_t last_launch_time_internal = 0;
  base::Time last_launch_time;
  if (GetInt64FromPref(app, kLastLaunchTime, &last_launch_time_internal)) {
    last_launch_time = base::Time::FromInternalValue(last_launch_time_internal);
  }

  bool deferred;
  if (SetNotificationsEnabledDeferred(prefs_).Get(app_id, &deferred))
    notifications_enabled = deferred;

  arc::mojom::OrientationLock orientation_lock =
      static_cast<arc::mojom::OrientationLock>(orientation_lock_value);

  return base::MakeUnique<AppInfo>(
      name, package_name, activity, intent_uri, icon_resource_id,
      last_launch_time, GetInstallTime(app_id), sticky, notifications_enabled,
      ready_apps_.count(app_id) > 0,
      launchable && arc::ShouldShowInLauncher(app_id), shortcut, launchable,
      orientation_lock);
}

bool ArcAppListPrefs::IsRegistered(const std::string& app_id) const {
  if (!IsArcEnabled() && !default_apps_.HasApp(app_id))
    return false;

  const base::DictionaryValue* app = nullptr;
  const base::DictionaryValue* apps = prefs_->GetDictionary(prefs::kArcApps);
  return apps && apps->GetDictionaryWithoutPathExpansion(app_id, &app);
}

bool ArcAppListPrefs::IsDefault(const std::string& app_id) const {
  return default_apps_.HasApp(app_id);
}

bool ArcAppListPrefs::IsOem(const std::string& app_id) const {
  const ArcDefaultAppList::AppInfo* app_info = default_apps_.GetApp(app_id);
  return app_info && app_info->oem;
}

bool ArcAppListPrefs::IsShortcut(const std::string& app_id) const {
  std::unique_ptr<ArcAppListPrefs::AppInfo> app_info = GetApp(app_id);
  return app_info && app_info->shortcut;
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
  std::unordered_set<std::string> old_ready_apps;
  old_ready_apps.swap(ready_apps_);
  for (auto& app_id : old_ready_apps) {
    for (auto& observer : observer_list_)
      observer.OnAppReadyChanged(app_id, false);
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

    // Default apps are reported earlier.
    if (tracked_apps_.insert(app_id).second) {
      for (auto& observer : observer_list_)
        observer.OnAppRegistered(app_id, *app_info);
    }
  }

  apps_restored_ = true;
}

void ArcAppListPrefs::RemoveAllApps() {
  std::vector<std::string> app_ids = GetAppIdsNoArcEnabledCheck();
  for (const auto& app_id : app_ids) {
    if (!default_apps_.HasApp(app_id)) {
      RemoveApp(app_id);
    } else {
      if (ready_apps_.count(app_id)) {
        ready_apps_.erase(app_id);
        for (auto& observer : observer_list_)
          observer.OnAppReadyChanged(app_id, false);
      }
    }
  }
  DCHECK(ready_apps_.empty());
}

void ArcAppListPrefs::OnOptInEnabled(bool enabled) {
  UpdateDefaultAppsHiddenState();

  if (enabled)
    NotifyRegisteredApps();
  else
    RemoveAllApps();
}

void ArcAppListPrefs::UpdateDefaultAppsHiddenState() {
  const bool was_hidden = default_apps_.is_hidden();
  // There is no a blacklisting mechanism for Android apps. Until there is
  // one, we have no option but to ban all pre-installed apps on Android side.
  // Match this requirement and don't show pre-installed apps for managed users
  // in app list.
  const bool now_hidden = arc::policy_util::IsAccountManaged(profile_);
  default_apps_.set_hidden(now_hidden);
  if (was_hidden && !default_apps_.is_hidden())
    RegisterDefaultApps();
}

void ArcAppListPrefs::OnDefaultAppsReady() {
  // Apply uninstalled packages now.

  const std::vector<std::string> uninstalled_package_names =
      GetPackagesFromPrefs(false);
  for (const auto& uninstalled_package_name : uninstalled_package_names)
    default_apps_.MaybeMarkPackageUninstalled(uninstalled_package_name, true);

  UpdateDefaultAppsHiddenState();

  default_apps_ready_ = true;
  if (!default_apps_ready_callback_.is_null())
    default_apps_ready_callback_.Run();

  StartPrefs();
}

void ArcAppListPrefs::RegisterDefaultApps() {
  // Report default apps first, note, app_map includes uninstalled apps as well.
  for (const auto& default_app : default_apps_.app_map()) {
    const std::string& app_id = default_app.first;
    if (!default_apps_.HasApp(app_id))
      continue;
    const ArcDefaultAppList::AppInfo& app_info = *default_app.second.get();
    AddAppAndShortcut(false /* app_ready */,
                      app_info.name,
                      app_info.package_name,
                      app_info.activity,
                      std::string() /* intent_uri */,
                      std::string() /* icon_resource_id */,
                      false /* sticky */,
                      false /* notifications_enabled */,
                      false /* shortcut */,
                      true /* launchable */,
                      arc::mojom::OrientationLock::NONE);
  }
}

void ArcAppListPrefs::SetDefaltAppsReadyCallback(base::Closure callback) {
  DCHECK(!callback.is_null());
  DCHECK(default_apps_ready_callback_.is_null());
  default_apps_ready_callback_ = callback;
  if (default_apps_ready_)
    default_apps_ready_callback_.Run();
}

void ArcAppListPrefs::OnInstanceReady() {
  // Init() is also available at version 0.
  arc::mojom::AppInstance* app_instance =
      app_instance_holder_->GetInstanceForMethod("RefreshAppList");

  // Note, sync_service_ may be nullptr in testing.
  sync_service_ = arc::ArcPackageSyncableService::Get(profile_);

  // Kiosk apps should be run only for kiosk sessions.
  if (user_manager::UserManager::Get()->IsLoggedInAsArcKioskApp()) {
    kiosk_app_service_ = chromeos::ArcKioskAppService::Get(profile_);
    DCHECK(kiosk_app_service_);
  }

  // In some tests app_instance may not be set.
  if (!app_instance)
    return;

  is_initialized_ = false;

  app_instance->Init(binding_.CreateInterfacePtrAndBind());
  app_instance->RefreshAppList();
}

void ArcAppListPrefs::OnInstanceClosed() {
  DisableAllApps();
  binding_.Close();

  if (sync_service_) {
    sync_service_->StopSyncing(syncer::ARC_PACKAGE);
    sync_service_ = nullptr;
  }

  kiosk_app_service_ = nullptr;
  is_initialized_ = false;
  package_list_initial_refreshed_ = false;
}

void ArcAppListPrefs::MaybeAddNonLaunchableApp(
    const base::Optional<std::string>& name,
    const std::string& package_name,
    const std::string& activity) {
  DCHECK(IsArcEnabled());
  if (IsRegistered(GetAppId(package_name, activity)))
    return;

  AddAppAndShortcut(true /* app_ready */, name.has_value() ? *name : "",
                    package_name, activity, std::string() /* intent_uri */,
                    std::string() /* icon_resource_id */, false /* sticky */,
                    false /* notifications_enabled */, false /* shortcut */,
                    false /* launchable */, arc::mojom::OrientationLock::NONE);
}

void ArcAppListPrefs::AddAppAndShortcut(
    bool app_ready,
    const std::string& name,
    const std::string& package_name,
    const std::string& activity,
    const std::string& intent_uri,
    const std::string& icon_resource_id,
    const bool sticky,
    const bool notifications_enabled,
    const bool shortcut,
    const bool launchable,
    const arc::mojom::OrientationLock orientation_lock) {
  const std::string app_id = shortcut ? GetAppId(package_name, intent_uri)
                                      : GetAppId(package_name, activity);
  std::string updated_name = name;
  // Add "(beta)" string to Play Store. See crbug.com/644576 for details.
  if (app_id == arc::kPlayStoreAppId)
    updated_name = l10n_util::GetStringUTF8(IDS_ARC_PLAYSTORE_ICON_TITLE_BETA);

  const bool was_tracked = tracked_apps_.count(app_id);
  if (was_tracked) {
    std::unique_ptr<ArcAppListPrefs::AppInfo> app_old_info = GetApp(app_id);
    DCHECK(app_old_info);
    DCHECK(launchable);
    if (updated_name != app_old_info->name) {
      for (auto& observer : observer_list_)
        observer.OnAppNameUpdated(app_id, updated_name);
    }
  }

  ScopedArcPrefUpdate update(prefs_, app_id, prefs::kArcApps);
  base::DictionaryValue* app_dict = update.Get();
  app_dict->SetString(kName, updated_name);
  app_dict->SetString(kPackageName, package_name);
  app_dict->SetString(kActivity, activity);
  app_dict->SetString(kIntentUri, intent_uri);
  app_dict->SetString(kIconResourceId, icon_resource_id);
  app_dict->SetBoolean(kSticky, sticky);
  app_dict->SetBoolean(kNotificationsEnabled, notifications_enabled);
  app_dict->SetBoolean(kShortcut, shortcut);
  app_dict->SetBoolean(kLaunchable, launchable);
  app_dict->SetInteger(kOrientationLock, static_cast<int>(orientation_lock));

  // Note the install time is the first time the Chrome OS sees the app, not the
  // actual install time in Android side.
  if (GetInstallTime(app_id).is_null()) {
    std::string install_time_str =
        base::Int64ToString(base::Time::Now().ToInternalValue());
    app_dict->SetString(kInstallTime, install_time_str);
  }

  const bool was_disabled = ready_apps_.count(app_id) == 0;
  DCHECK(!(!was_disabled && !app_ready));
  if (was_disabled && app_ready)
    ready_apps_.insert(app_id);

  if (was_tracked) {
    if (was_disabled && app_ready) {
      for (auto& observer : observer_list_)
        observer.OnAppReadyChanged(app_id, true);
    }
  } else {
    AppInfo app_info(updated_name, package_name, activity, intent_uri,
                     icon_resource_id, base::Time(), GetInstallTime(app_id),
                     sticky, notifications_enabled, true,
                     launchable && arc::ShouldShowInLauncher(app_id), shortcut,
                     launchable, orientation_lock);
    for (auto& observer : observer_list_)
      observer.OnAppRegistered(app_id, app_info);
    tracked_apps_.insert(app_id);
  }

  if (app_ready) {
    auto deferred_icons = request_icon_deferred_.find(app_id);
    if (deferred_icons != request_icon_deferred_.end()) {
      for (uint32_t i = ui::SCALE_FACTOR_100P; i < ui::NUM_SCALE_FACTORS; ++i) {
        if (deferred_icons->second & (1 << i)) {
          RequestIcon(app_id, static_cast<ui::ScaleFactor>(i));
        }
      }
      request_icon_deferred_.erase(deferred_icons);
    }

    bool deferred_notifications_enabled;
    if (SetNotificationsEnabledDeferred(prefs_).Get(
            app_id, &deferred_notifications_enabled)) {
      SetNotificationsEnabled(app_id, deferred_notifications_enabled);
    }
  }
}

void ArcAppListPrefs::RemoveApp(const std::string& app_id) {
  // Delete cached icon if there is any.
  std::unique_ptr<ArcAppListPrefs::AppInfo> app_info = GetApp(app_id);
  if (app_info && !app_info->icon_resource_id.empty()) {
    arc::RemoveCachedIcon(app_info->icon_resource_id);
  }

  // From now, app is not available.
  ready_apps_.erase(app_id);

  // app_id may be released by observers, get the path first. It should be done
  // before removing prefs entry in order not to mix with pre-build default apps
  // files.
  const base::FilePath app_path = GetAppPath(app_id);

  // Remove from prefs.
  DictionaryPrefUpdate update(prefs_, prefs::kArcApps);
  base::DictionaryValue* apps = update.Get();
  const bool removed = apps->Remove(app_id, nullptr);
  DCHECK(removed);

  DCHECK(tracked_apps_.count(app_id));
  for (auto& observer : observer_list_)
    observer.OnAppRemoved(app_id);
  tracked_apps_.erase(app_id);

  // Remove local data on file system.
  content::BrowserThread::GetBlockingPool()->PostTask(
      FROM_HERE, base::Bind(&DeleteAppFolderFromFileThread, app_path));
}

void ArcAppListPrefs::AddOrUpdatePackagePrefs(
    PrefService* prefs, const arc::mojom::ArcPackageInfo& package) {
  DCHECK(IsArcEnabled());
  const std::string& package_name = package.package_name;
  default_apps_.MaybeMarkPackageUninstalled(package_name, false);
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
  package_dict->SetBoolean(kSystem, package.system);
  package_dict->SetBoolean(kUninstalled, false);
}

void ArcAppListPrefs::RemovePackageFromPrefs(PrefService* prefs,
                                             const std::string& package_name) {
  DCHECK(IsArcEnabled());
  default_apps_.MaybeMarkPackageUninstalled(package_name, true);
  if (!default_apps_.HasPackage(package_name)) {
    DictionaryPrefUpdate update(prefs, prefs::kArcPackages);
    base::DictionaryValue* packages = update.Get();
    const bool removed = packages->RemoveWithoutPathExpansion(package_name,
                                                              nullptr);
    DCHECK(removed);
  } else {
    ScopedArcPrefUpdate update(prefs, package_name, prefs::kArcPackages);
    base::DictionaryValue* package_dict = update.Get();
    package_dict->SetBoolean(kUninstalled, true);
  }
}

void ArcAppListPrefs::OnAppListRefreshed(
    std::vector<arc::mojom::AppInfoPtr> apps) {
  DCHECK(IsArcEnabled());
  std::vector<std::string> old_apps = GetAppIds();

  ready_apps_.clear();
  for (const auto& app : apps) {
    // TODO(oshima): Do we have to update orientation?
    AddAppAndShortcut(
        true /* app_ready */,
        app->name,
        app->package_name,
        app->activity,
        std::string() /* intent_uri */,
        std::string() /* icon_resource_id */,
        app->sticky,
        app->notifications_enabled,
        false /* shortcut */,
        true /* launchable */,
        app->orientation_lock);
  }

  // Detect removed ARC apps after current refresh.
  for (const auto& app_id : old_apps) {
    if (ready_apps_.count(app_id))
      continue;

    if (IsShortcut(app_id)) {
      // If this is a shortcut, we just mark it as ready.
      ready_apps_.insert(app_id);
    } else {
      // Default apps may not be installed yet at this moment.
      if (!default_apps_.HasApp(app_id))
        RemoveApp(app_id);
    }
  }

  if (!is_initialized_) {
    is_initialized_ = true;
    UMA_HISTOGRAM_COUNTS_1000("Arc.AppsInstalledAtStartup", ready_apps_.size());
  }
}

void ArcAppListPrefs::OnTaskOrientationLockRequested(
    int32_t task_id,
    const arc::mojom::OrientationLock orientation_lock) {
  for (auto& observer : observer_list_)
    observer.OnTaskOrientationLockRequested(task_id, orientation_lock);
}

void ArcAppListPrefs::AddApp(const arc::mojom::AppInfo& app_info) {
  if ((app_info.name.empty() || app_info.package_name.empty() ||
       app_info.activity.empty())) {
    VLOG(2) << "App Name, package name, and activity cannot be empty.";
    return;
  }

  AddAppAndShortcut(true /* app_ready */,
                    app_info.name,
                    app_info.package_name,
                    app_info.activity,
                    std::string() /* intent_uri */,
                    std::string() /* icon_resource_id */,
                    app_info.sticky,
                    app_info.notifications_enabled,
                    false /* shortcut */,
                    true /* launchable */,
                    app_info.orientation_lock);
}

void ArcAppListPrefs::OnAppAddedDeprecated(arc::mojom::AppInfoPtr app) {
  AddApp(*app);
}

void ArcAppListPrefs::OnPackageAppListRefreshed(
    const std::string& package_name,
    std::vector<arc::mojom::AppInfoPtr> apps) {
  if (package_name.empty()) {
    VLOG(2) << "Package name cannot be empty.";
    return;
  }

  std::unordered_set<std::string> apps_to_remove =
      GetAppsForPackage(package_name);
  default_apps_.MaybeMarkPackageUninstalled(package_name, false);
  for (const auto& app : apps) {
    apps_to_remove.erase(GetAppId(app->package_name, app->activity));
    AddApp(*app);
  }

  for (const auto& app_id : apps_to_remove)
    RemoveApp(app_id);
}

void ArcAppListPrefs::OnInstallShortcut(arc::mojom::ShortcutInfoPtr shortcut) {
  if ((shortcut->name.empty() || shortcut->intent_uri.empty())) {
    VLOG(2) << "Shortcut Name, and intent_uri cannot be empty.";
    return;
  }

  AddAppAndShortcut(true /* app_ready */,
                    shortcut->name,
                    shortcut->package_name,
                    std::string() /* activity */,
                    shortcut->intent_uri,
                    shortcut->icon_resource_id,
                    false /* sticky */,
                    false /* notifications_enabled */,
                    true /* shortcut */,
                    true /* launchable */,
                    arc::mojom::OrientationLock::NONE);
}

std::unordered_set<std::string> ArcAppListPrefs::GetAppsForPackage(
    const std::string& package_name) const {
  std::unordered_set<std::string> app_set;
  const base::DictionaryValue* apps = prefs_->GetDictionary(prefs::kArcApps);
  for (base::DictionaryValue::Iterator app_it(*apps); !app_it.IsAtEnd();
       app_it.Advance()) {
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

    app_set.insert(app_it.key());
  }

  return app_set;
}

void ArcAppListPrefs::OnPackageRemoved(const std::string& package_name) {
  const std::unordered_set<std::string> apps_to_remove =
      GetAppsForPackage(package_name);
  for (const auto& app_id : apps_to_remove)
    RemoveApp(app_id);

  RemovePackageFromPrefs(prefs_, package_name);
  for (auto& observer : observer_list_)
    observer.OnPackageRemoved(package_name);
}

void ArcAppListPrefs::OnAppIcon(const std::string& package_name,
                                const std::string& activity,
                                arc::mojom::ScaleFactor scale_factor,
                                const std::vector<uint8_t>& icon_png_data) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_NE(0u, icon_png_data.size());

  std::string app_id = GetAppId(package_name, activity);
  if (!IsRegistered(app_id)) {
    VLOG(2) << "Request to update icon for non-registered app: " << app_id;
    return;
  }

  InstallIcon(app_id, static_cast<ui::ScaleFactor>(scale_factor),
              icon_png_data);
}

void ArcAppListPrefs::OnIcon(const std::string& app_id,
                             arc::mojom::ScaleFactor scale_factor,
                             const std::vector<uint8_t>& icon_png_data) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_NE(0u, icon_png_data.size());

  if (!IsRegistered(app_id)) {
    VLOG(2) << "Request to update icon for non-registered app: " << app_id;
    return;
  }

  InstallIcon(app_id, static_cast<ui::ScaleFactor>(scale_factor),
              icon_png_data);
}

void ArcAppListPrefs::OnTaskCreated(int32_t task_id,
                                    const std::string& package_name,
                                    const std::string& activity,
                                    const base::Optional<std::string>& name) {
  MaybeAddNonLaunchableApp(name, package_name, activity);
  for (auto& observer : observer_list_)
    observer.OnTaskCreated(task_id, package_name, activity);
}

void ArcAppListPrefs::OnTaskDestroyed(int32_t task_id) {
  for (auto& observer : observer_list_)
    observer.OnTaskDestroyed(task_id);
}

void ArcAppListPrefs::OnTaskSetActive(int32_t task_id) {
  for (auto& observer : observer_list_)
    observer.OnTaskSetActive(task_id);
}

void ArcAppListPrefs::OnNotificationsEnabledChanged(
    const std::string& package_name,
    bool enabled) {
  const base::DictionaryValue* apps = prefs_->GetDictionary(prefs::kArcApps);
  for (base::DictionaryValue::Iterator app(*apps); !app.IsAtEnd();
       app.Advance()) {
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
  for (auto& observer : observer_list_)
    observer.OnNotificationsEnabledChanged(package_name, enabled);
}

void ArcAppListPrefs::MaybeShowPackageInAppLauncher(
    const arc::mojom::ArcPackageInfo& package_info) {
  // Ignore system packages and auxiliary packages.
  if (!package_info.sync || package_info.system)
    return;

  std::unordered_set<std::string> app_ids =
      GetAppsForPackage(package_info.package_name);
  for (const auto& app_id : app_ids) {
    std::unique_ptr<ArcAppListPrefs::AppInfo> app_info = GetApp(app_id);
    if (!app_info) {
      NOTREACHED();
      continue;
    }
    if (!app_info->showInLauncher)
      continue;

    AppListService* service = AppListService::Get();
    CHECK(service);
    service->ShowForAppInstall(profile_, app_id, false);
    break;
  }
}

void ArcAppListPrefs::OnPackageAdded(
    arc::mojom::ArcPackageInfoPtr package_info) {
  DCHECK(IsArcEnabled());

  // Ignore packages installed by internal sync.
  DCHECK(sync_service_);
  const bool new_package_in_system = !GetPackage(package_info->package_name) &&
      !sync_service_->IsPackageSyncing(package_info->package_name);

  AddOrUpdatePackagePrefs(prefs_, *package_info);
  for (auto& observer : observer_list_)
    observer.OnPackageInstalled(*package_info);
  if (new_package_in_system)
    MaybeShowPackageInAppLauncher(*package_info);
}

void ArcAppListPrefs::OnPackageModified(
    arc::mojom::ArcPackageInfoPtr package_info) {
  DCHECK(IsArcEnabled());
  AddOrUpdatePackagePrefs(prefs_, *package_info);
  for (auto& observer : observer_list_)
    observer.OnPackageModified(*package_info);
}

void ArcAppListPrefs::OnPackageListRefreshed(
    std::vector<arc::mojom::ArcPackageInfoPtr> packages) {
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

  // TODO(lgcheng@) File http://b/31944261. Remove the flag after Android side
  // cleanup.
  if (package_list_initial_refreshed_)
    return;

  package_list_initial_refreshed_ = true;
  for (auto& observer : observer_list_)
    observer.OnPackageListInitialRefreshed();
}

std::vector<std::string> ArcAppListPrefs::GetPackagesFromPrefs() const {
  return GetPackagesFromPrefs(true);
}

std::vector<std::string> ArcAppListPrefs::GetPackagesFromPrefs(
    bool installed) const {
  std::vector<std::string> packages;
  if (!IsArcEnabled() && installed)
    return packages;

  const base::DictionaryValue* package_prefs =
      prefs_->GetDictionary(prefs::kArcPackages);
  for (base::DictionaryValue::Iterator package(*package_prefs);
       !package.IsAtEnd(); package.Advance()) {
    const base::DictionaryValue* package_info;
    if (!package.value().GetAsDictionary(&package_info)) {
      NOTREACHED();
      continue;
    }

    bool uninstalled = false;
    package_info->GetBoolean(kUninstalled, &uninstalled);
    if (installed != !uninstalled)
      continue;

    packages.push_back(package.key());
  }

  return packages;
}

base::Time ArcAppListPrefs::GetInstallTime(const std::string& app_id) const {
  const base::DictionaryValue* app = nullptr;
  const base::DictionaryValue* apps = prefs_->GetDictionary(prefs::kArcApps);
  if (!apps || !apps->GetDictionaryWithoutPathExpansion(app_id, &app))
    return base::Time();

  std::string install_time_str;
  if (!app->GetString(kInstallTime, &install_time_str))
    return base::Time();

  int64_t install_time_i64;
  if (!base::StringToInt64(install_time_str, &install_time_i64))
    return base::Time();
  return base::Time::FromInternalValue(install_time_i64);
}

void ArcAppListPrefs::InstallIcon(const std::string& app_id,
                                  ui::ScaleFactor scale_factor,
                                  const std::vector<uint8_t>& content_png) {
  base::FilePath icon_path = GetIconPath(app_id, scale_factor);
  base::PostTaskAndReplyWithResult(
      content::BrowserThread::GetBlockingPool(), FROM_HERE,
      base::Bind(&InstallIconFromFileThread, app_id, scale_factor, icon_path,
                 content_png),
      base::Bind(&ArcAppListPrefs::OnIconInstalled,
                 weak_ptr_factory_.GetWeakPtr(), app_id, scale_factor));
}

void ArcAppListPrefs::OnIconInstalled(const std::string& app_id,
                                      ui::ScaleFactor scale_factor,
                                      bool install_succeed) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!install_succeed)
    return;

  for (auto& observer : observer_list_)
    observer.OnAppIconUpdated(app_id, scale_factor);
}

ArcAppListPrefs::AppInfo::AppInfo(const std::string& name,
                                  const std::string& package_name,
                                  const std::string& activity,
                                  const std::string& intent_uri,
                                  const std::string& icon_resource_id,
                                  const base::Time& last_launch_time,
                                  const base::Time& install_time,
                                  bool sticky,
                                  bool notifications_enabled,
                                  bool ready,
                                  bool showInLauncher,
                                  bool shortcut,
                                  bool launchable,
                                  arc::mojom::OrientationLock orientation_lock)
    : name(name),
      package_name(package_name),
      activity(activity),
      intent_uri(intent_uri),
      icon_resource_id(icon_resource_id),
      last_launch_time(last_launch_time),
      install_time(install_time),
      sticky(sticky),
      notifications_enabled(notifications_enabled),
      ready(ready),
      showInLauncher(showInLauncher),
      shortcut(shortcut),
      launchable(launchable),
      orientation_lock(orientation_lock) {}

// Need to add explicit destructor for chromium style checker error:
// Complex class/struct needs an explicit out-of-line destructor
ArcAppListPrefs::AppInfo::~AppInfo() {}

ArcAppListPrefs::PackageInfo::PackageInfo(const std::string& package_name,
                                          int32_t package_version,
                                          int64_t last_backup_android_id,
                                          int64_t last_backup_time,
                                          bool should_sync,
                                          bool system)
    : package_name(package_name),
      package_version(package_version),
      last_backup_android_id(last_backup_android_id),
      last_backup_time(last_backup_time),
      should_sync(should_sync),
      system(system) {}
