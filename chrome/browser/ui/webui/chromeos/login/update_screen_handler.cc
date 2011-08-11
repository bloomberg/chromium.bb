// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/update_screen_handler.h"

#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

// Update screen ID.
const char kUpdateScreen[] = "update";

}  // namespace

namespace chromeos {

UpdateScreenHandler::UpdateScreenHandler() : show_on_init_(false) {
}

UpdateScreenHandler::~UpdateScreenHandler() {
}

void UpdateScreenHandler::GetLocalizedStrings(
    DictionaryValue *localized_strings) {
  localized_strings->SetString("updateScreenTitle",
      l10n_util::GetStringUTF16(IDS_UPDATE_SCREEN_TITLE));
  localized_strings->SetString("checkingForUpdates",
      l10n_util::GetStringUTF16(IDS_CHECKING_FOR_UPDATES));
  localized_strings->SetString("installingUpdateDesc",
      l10n_util::GetStringUTF16(IDS_INSTALLING_UPDATE_DESC));
}

void UpdateScreenHandler::Initialize() {
  if (show_on_init_) {
    Show();
    show_on_init_ = false;
  }
}

void UpdateScreenHandler::Show() {
  if (!page_is_ready()) {
    show_on_init_ = true;
    return;
  }
  ShowScreen(kUpdateScreen, NULL);
}

void UpdateScreenHandler::Hide() {
}

void UpdateScreenHandler::PrepareToShow() {
}

void UpdateScreenHandler::ShowManualRebootInfo() {
  StringValue message(l10n_util::GetStringUTF16(IDS_UPDATE_COMPLETED));
  web_ui_->CallJavascriptFunction("cr.ui.Oobe.setUpdateMessage", message);
}

void UpdateScreenHandler::SetProgress(int progress) {
  base::FundamentalValue progress_value(progress);
  web_ui_->CallJavascriptFunction("cr.ui.Oobe.setUpdateProgress",
                                  progress_value);
}

void UpdateScreenHandler::ShowCurtain(bool enable) {
  base::FundamentalValue enable_value(enable);
  web_ui_->CallJavascriptFunction("cr.ui.Oobe.showUpdateCurtain", enable_value);
}

void UpdateScreenHandler::ShowPreparingUpdatesInfo(bool visible) {
  scoped_ptr<StringValue> info_message;
  if (visible) {
    info_message.reset(Value::CreateStringValue(
        l10n_util::GetStringUTF16(IDS_UPDATE_AVAILABLE)));
  } else {
    info_message.reset(Value::CreateStringValue(
        l10n_util::GetStringFUTF16(IDS_INSTALLING_UPDATE,
          l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_NAME))));
  }
  web_ui_->CallJavascriptFunction("cr.ui.Oobe.setUpdateMessage",
                                  *info_message);
}

void UpdateScreenHandler::RegisterMessages() {
}

}  // namespace chromeos
