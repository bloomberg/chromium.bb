// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/device_stylus_handler.h"

#include "chrome/browser/profiles/profile.h"

namespace chromeos {
namespace settings {

namespace {

// Keys in objects passed to onNoteTakingAppsUpdated.
constexpr char kAppNameKey[] = "name";
constexpr char kAppIdKey[] = "value";
constexpr char kAppPreferredKey[] = "preferred";

}  // namespace

StylusHandler::StylusHandler() {
  NoteTakingHelper::Get()->AddObserver(this);
}

StylusHandler::~StylusHandler() {
  NoteTakingHelper::Get()->RemoveObserver(this);
}

void StylusHandler::RegisterMessages() {
  DCHECK(web_ui());

  web_ui()->RegisterMessageCallback(
      "requestNoteTakingApps",
      base::Bind(&StylusHandler::RequestApps, base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setPreferredNoteTakingApp",
      base::Bind(&StylusHandler::SetPreferredNoteTakingApp,
                 base::Unretained(this)));
}

void StylusHandler::OnAvailableNoteTakingAppsUpdated() {
  UpdateNoteTakingApps();
}

void StylusHandler::UpdateNoteTakingApps() {
  bool waiting_for_android = false;
  note_taking_app_ids_.clear();
  base::ListValue apps_list;

  NoteTakingHelper* helper = NoteTakingHelper::Get();
  if (helper->android_enabled() && !helper->android_apps_received()) {
    // If Android is enabled but not ready yet, let the JS know so it can
    // disable the menu and display an explanatory message.
    waiting_for_android = true;
  } else {
    std::vector<NoteTakingAppInfo> available_apps =
        NoteTakingHelper::Get()->GetAvailableApps(Profile::FromWebUI(web_ui()));
    for (const NoteTakingAppInfo& info : available_apps) {
      auto dict = base::MakeUnique<base::DictionaryValue>();
      dict->SetString(kAppNameKey, info.name);
      dict->SetString(kAppIdKey, info.app_id);
      dict->SetBoolean(kAppPreferredKey, info.preferred);
      apps_list.Append(std::move(dict));

      note_taking_app_ids_.insert(info.app_id);
    }
  }

  CallJavascriptFunction(
      "cr.webUIListenerCallback", base::StringValue("onNoteTakingAppsUpdated"),
      apps_list, base::FundamentalValue(waiting_for_android));
}

void StylusHandler::RequestApps(const base::ListValue* unused_args) {
  AllowJavascript();
  UpdateNoteTakingApps();
}

void StylusHandler::SetPreferredNoteTakingApp(const base::ListValue* args) {
  std::string app_id;
  CHECK(args->GetString(0, &app_id));

  // Sanity check: make sure that the ID we got back from WebUI is in the
  // currently-available set.
  if (!note_taking_app_ids_.count(app_id)) {
    LOG(ERROR) << "Got unknown note-taking-app ID \"" << app_id << "\"";
    return;
  }

  NoteTakingHelper::Get()->SetPreferredApp(Profile::FromWebUI(web_ui()),
                                           app_id);
}

}  // namespace settings
}  // namespace chromeos
