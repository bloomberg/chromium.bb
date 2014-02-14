// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/recommended_apps.h"

#include <algorithm>
#include <vector>

#include "base/bind.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/install_tracker.h"
#include "chrome/browser/extensions/install_tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/recommended_apps_observer.h"
#include "chrome/common/pref_names.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_system_provider.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/pref_names.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"

namespace app_list {

namespace {

struct AppSortInfo {
  AppSortInfo() : app(NULL) {}
  AppSortInfo(const extensions::Extension* app,
              const base::Time& last_launch_time)
      : app(app), last_launch_time(last_launch_time) {}

  const extensions::Extension* app;
  base::Time last_launch_time;
};

bool AppLaunchedMoreRecent(const AppSortInfo& app1, const AppSortInfo& app2) {
  return app1.last_launch_time > app2.last_launch_time;
}

}  // namespace

RecommendedApps::RecommendedApps(Profile* profile) : profile_(profile) {
  extensions::InstallTrackerFactory::GetForProfile(profile_)->AddObserver(this);

  ExtensionService* service =
      extensions::ExtensionSystem::Get(profile_)->extension_service();
  extensions::ExtensionPrefs* prefs = service->extension_prefs();
  pref_change_registrar_.Init(prefs->pref_service());
  pref_change_registrar_.Add(extensions::pref_names::kExtensions,
                             base::Bind(&RecommendedApps::Update,
                                        base::Unretained(this)));

  Update();
}

RecommendedApps::~RecommendedApps() {
  extensions::InstallTrackerFactory::GetForProfile(profile_)
      ->RemoveObserver(this);
}

void RecommendedApps::AddObserver(RecommendedAppsObserver* observer) {
  observers_.AddObserver(observer);
}

void RecommendedApps::RemoveObserver(RecommendedAppsObserver* observer) {
  observers_.RemoveObserver(observer);
}

void RecommendedApps::Update() {
  ExtensionService* service =
      extensions::ExtensionSystem::Get(profile_)->extension_service();
  extensions::ExtensionPrefs* prefs = service->extension_prefs();

  std::vector<AppSortInfo> sorted_apps;
  const extensions::ExtensionSet* extensions = service->extensions();
  for (extensions::ExtensionSet::const_iterator app = extensions->begin();
       app != extensions->end(); ++app) {
    if (!(*app)->ShouldDisplayInAppLauncher())
      continue;

    sorted_apps.push_back(
        AppSortInfo(app->get(), prefs->GetLastLaunchTime((*app)->id())));
  }

  std::sort(sorted_apps.begin(), sorted_apps.end(), &AppLaunchedMoreRecent);

  const size_t kMaxRecommendedApps = 4;
  sorted_apps.resize(std::min(kMaxRecommendedApps, sorted_apps.size()));

  Apps new_recommends;
  for (size_t i = 0; i < sorted_apps.size(); ++i)
    new_recommends.push_back(sorted_apps[i].app);

  const bool changed = apps_.size() != new_recommends.size() ||
      !std::equal(apps_.begin(), apps_.end(), new_recommends.begin());
  if (changed) {
    apps_.swap(new_recommends);
    FOR_EACH_OBSERVER(
        RecommendedAppsObserver, observers_, OnRecommendedAppsChanged());
  }
}

void RecommendedApps::OnBeginExtensionInstall(
    const ExtensionInstallParams& params) {}

void RecommendedApps::OnDownloadProgress(const std::string& extension_id,
                                int percent_downloaded) {}

void RecommendedApps::OnInstallFailure(const std::string& extension_id) {}

void RecommendedApps::OnExtensionInstalled(
    const extensions::Extension* extension) {
  Update();
}

void RecommendedApps::OnExtensionLoaded(
    const extensions::Extension* extension) {
  Update();
}

void RecommendedApps::OnExtensionUnloaded(
    const extensions::Extension* extension) {
  Update();
}

void RecommendedApps::OnExtensionUninstalled(
    const extensions::Extension* extension) {
  Update();
}

void RecommendedApps::OnAppsReordered() {}

void RecommendedApps::OnAppInstalledToAppList(
    const std::string& extension_id) {}

void RecommendedApps::OnShutdown() {}

}  // namespace app_list
