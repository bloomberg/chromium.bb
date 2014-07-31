// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/terms_of_service_screen_handler.h"

#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_split.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/base/locale_util.h"
#include "chrome/browser/chromeos/login/screens/core_oobe_actor.h"
#include "chrome/browser/chromeos/login/users/user_manager.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ime/input_method_manager.h"
#include "components/user_manager/user.h"
#include "content/public/browser/web_ui.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"

namespace {

const char kJsScreenPath[] = "login.TermsOfServiceScreen";

}  // namespace

namespace chromeos {

TermsOfServiceScreenHandler::TermsOfServiceScreenHandler(
    CoreOobeActor* core_oobe_actor)
    : BaseScreenHandler(kJsScreenPath),
      screen_(NULL),
      core_oobe_actor_(core_oobe_actor),
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

  const std::string locale = ProfileHelper::Get()->GetProfileByUser(
      UserManager::Get()->GetActiveUser())->GetPrefs()->GetString(
          prefs::kApplicationLocale);
  if (locale.empty() || locale == g_browser_process->GetApplicationLocale()) {
    // If the user has not chosen a UI locale yet or the chosen locale matches
    // the current UI locale, show the screen immediately.
    DoShow();
    return;
  }

  // Switch to the user's UI locale before showing the screen.
  scoped_ptr<locale_util::SwitchLanguageCallback> callback(
      new locale_util::SwitchLanguageCallback(
          base::Bind(&TermsOfServiceScreenHandler::OnLanguageChangedCallback,
                     base::Unretained(this))));
  locale_util::SwitchLanguage(locale,
                              true,   // enable_locale_keyboard_layouts
                              false,  // login_layouts_only
                              callback.Pass());
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

void TermsOfServiceScreenHandler::OnLanguageChangedCallback(
    const std::string& requested_locale,
    const std::string& loaded_locale,
    const bool success) {
  // Update the screen contents to the new locale.
  base::DictionaryValue localized_strings;
  static_cast<OobeUI*>(web_ui()->GetController())
      ->GetLocalizedStrings(&localized_strings);
  core_oobe_actor_->ReloadContent(localized_strings);

  DoShow();
}

void TermsOfServiceScreenHandler::DoShow() {
  // Determine the user's most preferred input method.
  std::vector<std::string> input_methods;
  base::SplitString(
      ProfileHelper::Get()->GetProfileByUser(
          UserManager::Get()->GetActiveUser())->GetPrefs()->GetString(
              prefs::kLanguagePreloadEngines),
      ',',
      &input_methods);

  if (!input_methods.empty()) {
    // If the user has a preferred input method, enable it and switch to it.
    chromeos::input_method::InputMethodManager* input_method_manager =
        chromeos::input_method::InputMethodManager::Get();
    input_method_manager->EnableInputMethod(input_methods.front());
    input_method_manager->ChangeInputMethod(input_methods.front());
  }

  // Updates the domain name shown in the UI.
  UpdateDomainInUI();

  // Update the UI to show an error message or the Terms of Service.
  UpdateTermsOfServiceInUI();

  ShowScreen(OobeUI::kScreenTermsOfService, NULL);
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
