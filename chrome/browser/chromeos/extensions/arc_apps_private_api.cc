// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/arc_apps_private_api.h"

#include <string>
#include <vector>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/common/extensions/api/arc_apps_private.h"
#include "ui/events/event_constants.h"

ArcAppsPrivateGetLaunchableAppsFunction::
    ArcAppsPrivateGetLaunchableAppsFunction() = default;

ArcAppsPrivateGetLaunchableAppsFunction::
    ~ArcAppsPrivateGetLaunchableAppsFunction() = default;

ExtensionFunction::ResponseAction
ArcAppsPrivateGetLaunchableAppsFunction::Run() {
  ArcAppListPrefs* prefs =
      ArcAppListPrefs::Get(Profile::FromBrowserContext(browser_context()));
  if (!prefs)
    return RespondNow(Error("Not available"));

  std::vector<extensions::api::arc_apps_private::AppInfo> result;
  const std::vector<std::string> app_ids = prefs->GetAppIds();
  for (const auto& app_id : app_ids) {
    std::unique_ptr<ArcAppListPrefs::AppInfo> app_info = prefs->GetApp(app_id);
    if (app_info && app_info->launchable) {
      extensions::api::arc_apps_private::AppInfo app_info_result;
      app_info_result.id = app_id;
      result.push_back(std::move(app_info_result));
    }
  }
  return RespondNow(ArgumentList(
      extensions::api::arc_apps_private::GetLaunchableApps::Results::Create(
          result)));
}

ArcAppsPrivateLaunchAppFunction::ArcAppsPrivateLaunchAppFunction() = default;

ArcAppsPrivateLaunchAppFunction::~ArcAppsPrivateLaunchAppFunction() = default;

ExtensionFunction::ResponseAction ArcAppsPrivateLaunchAppFunction::Run() {
  std::unique_ptr<extensions::api::arc_apps_private::LaunchApp::Params> params(
      extensions::api::arc_apps_private::LaunchApp::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  if (!ArcAppListPrefs::Get(Profile::FromBrowserContext(browser_context())))
    return RespondNow(Error("Not available"));

  if (!arc::LaunchApp(
          browser_context(), params->app_id, ui::EF_NONE,
          arc::UserInteractionType::APP_STARTED_FROM_EXTENSION_API)) {
    return RespondNow(Error("Launch failed"));
  }
  return RespondNow(NoArguments());
}