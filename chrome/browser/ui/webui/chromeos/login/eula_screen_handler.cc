// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/eula_screen_handler.h"

#include "base/values.h"
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {

EulaScreenHandler::EulaScreenHandler() {
}

EulaScreenHandler::~EulaScreenHandler() {
}

void EulaScreenHandler::PrepareToShow() {
}

void EulaScreenHandler::Show() {
  scoped_ptr<Value> value(Value::CreateIntegerValue(1));
  web_ui_->CallJavascriptFunction("cr.ui.Oobe.toggleStep", *value);
}

void EulaScreenHandler::Hide() {
}

void EulaScreenHandler::SetDelegate(Delegate* delegate) {
  delegate_ = delegate;
}

void EulaScreenHandler::GetLocalizedSettings(
    DictionaryValue* localized_strings) {
  localized_strings->SetString("eulaScreenTitle",
                    l10n_util::GetStringFUTF16(IDS_EULA_SCREEN_TITLE,
                        l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_NAME)));
  localized_strings->SetString("acceptAgreement",
      l10n_util::GetStringUTF16(IDS_EULA_ACCEPT_AND_CONTINUE_BUTTON));
  localized_strings->SetString("back",
                    l10n_util::GetStringUTF16(IDS_EULA_BACK_BUTTON));
  localized_strings->SetString("checkboxLogging",
      l10n_util::GetStringUTF16(IDS_EULA_CHECKBOX_ENABLE_LOGGING));
  localized_strings->SetString("learnMore",
                    l10n_util::GetStringUTF16(IDS_LEARN_MORE));
}

void EulaScreenHandler::Initialize() {
  scoped_ptr<Value> value(Value::CreateBooleanValue(true));
  web_ui_->CallJavascriptFunction("setUsageStats", *value);
}

void EulaScreenHandler::RegisterMessages() {
  web_ui_->RegisterMessageCallback("eulaOnExit",
      NewCallback(this, &EulaScreenHandler::OnExit));
}

void EulaScreenHandler::OnExit(const ListValue* args) {
  DCHECK(args->GetSize() == 2);

  bool accepted = false;
  if (!args->GetBoolean(0, &accepted))
    NOTREACHED();

  bool is_usage_stats_checked = false;
  if (!args->GetBoolean(1, &is_usage_stats_checked))
    NOTREACHED();

  if (!delegate_)
    return;

  delegate_->OnExit(accepted, is_usage_stats_checked);
}

}  // namespace chromeos
