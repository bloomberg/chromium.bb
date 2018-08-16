// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/bookmark_apps/policy/web_app_policy_manager.h"

#include <vector>

#include "base/bind.h"
#include "base/values.h"
#include "chrome/browser/web_applications/bookmark_apps/policy/web_app_policy_constants.h"
#include "chrome/browser/web_applications/extensions/pending_bookmark_app_manager.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"

namespace web_app {

WebAppPolicyManager::WebAppPolicyManager(PrefService* pref_service,
                                         PendingAppManager* pending_app_manager)
    : pref_service_(pref_service), pending_app_manager_(pending_app_manager) {
  content::BrowserThread::PostAfterStartupTask(
      FROM_HERE,
      content::BrowserThread::GetTaskRunnerForThread(
          content::BrowserThread::UI),
      base::BindOnce(&WebAppPolicyManager::RefreshPolicyInstalledApps,
                     weak_ptr_factory_.GetWeakPtr()));
}

WebAppPolicyManager::~WebAppPolicyManager() = default;

// static
void WebAppPolicyManager::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterListPref(prefs::kWebAppInstallForceList);
}

void WebAppPolicyManager::RefreshPolicyInstalledApps() {
  const base::Value* web_apps =
      pref_service_->GetList(prefs::kWebAppInstallForceList);

  std::vector<PendingAppManager::AppInfo> apps_to_install;
  for (const base::Value& info : web_apps->GetList()) {
    const base::Value& url = *info.FindKey(kUrlKey);
    const base::Value& launch_container = *info.FindKey(kLaunchContainerKey);

    DCHECK(launch_container.GetString() == kLaunchContainerWindowValue ||
           launch_container.GetString() == kLaunchContainerTabValue);

    apps_to_install.emplace_back(
        GURL(url.GetString()),
        launch_container.GetString() == kLaunchContainerWindowValue
            ? PendingAppManager::LaunchContainer::kWindow
            : PendingAppManager::LaunchContainer::kTab);
  }
  pending_app_manager_->ProcessAppOperations(std::move(apps_to_install));
}

}  // namespace web_app
