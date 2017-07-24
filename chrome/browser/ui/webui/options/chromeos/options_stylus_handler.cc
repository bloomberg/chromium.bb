// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/chromeos/options_stylus_handler.h"

#include "ash/system/palette/palette_utils.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/devices/input_device_manager.h"

namespace chromeos {
namespace options {

namespace {

// Keys in objects passed to updateNoteTakingApps_.
constexpr char kAppNameKey[] = "name";
constexpr char kAppIdKey[] = "id";
constexpr char kAppPreferredKey[] = "preferred";

}  // namespace

OptionsStylusHandler::OptionsStylusHandler() : weak_ptr_factory_(this) {
  NoteTakingHelper::Get()->AddObserver(this);
  ui::InputDeviceManager::GetInstance()->AddObserver(this);
}

OptionsStylusHandler::~OptionsStylusHandler() {
  ui::InputDeviceManager::GetInstance()->RemoveObserver(this);
  NoteTakingHelper::Get()->RemoveObserver(this);
}

void OptionsStylusHandler::GetLocalizedValues(
    base::DictionaryValue* localized_strings) {
  DCHECK(localized_strings);

  RegisterTitle(localized_strings, "stylusOverlay", IDS_SETTINGS_STYLUS_TITLE);

  localized_strings->SetString(
      "stylusSettingsButtonTitle",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_DEVICE_GROUP_STYLUS_SETTINGS_BUTTON));
  localized_strings->SetString(
      "stylusAutoOpenStylusTools",
      l10n_util::GetStringUTF16(IDS_SETTINGS_STYLUS_AUTO_OPEN_STYLUS_TOOLS));
  localized_strings->SetString(
      "stylusEnableStylusTools",
      l10n_util::GetStringUTF16(IDS_SETTINGS_STYLUS_ENABLE_STYLUS_TOOLS));
  localized_strings->SetString(
      "stylusFindMoreApps",
      l10n_util::GetStringUTF16(IDS_SETTINGS_STYLUS_FIND_MORE_APPS_PRIMARY));
  localized_strings->SetString(
      "stylusNoteTakingApp",
      l10n_util::GetStringUTF16(IDS_OPTIONS_STYLUS_NOTE_TAKING_APP_LABEL));
  localized_strings->SetString(
      "stylusNoteTakingAppNoneAvailable",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_STYLUS_NOTE_TAKING_APP_NONE_AVAILABLE));
  localized_strings->SetString(
      "stylusNoteTakingAppWaitingForAndroid",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_STYLUS_NOTE_TAKING_APP_WAITING_FOR_ANDROID));
}

void OptionsStylusHandler::InitializePage() {
  UpdateNoteTakingApps();
}

void OptionsStylusHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "setPreferredNoteTakingApp",
      base::Bind(&OptionsStylusHandler::SetPreferredNoteTakingApp,
                 weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "requestStylusHardwareState",
      base::Bind(&OptionsStylusHandler::RequestStylusHardwareState,
                 weak_ptr_factory_.GetWeakPtr()));
}

void OptionsStylusHandler::OnAvailableNoteTakingAppsUpdated() {
  UpdateNoteTakingApps();
}

void OptionsStylusHandler::OnPreferredNoteTakingAppUpdated(Profile* profile) {
  if (Profile::FromWebUI(web_ui()) == profile)
    UpdateNoteTakingApps();
}

void OptionsStylusHandler::OnDeviceListsComplete() {
  SendHasStylus();
}

void OptionsStylusHandler::RequestStylusHardwareState(
    const base::ListValue* args) {
  if (ui::InputDeviceManager::GetInstance()->AreDeviceListsComplete())
    SendHasStylus();
}

void OptionsStylusHandler::SendHasStylus() {
  DCHECK(ui::InputDeviceManager::GetInstance()->AreDeviceListsComplete());

  web_ui()->CallJavascriptFunctionUnsafe(
      "BrowserOptions.setStylusInputStatus",
      base::Value(ash::palette_utils::HasStylusInput()));
}

void OptionsStylusHandler::UpdateNoteTakingApps() {
  bool waiting_for_android = false;
  note_taking_app_ids_.clear();
  base::ListValue apps_list;

  NoteTakingHelper* helper = NoteTakingHelper::Get();
  if (helper->play_store_enabled() && !helper->android_apps_received()) {
    // If Play Store is enabled but not ready yet, let the JS know so it can
    // disable the menu and display an explanatory message.
    waiting_for_android = true;
  } else {
    for (const NoteTakingAppInfo& info :
         NoteTakingHelper::Get()->GetAvailableApps(
             Profile::FromWebUI(web_ui()))) {
      std::unique_ptr<base::DictionaryValue> dict(
          new base::DictionaryValue());
      dict->SetString(kAppNameKey, info.name);
      dict->SetString(kAppIdKey, info.app_id);
      dict->SetBoolean(kAppPreferredKey, info.preferred);
      apps_list.Append(std::move(dict));

      note_taking_app_ids_.insert(info.app_id);
    }
  }

  web_ui()->CallJavascriptFunctionUnsafe("StylusOverlay.updateNoteTakingApps",
                                         apps_list,
                                         base::Value(waiting_for_android));
}

void OptionsStylusHandler::SetPreferredNoteTakingApp(
    const base::ListValue* args) {
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

}  // namespace options
}  // namespace chromeos
