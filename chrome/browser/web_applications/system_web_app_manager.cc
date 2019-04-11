// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/system_web_app_manager.h"

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/stl_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"

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

InstallOptions CreateInstallOptionsForSystemApp(const GURL& url) {
  DCHECK_EQ(content::kChromeUIScheme, url.scheme());

  web_app::InstallOptions install_options(url, LaunchContainer::kWindow,
                                          InstallSource::kSystemInstalled);
  install_options.add_to_applications_menu = false;
  install_options.add_to_desktop = false;
  install_options.add_to_quick_launch_bar = false;
  install_options.bypass_service_worker_check = true;
  install_options.always_update = true;
  return install_options;
}

}  // namespace

SystemWebAppManager::SystemWebAppManager(Profile* profile,
                                         PendingAppManager* pending_app_manager)
    : pending_app_manager_(pending_app_manager) {
  system_app_urls_ = CreateSystemWebApps();
}

SystemWebAppManager::~SystemWebAppManager() = default;

void SystemWebAppManager::Start() {
  std::vector<InstallOptions> install_options_list;
  if (IsEnabled()) {
    // Skipping this will uninstall all System Apps currently installed.
    for (const auto& app : system_app_urls_) {
      install_options_list.push_back(
          CreateInstallOptionsForSystemApp(app.second));
    }
  }

  pending_app_manager_->SynchronizeInstalledApps(
      std::move(install_options_list), InstallSource::kSystemInstalled,
      base::DoNothing());
}

base::Optional<std::string> SystemWebAppManager::GetAppIdForSystemApp(
    SystemAppType id) const {
  auto app = system_app_urls_.find(id);
  DCHECK(app != system_app_urls_.end());
  return pending_app_manager_->LookupAppId(app->second);
}

bool SystemWebAppManager::IsSystemWebApp(const AppId& app_id) const {
  return pending_app_manager_->HasAppIdWithInstallSource(
      app_id, InstallSource::kSystemInstalled);
}

void SystemWebAppManager::SetSystemAppsForTesting(
    base::flat_map<SystemAppType, GURL> system_app_urls) {
  system_app_urls_ = std::move(system_app_urls);
}

// static
bool SystemWebAppManager::IsEnabled() {
  return base::FeatureList::IsEnabled(features::kSystemWebApps);
}

}  // namespace web_app
