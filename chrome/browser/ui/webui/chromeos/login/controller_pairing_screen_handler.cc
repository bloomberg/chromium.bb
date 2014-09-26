// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/controller_pairing_screen_handler.h"

#include "base/strings/string_util.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"

namespace chromeos {

namespace {

const char kJsScreenPath[] = "login.ControllerPairingScreen";

const char kMethodContextChanged[] = "contextChanged";

const char kCallbackUserActed[] = "userActed";
const char kCallbackContextChanged[] = "contextChanged";

}  // namespace

ControllerPairingScreenHandler::ControllerPairingScreenHandler()
    : BaseScreenHandler(kJsScreenPath), delegate_(NULL), show_on_init_(false) {
}

ControllerPairingScreenHandler::~ControllerPairingScreenHandler() {
  if (delegate_)
    delegate_->OnActorDestroyed(this);
}

void ControllerPairingScreenHandler::HandleUserActed(
    const std::string& action) {
  if (!delegate_)
    return;
  delegate_->OnUserActed(action);
}

void ControllerPairingScreenHandler::HandleContextChanged(
    const base::DictionaryValue* diff) {
  if (!delegate_)
    return;
  delegate_->OnScreenContextChanged(*diff);
}

void ControllerPairingScreenHandler::Initialize() {
  if (!page_is_ready() || !delegate_)
    return;

  if (show_on_init_) {
    Show();
    show_on_init_ = false;
  }
}

void ControllerPairingScreenHandler::DeclareLocalizedValues(
    LocalizedValuesBuilder* builder) {
  // TODO(dzhioev): Move the prefix logic to the base screen handler after
  // migration.
  std::string prefix;
  base::ReplaceChars(kJsScreenPath, ".", "_", &prefix);
  prefix += "_";

  builder->Add(prefix + "welcome", IDS_PAIRING_CONTROLLER_WELCOME);
  builder->Add(prefix + "searching", IDS_PAIRING_CONTROLLER_SEARCHING);
  builder->Add(prefix + "needHelp", IDS_PAIRING_NEED_HELP);
  builder->Add(prefix + "troubleConnecting",
               IDS_PAIRING_CONTROLLER_TROUBLE_CONNECTING);
  builder->Add(prefix + "connectingAdvice",
               IDS_PAIRING_CONTROLLER_CONNECTING_ADVICE);
  builder->Add(prefix + "adviceGotIt", IDS_PAIRING_CONTROLLER_ADVICE_GOT_IT);
  builder->Add(prefix + "selectTitle", IDS_PAIRING_CONTROLLER_SELECT_TITLE);
  builder->Add(prefix + "connect", IDS_PAIRING_CONTROLLER_CONNECT);
  builder->Add(prefix + "connecting", IDS_PAIRING_CONTROLLER_CONNECTING);
  builder->Add(prefix + "confirmationTitle",
               IDS_PAIRING_CONTROLLER_CONFIRMATION_TITLE);
  builder->Add(prefix + "confirmationQuestion",
               IDS_PAIRING_CONTROLLER_CONFIRMATION_QUESTION);
  builder->Add(prefix + "rejectCode", IDS_PAIRING_CONTROLLER_REJECT_CODE);
  builder->Add(prefix + "acceptCode", IDS_PAIRING_CONTROLLER_ACCEPT_CODE);
  builder->Add(prefix + "updateTitle", IDS_PAIRING_CONTROLLER_UPDATE_TITLE);
  builder->Add(prefix + "updateText", IDS_PAIRING_CONTROLLER_UPDATE_TEXT);
  builder->Add(prefix + "connectionLostTitle",
               IDS_PAIRING_CONTROLLER_CONNECTION_LOST_TITLE);
  builder->Add(prefix + "connectionLostText",
               IDS_PAIRING_CONTROLLER_CONNECTION_LOST_TEXT);
  builder->Add(prefix + "enrollTitle", IDS_PAIRING_ENROLL_TITLE);
  builder->Add(prefix + "enrollText1", IDS_PAIRING_CONTROLLER_ENROLL_TEXT_1);
  builder->Add(prefix + "enrollText2", IDS_PAIRING_CONTROLLER_ENROLL_TEXT_2);
  builder->Add(prefix + "continue", IDS_PAIRING_CONTROLLER_CONTINUE);
  builder->Add(prefix + "enrollmentInProgress",
               IDS_PAIRING_ENROLLMENT_IN_PROGRESS);
  builder->Add(prefix + "enrollmentErrorTitle",
               IDS_PAIRING_ENROLLMENT_ERROR_TITLE);
  builder->Add(prefix + "enrollmentErrorHostRestarts",
               IDS_PAIRING_CONTROLLER_ENROLLMENT_ERROR_HOST_RESTARTS);
  builder->Add(prefix + "successTitle", IDS_PAIRING_CONTROLLER_SUCCESS_TITLE);
  builder->Add(prefix + "successText", IDS_PAIRING_CONTROLLER_SUCCESS_TEXT);
  builder->Add(prefix + "continueToHangouts",
               IDS_PAIRING_CONTROLLER_CONTINUE_TO_HANGOUTS);
}

void ControllerPairingScreenHandler::RegisterMessages() {
  AddPrefixedCallback(kCallbackUserActed,
                      &ControllerPairingScreenHandler::HandleUserActed);
  AddPrefixedCallback(kCallbackContextChanged,
                      &ControllerPairingScreenHandler::HandleContextChanged);
}

void ControllerPairingScreenHandler::Show() {
  if (!page_is_ready()) {
    show_on_init_ = true;
    return;
  }
  ShowScreen(OobeUI::kScreenControllerPairing, NULL);
}

void ControllerPairingScreenHandler::Hide() {
}

void ControllerPairingScreenHandler::SetDelegate(Delegate* delegate) {
  delegate_ = delegate;
  if (page_is_ready())
    Initialize();
}

void ControllerPairingScreenHandler::OnContextChanged(
    const base::DictionaryValue& diff) {
  CallJS(kMethodContextChanged, diff);
}

content::BrowserContext* ControllerPairingScreenHandler::GetBrowserContext() {
  return web_ui()->GetWebContents()->GetBrowserContext();
}

}  // namespace chromeos
