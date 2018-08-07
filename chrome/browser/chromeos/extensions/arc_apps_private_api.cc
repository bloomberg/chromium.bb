// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/arc_apps_private_api.h"

#include <string>
#include <vector>

#include "base/logging.h"
#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/common/extensions/api/arc_apps_private.h"
#include "ui/events/event_constants.h"

namespace extensions {

// static
BrowserContextKeyedAPIFactory<ArcAppsPrivateAPI>*
ArcAppsPrivateAPI::GetFactoryInstance() {
  static base::NoDestructor<BrowserContextKeyedAPIFactory<ArcAppsPrivateAPI>>
      instance;
  return instance.get();
}

ArcAppsPrivateAPI::ArcAppsPrivateAPI(content::BrowserContext* context)
    : context_(context), scoped_prefs_observer_(this) {
  EventRouter::Get(context_)->RegisterObserver(
      this, api::arc_apps_private::OnInstalled::kEventName);
}

ArcAppsPrivateAPI::~ArcAppsPrivateAPI() = default;

void ArcAppsPrivateAPI::Shutdown() {
  EventRouter::Get(context_)->UnregisterObserver(this);
  scoped_prefs_observer_.RemoveAll();
}

void ArcAppsPrivateAPI::OnListenerAdded(const EventListenerInfo& details) {
  DCHECK_EQ(details.event_name, api::arc_apps_private::OnInstalled::kEventName);
  auto* prefs = ArcAppListPrefs::Get(Profile::FromBrowserContext(context_));
  if (prefs && !scoped_prefs_observer_.IsObserving(prefs))
    scoped_prefs_observer_.Add(prefs);
}

void ArcAppsPrivateAPI::OnListenerRemoved(const EventListenerInfo& details) {
  if (!EventRouter::Get(context_)->HasEventListener(
          api::arc_apps_private::OnInstalled::kEventName)) {
    scoped_prefs_observer_.RemoveAll();
  }
}

void ArcAppsPrivateAPI::OnAppRegistered(
    const std::string& app_id,
    const ArcAppListPrefs::AppInfo& app_info) {
  if (!app_info.launchable)
    return;
  api::arc_apps_private::AppInfo app_info_result;
  app_info_result.id = app_id;
  auto event = std::make_unique<Event>(
      events::ARC_APPS_PRIVATE_ON_INSTALLED,
      api::arc_apps_private::OnInstalled::kEventName,
      api::arc_apps_private::OnInstalled::Create(app_info_result), context_);
  EventRouter::Get(context_)->BroadcastEvent(std::move(event));
}

ArcAppsPrivateGetLaunchableAppsFunction::
    ArcAppsPrivateGetLaunchableAppsFunction() = default;

ArcAppsPrivateGetLaunchableAppsFunction::
    ~ArcAppsPrivateGetLaunchableAppsFunction() = default;

ExtensionFunction::ResponseAction
ArcAppsPrivateGetLaunchableAppsFunction::Run() {
  auto* prefs =
      ArcAppListPrefs::Get(Profile::FromBrowserContext(browser_context()));
  if (!prefs)
    return RespondNow(Error("Not available"));

  std::vector<api::arc_apps_private::AppInfo> result;
  const std::vector<std::string> app_ids = prefs->GetAppIds();
  for (const auto& app_id : app_ids) {
    std::unique_ptr<ArcAppListPrefs::AppInfo> app_info = prefs->GetApp(app_id);
    if (app_info && app_info->launchable) {
      api::arc_apps_private::AppInfo app_info_result;
      app_info_result.id = app_id;
      result.push_back(std::move(app_info_result));
    }
  }
  return RespondNow(ArgumentList(
      api::arc_apps_private::GetLaunchableApps::Results::Create(result)));
}

ArcAppsPrivateLaunchAppFunction::ArcAppsPrivateLaunchAppFunction() = default;

ArcAppsPrivateLaunchAppFunction::~ArcAppsPrivateLaunchAppFunction() = default;

ExtensionFunction::ResponseAction ArcAppsPrivateLaunchAppFunction::Run() {
  std::unique_ptr<api::arc_apps_private::LaunchApp::Params> params(
      api::arc_apps_private::LaunchApp::Params::Create(*args_));
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

}  // namespace extensions
