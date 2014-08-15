// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/easy_unlock_handler.h"

#include "base/bind.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/easy_unlock_service.h"
#include "content/public/browser/web_ui.h"
#include "grit/generated_resources.h"

namespace options {

EasyUnlockHandler::EasyUnlockHandler() {
}

EasyUnlockHandler::~EasyUnlockHandler() {
  EasyUnlockService::Get(Profile::FromWebUI(web_ui()))->RemoveObserver(this);
}

void EasyUnlockHandler::GetLocalizedValues(base::DictionaryValue* values) {
  static OptionsStringResource resources[] = {
      {"easyUnlockTurnOffButton", IDS_OPTIONS_EASY_UNLOCK_TURN_OFF_BUTTON},
      {"easyUnlockTurnOffTitle", IDS_OPTIONS_EASY_UNLOCK_TURN_OFF_TITLE},
      {"easyUnlockTurnOffDescription",
       IDS_OPTIONS_EASY_UNLOCK_TURN_OFF_DESCRIPTION},
      {"easyUnlockTurnOffOfflineTitle",
       IDS_OPTIONS_EASY_UNLOCK_TURN_OFF_OFFLINE_TITLE},
      {"easyUnlockTurnOffOfflineMessage",
       IDS_OPTIONS_EASY_UNLOCK_TURN_OFF_OFFLINE_MESSAGE},
      {"easyUnlockTurnOffErrorTitle",
       IDS_OPTIONS_EASY_UNLOCK_TURN_OFF_ERROR_TITLE},
      {"easyUnlockTurnOffErrorMessage",
       IDS_OPTIONS_EASY_UNLOCK_TURN_OFF_ERROR_MESSAGE},
      {"easyUnlockTurnOffRetryButton",
       IDS_OPTIONS_EASY_UNLOCK_TURN_OFF_RETRY_BUTTON},
  };

  RegisterStrings(values, resources, arraysize(resources));
}

void EasyUnlockHandler::InitializeHandler() {
  EasyUnlockService::Get(Profile::FromWebUI(web_ui()))->AddObserver(this);
}

void EasyUnlockHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "easyUnlockGetTurnOffFlowStatus",
      base::Bind(&EasyUnlockHandler::HandleGetTurnOffFlowStatus,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "easyUnlockRequestTurnOff",
      base::Bind(&EasyUnlockHandler::HandleRequestTurnOff,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "easyUnlockTurnOffOverlayDismissed",
      base::Bind(&EasyUnlockHandler::HandlePageDismissed,
                 base::Unretained(this)));
}

void EasyUnlockHandler::OnTurnOffOperationStatusChanged() {
  SendTurnOffOperationStatus();
}

void EasyUnlockHandler::SendTurnOffOperationStatus() {
  EasyUnlockService::TurnOffFlowStatus status =
      EasyUnlockService::Get(Profile::FromWebUI(web_ui()))
          ->turn_off_flow_status();

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
  web_ui()->CallJavascriptFunction("EasyUnlockTurnOffOverlay.updateUIState",
                                   base::StringValue(status_string));
}

void EasyUnlockHandler::HandleGetTurnOffFlowStatus(
    const base::ListValue* args) {
  SendTurnOffOperationStatus();
}

void EasyUnlockHandler::HandleRequestTurnOff(const base::ListValue* args) {
  EasyUnlockService::Get(Profile::FromWebUI(web_ui()))->RunTurnOffFlow();
}

void EasyUnlockHandler::HandlePageDismissed(const base::ListValue* args) {
  EasyUnlockService::Get(Profile::FromWebUI(web_ui()))->ResetTurnOffFlow();
}

}  // namespace options
