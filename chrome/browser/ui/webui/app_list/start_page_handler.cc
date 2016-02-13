// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/app_list/start_page_handler.h"

#include <string>

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/app_list_service.h"
#include "chrome/browser/ui/app_list/start_page_service.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/webui/extensions/extension_icon_source.h"
#include "chrome/common/pref_names.h"
#include "components/update_client/update_query_params.h"
#include "content/public/browser/web_ui.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_icon_set.h"
#include "ui/app_list/app_list_switches.h"
#include "ui/events/event_constants.h"

namespace app_list {

namespace {

const char kAppListDoodleActionHistogram[] = "Apps.AppListDoodleAction";

// Interactions a user has with the app list doodle. This enum must not have its
// order altered as it is used in histograms.
enum DoodleAction {
  DOODLE_SHOWN = 0,
  DOODLE_CLICKED,
  // Add values here.

  DOODLE_ACTION_LAST,
};

}  // namespace

StartPageHandler::StartPageHandler() {
}

StartPageHandler::~StartPageHandler() {
}

void StartPageHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "appListShown", base::Bind(&StartPageHandler::HandleAppListShown,
                                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "doodleClicked", base::Bind(&StartPageHandler::HandleDoodleClicked,
                                  base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "initialize",
      base::Bind(&StartPageHandler::HandleInitialize, base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "launchApp",
      base::Bind(&StartPageHandler::HandleLaunchApp, base::Unretained(this)));
}

void StartPageHandler::HandleAppListShown(const base::ListValue* args) {
  bool doodle_shown = false;
  if (args->GetBoolean(0, &doodle_shown) && doodle_shown) {
    UMA_HISTOGRAM_ENUMERATION(kAppListDoodleActionHistogram, DOODLE_SHOWN,
                              DOODLE_ACTION_LAST);
  }
}

void StartPageHandler::HandleDoodleClicked(const base::ListValue* args) {
  UMA_HISTOGRAM_ENUMERATION(kAppListDoodleActionHistogram, DOODLE_CLICKED,
                            DOODLE_ACTION_LAST);
}

void StartPageHandler::HandleInitialize(const base::ListValue* args) {
  Profile* profile = Profile::FromWebUI(web_ui());
  StartPageService* service = StartPageService::Get(profile);
  if (!service)
    return;

  service->WebUILoaded();
}

void StartPageHandler::HandleLaunchApp(const base::ListValue* args) {
  std::string app_id;
  CHECK(args->GetString(0, &app_id));

  Profile* profile = Profile::FromWebUI(web_ui());
  const extensions::Extension* app =
      extensions::ExtensionRegistry::Get(profile)
          ->GetExtensionById(app_id, extensions::ExtensionRegistry::EVERYTHING);
  if (!app) {
    NOTREACHED();
    return;
  }

  AppListControllerDelegate* controller =
      AppListService::Get()->GetControllerDelegate();
  controller->ActivateApp(profile,
                          app,
                          AppListControllerDelegate::LAUNCH_FROM_APP_LIST,
                          ui::EF_NONE);
}

}  // namespace app_list
