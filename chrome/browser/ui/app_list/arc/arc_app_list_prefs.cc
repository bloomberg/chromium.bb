// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"

#include "base/files/file_util.h"
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
const char kPackage[] = "package";
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
                               const std::vector<uint8>& content_png) {
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

}  // namespace

// static
ArcAppListPrefs* ArcAppListPrefs::Create(const base::FilePath& base_path,
                                         PrefService* prefs) {
  return new ArcAppListPrefs(base_path, prefs);
}

// static
void ArcAppListPrefs::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(
      prefs::kArcApps,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
}

// static
ArcAppListPrefs* ArcAppListPrefs::Get(content::BrowserContext* context) {
  return ArcAppListPrefsFactory::GetInstance()->GetForBrowserContext(context);
}

// static
std::string ArcAppListPrefs::GetAppId(const std::string& package,
                                      const std::string& activity) {
  std::string input = package + "#" + activity;
  return crx_file::id_util::GenerateId(input);
}

ArcAppListPrefs::ArcAppListPrefs(const base::FilePath& base_path,
                                 PrefService* prefs)
    : prefs_(prefs),
      weak_ptr_factory_(this) {
  base_path_ = base_path.AppendASCII(prefs::kArcApps);

  arc::ArcBridgeService* bridge_service = arc::ArcBridgeService::Get();
  if (!bridge_service) {
    VLOG(2) << "Bridge service is not available. Cannot init.";
    return;
  }

  bridge_service->AddObserver(this);
  bridge_service->AddAppObserver(this);
  OnStateChanged(bridge_service->state());
}

ArcAppListPrefs::~ArcAppListPrefs() {
  arc::ArcBridgeService* bridge_service = arc::ArcBridgeService::Get();
  if (bridge_service) {
    bridge_service->RemoveAppObserver(this);
    bridge_service->RemoveObserver(this);
  }
}

base::FilePath ArcAppListPrefs::GetIconPath(
    const std::string& app_id,
    ui::ScaleFactor scale_factor) const {
  base::FilePath app_path = base_path_.AppendASCII(app_id);
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
  if (!bridge_service ||
      bridge_service->state() != arc::ArcBridgeService::State::READY) {
    VLOG(2) << "Request to load icon when bridge service is not ready: "
            <<  app_id << ".";
    return;
  }

  scoped_ptr<AppInfo> app_info = GetApp(app_id);
  if (!app_info) {
    VLOG(2) << "Failed to get app info: " <<  app_id << ".";
    return;
  }

  bridge_service->RequestAppIcon(app_info->package,
                                 app_info->activity,
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

  // crx_file::id_util is de-factor utility for id generation.
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
  std::string package;
  std::string activity;
  app->GetString(kName, &name);
  app->GetString(kPackage, &package);
  app->GetString(kActivity, &activity);
  scoped_ptr<AppInfo> app_info(new AppInfo(name,
                                           package,
                                           activity,
                                           ready_apps_.count(app_id) > 0));

  return app_info.Pass();
}

bool ArcAppListPrefs::IsRegistered(const std::string& app_id) {
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
  if (state == arc::ArcBridgeService::State::READY)
    arc::ArcBridgeService::Get()->RefreshAppList();
  else
    DisableAllApps();
}

void ArcAppListPrefs::OnAppReady(const arc::AppInfo& app) {
  if (app.name.empty() || app.package.empty() || app.activity.empty()) {
    VLOG(2) << "Name, package and activity cannot be empty.";
    return;
  }
  std::string app_id = GetAppId(app.package, app.activity);
  bool was_registered = IsRegistered(app_id);

  ScopedArcAppListPrefUpdate update(prefs_, app_id);
  base::DictionaryValue* app_dict = update.Get();
  app_dict->SetString(kName, app.name);
  app_dict->SetString(kPackage, app.package);
  app_dict->SetString(kActivity, app.activity);

  // From now, app is ready.
  if (!ready_apps_.count(app_id))
    ready_apps_.insert(app_id);

  if (was_registered) {
    FOR_EACH_OBSERVER(Observer,
                      observer_list_,
                      OnAppReadyChanged(app_id, true));
  } else {
    AppInfo app_info(app.name, app.package, app.activity, true);
    FOR_EACH_OBSERVER(Observer,
                      observer_list_,
                      OnAppRegistered(app_id, app_info));
  }

  std::map<std::string, uint32>::iterator deferred_icons =
      request_icon_deferred_.find(app_id);
  if (deferred_icons != request_icon_deferred_.end()) {
    for (uint32 i = ui::SCALE_FACTOR_100P; i < ui::NUM_SCALE_FACTORS; ++i) {
      if (deferred_icons->second & (1 << i)) {
         RequestIcon(app_id, static_cast<ui::ScaleFactor>(i));
      }
    }
    request_icon_deferred_.erase(deferred_icons);
  }
}

void ArcAppListPrefs::OnAppListRefreshed(
    const std::vector<arc::AppInfo>& apps) {
  std::set<std::string> old_ready_apps;
  old_ready_apps.swap(ready_apps_);

  for (auto& app : apps)
    OnAppReady(app);

  // Detect unavailable apps after current refresh.
  for (auto& app_id : old_ready_apps) {
    if (!ready_apps_.count(app_id)) {
      FOR_EACH_OBSERVER(Observer,
                        observer_list_,
                        OnAppReadyChanged(app_id, false));
    }
  }
}

void ArcAppListPrefs::OnAppIcon(const std::string& package,
                                const std::string& activity,
                                arc::ScaleFactor scale_factor,
                                const std::vector<uint8_t>& icon_png_data) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!icon_png_data.empty());
  DCHECK(scale_factor >= arc::SCALE_FACTOR_100P &&
         scale_factor < arc::NUM_SCALE_FACTORS);

  std::string app_id = GetAppId(package, activity);
  if (!IsRegistered(app_id)) {
    VLOG(2) << "Request to update icon for non-registered app: " << app_id;
    return;
  }

  InstallIcon(app_id,
              static_cast<ui::ScaleFactor>(scale_factor),
              icon_png_data);
}


void ArcAppListPrefs::InstallIcon(const std::string& app_id,
                                  ui::ScaleFactor scale_factor,
                                  const std::vector<uint8>& content_png) {

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
                                  const std::string& package,
                                  const std::string& activity,
                                  bool ready)
    : name(name),
      package(package),
      activity(activity),
      ready(ready) {
}
