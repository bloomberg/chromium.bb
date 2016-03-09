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
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/proximity_auth/switches.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

namespace chromeos {
namespace settings {

EasyUnlockSettingsHandler::EasyUnlockSettingsHandler(Profile* profile)
    : profile_(profile) {
  EasyUnlockService::Get(profile)->AddObserver(this);
}

EasyUnlockSettingsHandler::~EasyUnlockSettingsHandler() {
  EasyUnlockService::Get(profile_)->RemoveObserver(this);
}

EasyUnlockSettingsHandler* EasyUnlockSettingsHandler::Create(
    content::WebUIDataSource* html_source,
    Profile* profile) {
  bool allowed = EasyUnlockService::Get(profile)->IsAllowed();
  html_source->AddBoolean("easyUnlockAllowed", allowed);
  if (!allowed)
    return nullptr;

  html_source->AddString("easyUnlockLearnMoreURL",
                         chrome::kEasyUnlockLearnMoreUrl);
  html_source->AddBoolean(
      "easyUnlockProximityDetectionAllowed",
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          proximity_auth::switches::kEnableProximityDetection));

  return new EasyUnlockSettingsHandler(profile);
}

void EasyUnlockSettingsHandler::RegisterMessages() {
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

void EasyUnlockSettingsHandler::OnTurnOffOperationStatusChanged() {
  SendTurnOffOperationStatus();
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
