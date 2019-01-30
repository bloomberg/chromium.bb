// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_launch_event_logger.h"

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/time/time.h"
#include "chrome/browser/metrics/chrome_metrics_service_client.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/arc/arc_prefs.h"
#include "components/prefs/pref_service.h"
#include "components/ukm/app_source_url_recorder.h"
#include "extensions/common/extension.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "url/gurl.h"

namespace app_list {

const base::Feature kUkmAppLaunchEventLogging{"UkmAppLaunchEventLogging",
                                              base::FEATURE_ENABLED_BY_DEFAULT};

// Keys for Arc app specific preferences. Defined in
// chrome/browser/ui/app_list/arc/arc_app_list_prefs.cc.
const char AppLaunchEventLogger::kPackageName[] = "package_name";
const char AppLaunchEventLogger::kShouldSync[] = "should_sync";

namespace {

const char kArcScheme[] = "arc://";
const char kExtensionSchemeWithDelimiter[] = "chrome-extension://";

int HourOfDay(base::Time time) {
  base::Time::Exploded exploded;
  time.LocalExplode(&exploded);
  return exploded.hour;
}

int DayOfWeek(base::Time time) {
  base::Time::Exploded exploded;
  time.LocalExplode(&exploded);
  return exploded.day_of_week;
}

}  // namespace

AppLaunchEventLogger::AppLaunchEventLogger() : weak_factory_(this) {
  task_runner_ = base::CreateSequencedTaskRunnerWithTraits(
      {base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
  PopulatePwaIdUrlMap();
}

AppLaunchEventLogger::~AppLaunchEventLogger() {}

void AppLaunchEventLogger::OnSuggestionChipClicked(const std::string& id,
                                                   int suggestion_index) {
  if (!base::FeatureList::IsEnabled(kUkmAppLaunchEventLogging)) {
    return;
  }
  AppLaunchEvent event;
  event.set_launched_from(AppLaunchEvent_LaunchedFrom_SUGGESTED);
  event.set_app_id(RemoveScheme(id));
  event.set_index(suggestion_index);
  SetAppInfo(&event);
  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&AppLaunchEventLogger::Log,
                                        weak_factory_.GetWeakPtr(), event));
}

void AppLaunchEventLogger::OnGridClicked(const std::string& id) {
  if (!base::FeatureList::IsEnabled(kUkmAppLaunchEventLogging)) {
    return;
  }
  AppLaunchEvent event;
  event.set_launched_from(AppLaunchEvent_LaunchedFrom_GRID);
  event.set_app_id(id);
  SetAppInfo(&event);
  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&AppLaunchEventLogger::Log,
                                        weak_factory_.GetWeakPtr(), event));
}

void AppLaunchEventLogger::SetAppDataForTesting(
    extensions::ExtensionRegistry* registry,
    base::DictionaryValue* arc_apps,
    base::DictionaryValue* arc_packages) {
  testing_ = true;
  registry_ = registry;
  arc_apps_ = arc_apps;
  arc_packages_ = arc_packages;
}

std::string AppLaunchEventLogger::RemoveScheme(const std ::string& id) {
  std::string app_id(id);
  if (!app_id.compare(0, strlen(kExtensionSchemeWithDelimiter),
                      kExtensionSchemeWithDelimiter)) {
    app_id.erase(0, strlen(kExtensionSchemeWithDelimiter));
  }
  if (!app_id.compare(0, strlen(kArcScheme), kArcScheme)) {
    app_id.erase(0, strlen(kArcScheme));
  }
  if (app_id.size() && !app_id.compare(app_id.size() - 1, 1, "/")) {
    app_id.pop_back();
  }
  return app_id;
}

void AppLaunchEventLogger::PopulatePwaIdUrlMap() {
  pwa_id_url_map_ = base::flat_map<std::string, std::string>{
      // Google+
      {"dcdbodpaldbchkfinnjphocleggfceip", "https://plus.google.com/discover"},
      // Photos
      {"ncmjhecbjeaamljdfahankockkkdmedg", "https://photos.google.com/"}};
}

const std::string& AppLaunchEventLogger::GetPwaUrl(const std::string& id) {
  auto search = pwa_id_url_map_.find(id);
  if (search != pwa_id_url_map_.end()) {
    return search->second;
  }
  return base::EmptyString();
}

void AppLaunchEventLogger::SetAppInfo(AppLaunchEvent* event) {
  LoadInstalledAppInformation();
  if (IsChromeAppFromWebstore(event->app_id())) {
    event->set_app_type(AppLaunchEvent_AppType_CHROME);
    return;
  }
  const std::string& pwa_url = GetPwaUrl(event->app_id());
  if (pwa_url != base::EmptyString()) {
    event->set_app_type(AppLaunchEvent_AppType_PWA);
    event->set_pwa_url(pwa_url);
    return;
  }
  const std::string& arc_package_name(GetSyncedArcPackage(event->app_id()));
  if (arc_package_name != base::EmptyString()) {
    event->set_app_type(AppLaunchEvent_AppType_PLAY);
    event->set_arc_package_name(arc_package_name);
    return;
  }
  event->set_app_type(AppLaunchEvent_AppType_OTHER);
}

void AppLaunchEventLogger::LoadInstalledAppInformation() {
  // Tests provide installed app information, so don't overwrite that.
  if (testing_)
    return;

  Profile* profile = ProfileManager::GetLastUsedProfile();
  if (!profile) {
    LOG(DFATAL) << "No profile";
    return;
  }
  registry_ = extensions::ExtensionRegistry::Get(profile);

  PrefService* pref_service = profile->GetPrefs();
  if (pref_service) {
    arc_apps_ = pref_service->GetDictionary(arc::prefs::kArcApps);
    arc_packages_ = pref_service->GetDictionary(arc::prefs::kArcPackages);
  }
}

std::string AppLaunchEventLogger::GetSyncedArcPackage(const std::string& id) {
  if (!arc_apps_ || !arc_packages_)
    return base::EmptyString();

  const base::Value* app = arc_apps_->FindKey(id);
  if (!app) {
    // App with |id| is not in the list of installed Arc apps.
    return base::EmptyString();
  }
  const base::Value* package_name_value = app->FindKey(kPackageName);
  if (!package_name_value) {
    return base::EmptyString();
  }
  const base::Value* package =
      arc_packages_->FindKey(package_name_value->GetString());
  if (!package) {
    return base::EmptyString();
  }
  if (!package->FindKey(kShouldSync)->GetBool()) {
    return base::EmptyString();
  }
  return package_name_value->GetString();
}

bool AppLaunchEventLogger::IsChromeAppFromWebstore(const std::string& id) {
  if (!registry_)
    return false;
  const extensions::Extension* extension =
      registry_->GetExtensionById(id, extensions::ExtensionRegistry::ENABLED);
  if (!extension) {
    // App with |id| is not in the list of Chrome extensions.
    return false;
  }
  return extension->from_webstore();
}

void AppLaunchEventLogger::Log(AppLaunchEvent app_launch_event) {
  UMA_HISTOGRAM_ENUMERATION("Apps.AppListAppTypeClicked",
                            app_launch_event.app_type(),
                            AppLaunchEvent_AppType_AppType_ARRAYSIZE);

  ukm::SourceId source_id = 0;
  // SetAppInfo performed the checks to enforce logging policy. Apps that are
  // not to be logged were assigned an |app_type| of OTHER and so are not logged
  // here.
  if (app_launch_event.app_type() == AppLaunchEvent_AppType_CHROME) {
    source_id = ukm::AppSourceUrlRecorder::GetSourceIdForChromeApp(
        app_launch_event.app_id());
  } else if (app_launch_event.app_type() == AppLaunchEvent_AppType_PWA) {
    source_id = ukm::AppSourceUrlRecorder::GetSourceIdForPWA(
        GURL(app_launch_event.pwa_url()));
  } else if (app_launch_event.app_type() == AppLaunchEvent_AppType_PLAY) {
    source_id = ukm::AppSourceUrlRecorder::GetSourceIdForArc(
        app_launch_event.arc_package_name());
  } else {
    // Either app is Crostini; or Chrome but not in app store; or Arc but not
    // syncable; or PWA but not in whitelist.
    return;
  }

  ukm::builders::AppListAppLaunch app_launch(source_id);
  base::Time now(base::Time::Now());

  if (app_launch_event.launched_from() ==
      AppLaunchEvent_LaunchedFrom_SUGGESTED) {
    app_launch.SetPositionIndex(app_launch_event.index());
  }

  app_launch.SetAppType(app_launch_event.app_type())
      .SetLaunchedFrom(app_launch_event.launched_from())
      .SetDayOfWeek(DayOfWeek(now))
      .SetHourOfDay(HourOfDay(now))
      .Record(ukm::UkmRecorder::Get());
}

}  // namespace app_list
