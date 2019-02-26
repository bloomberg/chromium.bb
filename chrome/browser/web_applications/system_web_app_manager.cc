// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/system_web_app_manager.h"

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/stl_util.h"
#include "base/task/post_task.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace web_app {

namespace {

base::flat_map<SystemAppType, GURL> CreateSystemWebApps() {
  base::flat_map<SystemAppType, GURL> urls;

// TODO(calamity): Split this into per-platform functions.
#if defined(OS_CHROMEOS)
  urls[SystemAppType::DISCOVER] = GURL(chrome::kChromeUIDiscoverURL);
  constexpr char kChromeSettingsPWAURL[] = "chrome://settings/pwa.html";
  urls[SystemAppType::SETTINGS] = GURL(kChromeSettingsPWAURL);
#endif  // OS_CHROMEOS

  return urls;
}

PendingAppManager::AppInfo CreateAppInfoForSystemApp(const GURL& url) {
  DCHECK_EQ(content::kChromeUIScheme, url.scheme());

  web_app::PendingAppManager::AppInfo app_info(url, LaunchContainer::kWindow,
                                               InstallSource::kSystemInstalled);
  app_info.create_shortcuts = false;
  app_info.bypass_service_worker_check = true;
  app_info.always_update = true;
  return app_info;
}

}  // namespace

SystemWebAppManager::SystemWebAppManager(Profile* profile,
                                         PendingAppManager* pending_app_manager)
    : pending_app_manager_(pending_app_manager) {
  system_app_urls_ = CreateSystemWebApps();
}

SystemWebAppManager::~SystemWebAppManager() = default;

void SystemWebAppManager::Start() {
  content::BrowserThread::PostAfterStartupTask(
      FROM_HERE,
      base::CreateSingleThreadTaskRunnerWithTraits(
          {content::BrowserThread::UI}),
      base::BindOnce(&SystemWebAppManager::StartAppInstallation,
                     weak_ptr_factory_.GetWeakPtr()));
}

base::Optional<std::string> SystemWebAppManager::GetAppIdForSystemApp(
    SystemAppType id) const {
  auto app = system_app_urls_.find(id);
  DCHECK(app != system_app_urls_.end());
  return pending_app_manager_->LookupAppId(app->second);
}

void SystemWebAppManager::SetSystemAppsForTesting(
    base::flat_map<SystemAppType, GURL> system_app_urls) {
  system_app_urls_ = std::move(system_app_urls);
}

// static
bool SystemWebAppManager::IsEnabled() {
  return base::FeatureList::IsEnabled(features::kSystemWebApps);
}

void SystemWebAppManager::StartAppInstallation() {
  std::vector<PendingAppManager::AppInfo> apps_to_install;
  if (IsEnabled()) {
    // Skipping this will uninstall all System Apps currently installed.
    for (const auto& app : system_app_urls_)
      apps_to_install.push_back(CreateAppInfoForSystemApp(app.second));
  }

  pending_app_manager_->SynchronizeInstalledApps(
      std::move(apps_to_install), InstallSource::kSystemInstalled);
}

}  // namespace web_app
