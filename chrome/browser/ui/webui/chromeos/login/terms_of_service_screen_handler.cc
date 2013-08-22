// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/terms_of_service_screen_handler.h"

#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"

namespace {

const char kJsScreenPath[] = "login.TermsOfServiceScreen";

}  // namespace

namespace chromeos {

TermsOfServiceScreenHandler::TermsOfServiceScreenHandler()
    : BaseScreenHandler(kJsScreenPath),
      screen_(NULL),
      show_on_init_(false),
      load_error_(false) {
}

TermsOfServiceScreenHandler::~TermsOfServiceScreenHandler() {
  if (screen_)
    screen_->OnActorDestroyed(this);
}

void TermsOfServiceScreenHandler::RegisterMessages() {
  AddCallback("termsOfServiceBack",
              &TermsOfServiceScreenHandler::HandleBack);
  AddCallback("termsOfServiceAccept",
              &TermsOfServiceScreenHandler::HandleAccept);
}

void TermsOfServiceScreenHandler::DeclareLocalizedValues(
    LocalizedValuesBuilder* builder) {
  builder->Add("termsOfServiceScreenHeading",
               IDS_TERMS_OF_SERVICE_SCREEN_HEADING);
  builder->Add("termsOfServiceScreenSubheading",
               IDS_TERMS_OF_SERVICE_SCREEN_SUBHEADING);
  builder->Add("termsOfServiceContentHeading",
               IDS_TERMS_OF_SERVICE_SCREEN_CONTENT_HEADING);
  builder->Add("termsOfServiceLoading", IDS_TERMS_OF_SERVICE_SCREEN_LOADING);
  builder->Add("termsOfServiceError", IDS_TERMS_OF_SERVICE_SCREEN_ERROR);
  builder->Add("termsOfServiceTryAgain", IDS_TERMS_OF_SERVICE_SCREEN_TRY_AGAIN);
  builder->Add("termsOfServiceBackButton",
               IDS_TERMS_OF_SERVICE_SCREEN_BACK_BUTTON);
  builder->Add("termsOfServiceAcceptButton",
               IDS_TERMS_OF_SERVICE_SCREEN_ACCEPT_BUTTON);
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
  if (page_is_ready())
    CallJS("setDomain", domain_);
}

void TermsOfServiceScreenHandler::UpdateTermsOfServiceInUI() {
  if (!page_is_ready())
    return;

  // If either |load_error_| or |terms_of_service_| is set, the download of the
  // Terms of Service has completed and the UI should be updated. Otherwise, the
  // download is still in progress and the UI will be updated when the
  // OnLoadError() or the OnLoadSuccess() callback is called.
  if (load_error_)
    CallJS("setTermsOfServiceLoadError");
  else if (!terms_of_service_.empty())
    CallJS("setTermsOfService", terms_of_service_);
}

void TermsOfServiceScreenHandler::HandleBack() {
  if (screen_)
    screen_->OnDecline();
}

void TermsOfServiceScreenHandler::HandleAccept() {
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
