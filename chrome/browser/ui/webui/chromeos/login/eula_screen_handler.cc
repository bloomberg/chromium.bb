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

EulaScreenHandler::EulaScreenHandler()
    : delegate_(NULL), show_on_init_(false) {
}

EulaScreenHandler::~EulaScreenHandler() {
  if (delegate_)
    delegate_->OnActorDestroyed(this);
}

void EulaScreenHandler::PrepareToShow() {
}

void EulaScreenHandler::Show() {
  if (!page_is_ready()) {
    show_on_init_ = true;
    return;
  }
  scoped_ptr<Value> value(Value::CreateIntegerValue(1));
  web_ui_->CallJavascriptFunction("cr.ui.Oobe.toggleStep", *value);
}

void EulaScreenHandler::Hide() {
}

void EulaScreenHandler::SetDelegate(Delegate* delegate) {
  delegate_ = delegate;
  if (page_is_ready())
    Initialize();
}

void EulaScreenHandler::GetLocalizedStrings(
    DictionaryValue* localized_strings) {
  localized_strings->SetString("eulaScreenTitle",
      l10n_util::GetStringUTF16(IDS_EULA_SCREEN_TITLE));
  localized_strings->SetString("checkboxLogging",
      l10n_util::GetStringUTF16(IDS_EULA_CHECKBOX_ENABLE_LOGGING));
  localized_strings->SetString("learnMore",
      l10n_util::GetStringUTF16(IDS_LEARN_MORE));
  localized_strings->SetString("back",
      l10n_util::GetStringUTF16(IDS_EULA_BACK_BUTTON));
  localized_strings->SetString("acceptAgreement",
      l10n_util::GetStringUTF16(IDS_EULA_ACCEPT_AND_CONTINUE_BUTTON));
  localized_strings->SetString("eulaSystemSecuritySetting",
      l10n_util::GetStringUTF16(IDS_EULA_SYSTEM_SECURITY_SETTING));
  localized_strings->SetString("eulaTpmDesc",
      l10n_util::GetStringUTF16(IDS_EULA_SYSTEM_SECURITY_SETTING_DESCRIPTION));
  localized_strings->SetString("eulaTpmKeyDesc",
      l10n_util::GetStringUTF16(
          IDS_EULA_SYSTEM_SECURITY_SETTING_DESCRIPTION_KEY));
  localized_strings->SetString("eulaTpmBusy",
      l10n_util::GetStringUTF16(IDS_EULA_TPM_BUSY));
  localized_strings->SetString("eulaTpmOkButton",
      l10n_util::GetStringUTF16(IDS_OK));
}

void EulaScreenHandler::Initialize() {
  if (!page_is_ready() || !delegate_)
    return;

  FundamentalValue checked(delegate_->IsUsageStatsEnabled());
  web_ui_->CallJavascriptFunction("cr.ui.Oobe.setUsageStats", checked);

  StringValue oem_eula_url(delegate_->GetOemEulaUrl().spec());
  web_ui_->CallJavascriptFunction("cr.ui.Oobe.setOemEulaUrl", oem_eula_url);

  if (show_on_init_) {
    Show();
    show_on_init_ = false;
  }
}

void EulaScreenHandler::RegisterMessages() {
  web_ui_->RegisterMessageCallback("eulaOnExit",
      NewCallback(this, &EulaScreenHandler::OnExit));
  web_ui_->RegisterMessageCallback("eulaOnTpmPopupOpened",
      NewCallback(this, &EulaScreenHandler::OnTpmPopupOpened));
}

void EulaScreenHandler::OnPasswordFetched(const std::string& tpm_password) {
  StringValue tpm_password_value(tpm_password);
  web_ui_->CallJavascriptFunction("cr.ui.Oobe.setTpmPassword",
                                  tpm_password_value);
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

void EulaScreenHandler::OnTpmPopupOpened(const ListValue* args) {
  if (!delegate_)
    return;
  delegate_->InitiatePasswordFetch();
}

}  // namespace chromeos
