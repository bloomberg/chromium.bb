// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/easy_unlock_settings_handler.h"

#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/easy_unlock_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/proximity_auth/switches.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

namespace chromeos {
namespace settings {

EasyUnlockSettingsHandler::EasyUnlockSettingsHandler(Profile* profile)
    : profile_(profile), observers_registered_(false) {
  profile_pref_registrar_.Init(profile->GetPrefs());
}

EasyUnlockSettingsHandler::~EasyUnlockSettingsHandler() {
  if (observers_registered_)
    EasyUnlockService::Get(profile_)->RemoveObserver(this);
}

EasyUnlockSettingsHandler* EasyUnlockSettingsHandler::Create(
    content::WebUIDataSource* html_source,
    Profile* profile) {
  EasyUnlockService* easy_unlock_service = EasyUnlockService::Get(profile);
  bool allowed = easy_unlock_service->IsAllowed();
  html_source->AddBoolean("easyUnlockAllowed", allowed);
  html_source->AddBoolean("easyUnlockEnabled",
                          allowed ? easy_unlock_service->IsEnabled() : false);
  if (!allowed)
    return nullptr;

  html_source->AddBoolean(
      "easyUnlockProximityDetectionAllowed",
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          proximity_auth::switches::kEnableProximityDetection));

  return new EasyUnlockSettingsHandler(profile);
}

void EasyUnlockSettingsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "easyUnlockGetEnabledStatus",
      base::Bind(&EasyUnlockSettingsHandler::HandleGetEnabledStatus,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "easyUnlockLaunchSetup",
      base::Bind(&EasyUnlockSettingsHandler::HandleLaunchSetup,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "easyUnlockGetTurnOffFlowStatus",
      base::Bind(&EasyUnlockSettingsHandler::HandleGetTurnOffFlowStatus,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "easyUnlockRequestTurnOff",
      base::Bind(&EasyUnlockSettingsHandler::HandleRequestTurnOff,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "easyUnlockTurnOffOverlayDismissed",
      base::Bind(&EasyUnlockSettingsHandler::HandlePageDismissed,
                 base::Unretained(this)));
}

void EasyUnlockSettingsHandler::RenderViewReused() {
  // When the page is reloaded, we clear our observers and re-register when
  // the new page's DOM is ready.
  if (!observers_registered_)
    return;

  EasyUnlockService::Get(profile_)->RemoveObserver(this);
  profile_pref_registrar_.RemoveAll();

  observers_registered_ = false;
}

void EasyUnlockSettingsHandler::OnTurnOffOperationStatusChanged() {
  SendTurnOffOperationStatus();
}

void EasyUnlockSettingsHandler::SendEnabledStatus() {
  web_ui()->CallJavascriptFunction(
      "cr.webUIListenerCallback",
      base::StringValue("easy-unlock-enabled-status"),
      base::FundamentalValue(EasyUnlockService::Get(profile_)->IsEnabled()));
}

void EasyUnlockSettingsHandler::SendTurnOffOperationStatus() {
  EasyUnlockService::TurnOffFlowStatus status =
      EasyUnlockService::Get(profile_)->GetTurnOffFlowStatus();

  // Translate status into JS UI state string. Note the translated string
  // should match UIState defined in easy_unlock_turn_off_overlay.js.
  std::string status_string;
  switch (status) {
    case EasyUnlockService::IDLE:
      status_string = "idle";
      break;
    case EasyUnlockService::PENDING:
      status_string = "pending";
      break;
    case EasyUnlockService::FAIL:
      status_string = "server-error";
      break;
    default:
      LOG(ERROR) << "Unknown Easy unlock turn-off operation status: " << status;
      status_string = "idle";
      break;
  }

  web_ui()->CallJavascriptFunction(
      "cr.webUIListenerCallback",
      base::StringValue("easy-unlock-turn-off-flow-status"),
      base::StringValue(status_string));
}

void EasyUnlockSettingsHandler::HandleGetEnabledStatus(
    const base::ListValue* args) {
  // This method is called when the DOM is first ready. Therefore we initialize
  // our observers here.
  if (!observers_registered_) {
    EasyUnlockService::Get(profile_)->AddObserver(this);

    profile_pref_registrar_.Add(
        prefs::kEasyUnlockPairing,
        base::Bind(&EasyUnlockSettingsHandler::SendEnabledStatus,
                   base::Unretained(this)));

    observers_registered_ = true;
  }

  CHECK_EQ(1U, args->GetSize());
  const base::Value* callback_id;
  CHECK(args->Get(0, &callback_id));
  ResolveJavascriptCallback(
      *callback_id,
      base::FundamentalValue(EasyUnlockService::Get(profile_)->IsEnabled()));
}

void EasyUnlockSettingsHandler::HandleLaunchSetup(
    const base::ListValue* args) {
  EasyUnlockService::Get(profile_)->LaunchSetup();
}

void EasyUnlockSettingsHandler::HandleGetTurnOffFlowStatus(
    const base::ListValue* args) {
  SendTurnOffOperationStatus();
}

void EasyUnlockSettingsHandler::HandleRequestTurnOff(
    const base::ListValue* args) {
  EasyUnlockService::Get(profile_)->RunTurnOffFlow();
}

void EasyUnlockSettingsHandler::HandlePageDismissed(
    const base::ListValue* args) {
  EasyUnlockService::Get(profile_)->ResetTurnOffFlow();
}

}  // namespace settings
}  // namespace chromeos
