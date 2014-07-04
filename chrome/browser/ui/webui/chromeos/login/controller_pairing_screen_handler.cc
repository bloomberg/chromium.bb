// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/controller_pairing_screen_handler.h"

#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "content/public/browser/web_contents.h"

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
