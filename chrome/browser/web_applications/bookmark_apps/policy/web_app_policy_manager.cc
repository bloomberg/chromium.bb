// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/bookmark_apps/policy/web_app_policy_manager.h"

#include <vector>

#include "base/bind.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/bookmark_apps/policy/web_app_policy_constants.h"
#include "chrome/browser/web_applications/extensions/pending_bookmark_app_manager.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#endif  // OS_CHROMEOS

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

// static
bool WebAppPolicyManager::ShouldEnableForProfile(Profile* profile) {
// PolicyBrowserTests applies test policies to all profiles, including the
// sign-in profile. This causes tests to become flaky since the tests could
// finish before, during, or after the policy apps fail to install in the
// sign-in profile. So we temporarily add a guard to ignore the policy for the
// sign-in profile.
// TODO(crbug.com/876705): Remove once the policy no longer applies to the
// sign-in profile during tests.
#if defined(OS_CHROMEOS)
  return !chromeos::ProfileHelper::IsSigninProfile(profile);
#else  // !OS_CHROMEOS
  return true;
#endif
}

void WebAppPolicyManager::RefreshPolicyInstalledApps() {
  const base::Value* web_apps =
      pref_service_->GetList(prefs::kWebAppInstallForceList);

  std::vector<PendingAppManager::AppInfo> apps_to_install;
  for (const base::Value& info : web_apps->GetList()) {
    const base::Value& url = *info.FindKey(kUrlKey);
    const base::Value* launch_container = info.FindKey(kLaunchContainerKey);

    DCHECK(!launch_container ||
           launch_container->GetString() == kLaunchContainerWindowValue ||
           launch_container->GetString() == kLaunchContainerTabValue);

    PendingAppManager::LaunchContainer container;

    // TODO(crbug.com/864904): Remove this default once PendingAppManager
    // supports not setting the launch container.
    if (!launch_container)
      container = PendingAppManager::LaunchContainer::kTab;
    else if (launch_container->GetString() == kLaunchContainerWindowValue)
      container = PendingAppManager::LaunchContainer::kWindow;
    else
      container = PendingAppManager::LaunchContainer::kTab;

    apps_to_install.emplace_back(GURL(url.GetString()), container);
  }
  pending_app_manager_->InstallApps(std::move(apps_to_install),
                                    base::DoNothing());
}

}  // namespace web_app
