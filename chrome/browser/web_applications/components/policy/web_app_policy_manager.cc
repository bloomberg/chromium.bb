// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/policy/web_app_policy_manager.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/task/post_task.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/install_options.h"
#include "chrome/browser/web_applications/components/policy/web_app_policy_constants.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace web_app {

namespace {

InstallOptions GetInstallOptionsForPolicyEntry(const base::Value& entry) {
  const base::Value& url = *entry.FindKey(kUrlKey);
  const base::Value* default_launch_container =
      entry.FindKey(kDefaultLaunchContainerKey);
  const base::Value* create_desktop_shortcut =
      entry.FindKey(kCreateDesktopShorcutKey);

  DCHECK(!default_launch_container ||
         default_launch_container->GetString() ==
             kDefaultLaunchContainerWindowValue ||
         default_launch_container->GetString() ==
             kDefaultLaunchContainerTabValue);

  LaunchContainer launch_container;
  if (!default_launch_container) {
    launch_container = LaunchContainer::kTab;
  } else if (default_launch_container->GetString() ==
             kDefaultLaunchContainerTabValue) {
    launch_container = LaunchContainer::kTab;
  } else {
    launch_container = LaunchContainer::kWindow;
  }

  InstallOptions install_options(GURL(url.GetString()), launch_container,
                                 web_app::InstallSource::kExternalPolicy);

  bool create_shortcut = false;
  if (create_desktop_shortcut)
    create_shortcut = create_desktop_shortcut->GetBool();

  install_options.add_to_applications_menu = create_shortcut;
  install_options.add_to_desktop = create_shortcut;

  // It's not yet clear how pinning to shelf will work for policy installed
  // Web Apps, but for now never pin them. See crbug.com/880125.
  install_options.add_to_quick_launch_bar = false;

  install_options.install_placeholder = true;

  return install_options;
}

}  // namespace
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

void WebAppPolicyManager::ReinstallPlaceholderAppIfNecessary(const GURL& url) {
  const base::Value* web_apps =
      pref_service_->GetList(prefs::kWebAppInstallForceList);
  const auto& web_apps_list = web_apps->GetList();

  const auto it =
      std::find_if(web_apps_list.begin(), web_apps_list.end(),
                   [&url](const base::Value& entry) {
                     return entry.FindKey(kUrlKey)->GetString() == url.spec();
                   });

  if (it == web_apps_list.end())
    return;

  auto install_options = GetInstallOptionsForPolicyEntry(*it);
  install_options.install_placeholder = false;
  install_options.reinstall_placeholder = true;
  install_options.stop_if_window_opened = true;

  // If the app is not a placeholder app, PendingAppManager will ignore the
  // request.
  pending_app_manager_->Install(std::move(install_options), base::DoNothing());
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
  for (const base::Value& entry : web_apps->GetList()) {
    install_options_list.push_back(GetInstallOptionsForPolicyEntry(entry));
  }

  pending_app_manager_->SynchronizeInstalledApps(
      std::move(install_options_list), InstallSource::kExternalPolicy,
      base::DoNothing());
}

}  // namespace web_app
