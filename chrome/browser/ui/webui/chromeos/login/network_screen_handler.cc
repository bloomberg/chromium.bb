// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/network_screen_handler.h"

#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {

NetworkScreenHandler::NetworkScreenHandler()
    : screen_(NULL) {
}

NetworkScreenHandler::~NetworkScreenHandler() {
  ClearErrors();
}

void NetworkScreenHandler::SetDelegate(NetworkScreenActor::Delegate* screen) {
  screen_ = screen;
}

void NetworkScreenHandler::PrepareToShow() {
}

void NetworkScreenHandler::Show() {
  scoped_ptr<Value> value(Value::CreateIntegerValue(0));
  web_ui_->CallJavascriptFunction("cr.ui.Oobe.toggleStep", *value);
}

void NetworkScreenHandler::Hide() {
}

void NetworkScreenHandler::ShowError(const string16& message) {
  // scoped_ptr<Value> message_value(Value::CreateStringValue(message));
  // web_ui_->CallJavascriptFunction("cr.ui.Oobe.showError", *message_value);
}

void NetworkScreenHandler::ClearErrors() {
  // web_ui_->CallJavascriptFunction("cr.ui.Oobe.clearErrors");
}

void NetworkScreenHandler::ShowConnectingStatus(
    bool connecting,
    const string16& network_id) {
  // string16 connecting_label =
  //     l10n_util::GetStringFUTF16(IDS_NETWORK_SELECTION_CONNECTING,
  //                                network_id);
  // scoped_ptr<Value> connecting_value(Value::CreateBooleanValue(connecting));
  // scoped_ptr<Value> network_id_value(Value::CreateStringValue(network_id));
  // scoped_ptr<Value> connecting_label_value(
  //     Value::CreateStringValue(connecting_label));
  // web_ui_->CallJavascriptFunction("cr.ui.Oobe.showConnectingStatus",
  //                                 *connecting_value,
  //                                 *network_id_value,
  //                                 *connecting_label_value);
}

void NetworkScreenHandler::EnableContinue(bool enabled) {
  // scoped_ptr<Value> enabled_value(Value::CreateBooleanValue(enabled));
  // web_ui_->CallJavascriptFunction("cr.ui.Oobe.enableContinue",
  //                                 *enabled_value);
}

void NetworkScreenHandler::GetLocalizedStrings(
    DictionaryValue* localized_strings) {
  localized_strings->SetString("networkScreenTitle",
      l10n_util::GetStringUTF16(IDS_WELCOME_SCREEN_TITLE));
  localized_strings->SetString("selectLanguage",
      l10n_util::GetStringUTF16(IDS_LANGUAGE_SELECTION_SELECT));
  localized_strings->SetString("selectKeyboard",
      l10n_util::GetStringUTF16(IDS_KEYBOARD_SELECTION_SELECT));
  localized_strings->SetString("selectNetwork",
      l10n_util::GetStringUTF16(IDS_NETWORK_SELECTION_SELECT));
  localized_strings->SetString("proxySettings",
      l10n_util::GetStringUTF16(IDS_OPTIONS_PROXIES_CONFIGURE_BUTTON));
  localized_strings->SetString("continueButton",
      l10n_util::GetStringUTF16(IDS_NETWORK_SELECTION_CONTINUE_BUTTON));
}

void NetworkScreenHandler::Initialize() {
  // TODO(avayvod): Set necessary data.
  // TODO(avayvod): Initialize languages, keyboards and networks lists.
}

void NetworkScreenHandler::RegisterMessages() {
  web_ui_->RegisterMessageCallback("networkOnExit",
      NewCallback(this, &NetworkScreenHandler::OnExit));
}

void NetworkScreenHandler::OnExit(const ListValue* args) {
  ClearErrors();
  screen_->OnContinuePressed();
}

}  // namespace chromeos
