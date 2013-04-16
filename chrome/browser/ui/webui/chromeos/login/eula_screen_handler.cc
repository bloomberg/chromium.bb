// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/eula_screen_handler.h"

#include <string>

#include "base/values.h"
#include "chrome/browser/chromeos/login/help_app_launcher.h"
#include "chrome/browser/chromeos/login/webui_login_display.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/common/url_constants.h"
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/views/widget/widget.h"

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
  ShowScreen(OobeUI::kScreenOobeEula, NULL);
}

void EulaScreenHandler::Hide() {
}

void EulaScreenHandler::SetDelegate(Delegate* delegate) {
  delegate_ = delegate;
  if (page_is_ready())
    Initialize();
}

void EulaScreenHandler::DeclareLocalizedValues(
    LocalizedValuesBuilder* builder) {
  builder->Add("eulaScreenTitle", IDS_EULA_SCREEN_TITLE);
  builder->Add("checkboxLogging", IDS_EULA_CHECKBOX_ENABLE_LOGGING);
  builder->Add("back", IDS_EULA_BACK_BUTTON);
  builder->Add("acceptAgreement", IDS_EULA_ACCEPT_AND_CONTINUE_BUTTON);
  builder->Add("eulaSystemInstallationSettings",
               IDS_EULA_SYSTEM_SECURITY_SETTING);
  builder->Add("eulaTpmDesc", IDS_EULA_TPM_DESCRIPTION);
  builder->Add("eulaTpmKeyDesc", IDS_EULA_TPM_KEY_DESCRIPTION);
  builder->Add("eulaTpmDescPowerwash", IDS_EULA_TPM_KEY_DESCRIPTION_POWERWASH);
  builder->Add("eulaTpmBusy", IDS_EULA_TPM_BUSY);
  builder->Add("eulaSystemInstallationSettingsOkButton", IDS_OK);
#if defined(ENABLE_RLZ)
  builder->AddF("eulaRlzDesc",
                IDS_EULA_RLZ_DESCRIPTION,
                IDS_SHORT_PRODUCT_NAME,
                IDS_PRODUCT_NAME);
  builder->AddF("eulaRlzEnable",
                IDS_EULA_RLZ_ENABLE,
                IDS_SHORT_PRODUCT_OS_NAME);
#endif
}

void EulaScreenHandler::GetAdditionalParameters(base::DictionaryValue* dict) {
#if defined(ENABLE_RLZ)
  dict->SetString("rlzEnabled", "enabled");
#else
  dict->SetString("rlzEnabled", "disabled");
#endif
}

void EulaScreenHandler::Initialize() {
  if (!page_is_ready() || !delegate_)
    return;

  base::FundamentalValue checked(delegate_->IsUsageStatsEnabled());
  CallJS("cr.ui.Oobe.setUsageStats", checked);

  // This OEM EULA is a file:// URL which we're unable to load in iframe.
  // Instead if it's defined we use chrome://terms/oem that will load same file.
  if (!delegate_->GetOemEulaUrl().is_empty()) {
    StringValue oem_eula_url(chrome::kChromeUITermsOemURL);
    CallJS("cr.ui.Oobe.setOemEulaUrl", oem_eula_url);
  }

  if (show_on_init_) {
    Show();
    show_on_init_ = false;
  }
}

void EulaScreenHandler::RegisterMessages() {
  AddCallback("eulaOnExit", &EulaScreenHandler::HandleOnExit);
  AddCallback("eulaOnLearnMore", &EulaScreenHandler::HandleOnLearnMore);
  AddCallback("eulaOnInstallationSettingsPopupOpened",
              &EulaScreenHandler::HandleOnInstallationSettingsPopupOpened);
}

void EulaScreenHandler::OnPasswordFetched(const std::string& tpm_password) {
  StringValue tpm_password_value(tpm_password);
  CallJS("cr.ui.Oobe.setTpmPassword", tpm_password_value);
}

void EulaScreenHandler::HandleOnExit(const base::ListValue* args) {
  DCHECK(args->GetSize() == 2);

  bool accepted = false;
  if (!args->GetBoolean(0, &accepted))
    NOTREACHED();

  bool usage_stats_enabled = false;
  if (!args->GetBoolean(1, &usage_stats_enabled))
    NOTREACHED();

  if (!delegate_)
    return;

  delegate_->OnExit(accepted, usage_stats_enabled);
}

void EulaScreenHandler::HandleOnLearnMore(const base::ListValue* args) {
  if (!help_app_.get()) {
    help_app_ = new HelpAppLauncher(GetNativeWindow());
  }
  help_app_->ShowHelpTopic(HelpAppLauncher::HELP_STATS_USAGE);
}

void EulaScreenHandler::HandleOnInstallationSettingsPopupOpened(
    const base::ListValue* args) {
  if (!delegate_)
    return;
  delegate_->InitiatePasswordFetch();
}

}  // namespace chromeos
