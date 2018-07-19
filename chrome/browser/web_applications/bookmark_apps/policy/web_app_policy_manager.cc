// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/bookmark_apps/policy/web_app_policy_manager.h"

#include <utility>
#include <vector>

#include "base/values.h"
#include "chrome/browser/web_applications/bookmark_apps/policy/web_app_policy_constants.h"
#include "chrome/browser/web_applications/extensions/pending_bookmark_app_manager.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

namespace web_app {

WebAppPolicyManager::AppInfo::AppInfo(GURL url,
                                      LaunchContainer launch_container)
    : url(std::move(url)), launch_container(launch_container) {}

WebAppPolicyManager::AppInfo::AppInfo(AppInfo&& other) = default;

WebAppPolicyManager::AppInfo::~AppInfo() = default;

bool WebAppPolicyManager::AppInfo::operator==(const AppInfo& other) const {
  return std::tie(url, launch_container) ==
         std::tie(other.url, other.launch_container);
}

WebAppPolicyManager::WebAppPolicyManager(
    PrefService* pref_service,
    std::unique_ptr<PendingAppManager> pending_app_manager)
    : pref_service_(pref_service),
      pending_app_manager_(std::move(pending_app_manager)) {
  pending_app_manager_->ProcessAppOperations(GetAppsToInstall());
}

WebAppPolicyManager::WebAppPolicyManager(PrefService* pref_service)
    : WebAppPolicyManager(
          pref_service,
          std::make_unique<extensions::PendingBookmarkAppManager>()) {}

WebAppPolicyManager::~WebAppPolicyManager() = default;

std::vector<WebAppPolicyManager::AppInfo>
WebAppPolicyManager::GetAppsToInstall() {
  const base::Value* web_apps =
      pref_service_->GetList(prefs::kWebAppInstallForceList);

  std::vector<AppInfo> apps_to_install;
  for (const base::Value& info : web_apps->GetList()) {
    const base::Value& url = *info.FindKey(kUrlKey);
    const base::Value& launch_container = *info.FindKey(kLaunchContainerKey);

    DCHECK(launch_container.GetString() == kLaunchContainerWindowValue ||
           launch_container.GetString() == kLaunchContainerTabValue);

    apps_to_install.emplace_back(
        GURL(url.GetString()),
        launch_container.GetString() == kLaunchContainerWindowValue
            ? LaunchContainer::kWindow
            : LaunchContainer::kTab);
  }
  return apps_to_install;
}

WebAppPolicyManager::PendingAppManager::PendingAppManager() = default;

WebAppPolicyManager::PendingAppManager::~PendingAppManager() = default;

}  // namespace web_app
