// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/policy/web_app_policy_manager.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/task/post_task.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/policy/web_app_policy_constants.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace web_app {

WebAppPolicyManager::WebAppPolicyManager(Profile* profile,
                                         PendingAppManager* pending_app_manager)
    : profile_(profile),
      pref_service_(profile_->GetPrefs()),
      pending_app_manager_(pending_app_manager) {}

WebAppPolicyManager::~WebAppPolicyManager() = default;

void WebAppPolicyManager::Start() {
  content::BrowserThread::PostAfterStartupTask(
      FROM_HERE,
      base::CreateSingleThreadTaskRunnerWithTraits(
          {content::BrowserThread::UI}),
      base::BindOnce(&WebAppPolicyManager::
                         InitChangeRegistrarAndRefreshPolicyInstalledApps,
                     weak_ptr_factory_.GetWeakPtr()));
}

// static
void WebAppPolicyManager::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterListPref(prefs::kWebAppInstallForceList);
}

void WebAppPolicyManager::InitChangeRegistrarAndRefreshPolicyInstalledApps() {
  pref_change_registrar_.Init(pref_service_);
  pref_change_registrar_.Add(
      prefs::kWebAppInstallForceList,
      base::BindRepeating(&WebAppPolicyManager::RefreshPolicyInstalledApps,
                          weak_ptr_factory_.GetWeakPtr()));

  RefreshPolicyInstalledApps();
}

void WebAppPolicyManager::RefreshPolicyInstalledApps() {
  const base::Value* web_apps =
      pref_service_->GetList(prefs::kWebAppInstallForceList);
  std::vector<InstallOptions> install_options_list;
  // No need to validate the types or values of the policy members because we
  // are using a SimpleSchemaValidatingPolicyHandler which should validate them
  // for us.
  for (const base::Value& info : web_apps->GetList()) {
    const base::Value& url = *info.FindKey(kUrlKey);
    const base::Value* default_launch_container =
        info.FindKey(kDefaultLaunchContainerKey);
    const base::Value* create_desktop_shortcut =
        info.FindKey(kCreateDesktopShorcutKey);

    DCHECK(!default_launch_container ||
           default_launch_container->GetString() ==
               kDefaultLaunchContainerWindowValue ||
           default_launch_container->GetString() ==
               kDefaultLaunchContainerTabValue);

    LaunchContainer launch_container;
    if (!default_launch_container)
      launch_container = LaunchContainer::kTab;
    else if (default_launch_container->GetString() ==
             kDefaultLaunchContainerTabValue)
      launch_container = LaunchContainer::kTab;
    else
      launch_container = LaunchContainer::kWindow;

    InstallOptions install_options(GURL(std::move(url.GetString())),
                                   launch_container,
                                   web_app::InstallSource::kExternalPolicy);

    bool create_shortcut = false;
    if (create_desktop_shortcut)
      create_shortcut = create_desktop_shortcut->GetBool();

    // This currently pins the app to the shelf on Chrome OS, which we don't
    // want to do because there is a separate policy for that.
    // TODO(ortuno): Introduce an option to specifically create desktop
    // shortcuts and not pin the app to the shelf.
    install_options.create_shortcuts = create_shortcut;

    install_options_list.push_back(std::move(install_options));
  }

  pending_app_manager_->SynchronizeInstalledApps(
      std::move(install_options_list), InstallSource::kExternalPolicy);
}

}  // namespace web_app
