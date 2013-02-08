// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/terms_of_service_screen_handler.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "content/public/browser/web_ui.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {

TermsOfServiceScreenHandler::TermsOfServiceScreenHandler()
    : screen_(NULL),
      show_on_init_(false),
      load_error_(false) {
}

TermsOfServiceScreenHandler::~TermsOfServiceScreenHandler() {
  if (screen_)
    screen_->OnActorDestroyed(this);
}

void TermsOfServiceScreenHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback("termsOfServiceBack",
      base::Bind(&TermsOfServiceScreenHandler::HandleBack,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("termsOfServiceAccept",
      base::Bind(&TermsOfServiceScreenHandler::HandleAccept,
                 base::Unretained(this)));
}

void TermsOfServiceScreenHandler::GetLocalizedStrings(
    base::DictionaryValue *localized_strings) {
  localized_strings->SetString("termsOfServiceScreenHeading",
      l10n_util::GetStringUTF16(IDS_TERMS_OF_SERVICE_SCREEN_HEADING));
  localized_strings->SetString("termsOfServiceScreenSubheading",
      l10n_util::GetStringUTF16(IDS_TERMS_OF_SERVICE_SCREEN_SUBHEADING));
  localized_strings->SetString("termsOfServiceContentHeading",
      l10n_util::GetStringUTF16(IDS_TERMS_OF_SERVICE_SCREEN_CONTENT_HEADING));
  localized_strings->SetString("termsOfServiceLoading",
      l10n_util::GetStringUTF16(IDS_TERMS_OF_SERVICE_SCREEN_LOADING));
  localized_strings->SetString("termsOfServiceError",
      l10n_util::GetStringUTF16(IDS_TERMS_OF_SERVICE_SCREEN_ERROR));
  localized_strings->SetString("termsOfServiceTryAgain",
      l10n_util::GetStringUTF16(IDS_TERMS_OF_SERVICE_SCREEN_TRY_AGAIN));
  localized_strings->SetString("termsOfServiceBackButton",
      l10n_util::GetStringUTF16(IDS_TERMS_OF_SERVICE_SCREEN_BACK_BUTTON));
  localized_strings->SetString("termsOfServiceAcceptButton",
      l10n_util::GetStringUTF16(IDS_TERMS_OF_SERVICE_SCREEN_ACCEPT_BUTTON));
}

void TermsOfServiceScreenHandler::SetDelegate(Delegate* screen) {
  screen_ = screen;
}

void TermsOfServiceScreenHandler::Show() {
  if (!page_is_ready()) {
    show_on_init_ = true;
    return;
  }

  // Updates the domain name shown in the UI.
  UpdateDomainInUI();

  // Update the UI to show an error message or the Terms of Service.
  UpdateTermsOfServiceInUI();

  ShowScreen(OobeUI::kScreenTermsOfService, NULL);
}

void TermsOfServiceScreenHandler::Hide() {
}

void TermsOfServiceScreenHandler::SetDomain(const std::string& domain) {
  domain_ = domain;
  UpdateDomainInUI();
}

void TermsOfServiceScreenHandler::OnLoadError() {
  load_error_ = true;
  terms_of_service_ = "";
  UpdateTermsOfServiceInUI();
}

void TermsOfServiceScreenHandler::OnLoadSuccess(
    const std::string& terms_of_service) {
  load_error_ = false;
  terms_of_service_ = terms_of_service;
  UpdateTermsOfServiceInUI();
}

void TermsOfServiceScreenHandler::Initialize() {
  if (show_on_init_) {
    Show();
    show_on_init_ = false;
  }
}

void TermsOfServiceScreenHandler::UpdateDomainInUI() {
  if (!page_is_ready())
    return;

  base::StringValue domain(domain_);
  web_ui()->CallJavascriptFunction("cr.ui.Oobe.setTermsOfServiceDomain",
                                   domain);
}

void TermsOfServiceScreenHandler::UpdateTermsOfServiceInUI() {
  if (!page_is_ready())
    return;

  // If either |load_error_| or |terms_of_service_| is set, the download of the
  // Terms of Service has completed and the UI should be updated. Otherwise, the
  // download is still in progress and the UI will be updated when the
  // OnLoadError() or the OnLoadSuccess() callback is called.
  if (load_error_) {
    web_ui()->CallJavascriptFunction("cr.ui.Oobe.setTermsOfServiceLoadError");
  } else if (!terms_of_service_.empty()) {
    base::StringValue terms_of_service(terms_of_service_);
    web_ui()->CallJavascriptFunction("cr.ui.Oobe.setTermsOfService",
                                     terms_of_service);
  }
}

void TermsOfServiceScreenHandler::HandleBack(const base::ListValue* args) {
  if (screen_)
    screen_->OnDecline();
}

void TermsOfServiceScreenHandler::HandleAccept(const base::ListValue* args) {
  if (!screen_)
    return;

  // If the Terms of Service have not been successfully downloaded, the "accept
  // and continue" button should not be accessible. If the user managed to
  // activate it somehow anway, do not treat this as acceptance of the Terms
  // and Conditions and end the session instead, as if the user had declined.
  if (terms_of_service_.empty())
    screen_->OnDecline();
  else
    screen_->OnAccept();
}

}  // namespace chromeos
