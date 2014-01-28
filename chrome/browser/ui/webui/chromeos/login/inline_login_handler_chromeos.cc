// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/inline_login_handler_chromeos.h"

#include "chrome/browser/chromeos/login/oauth2_token_fetcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/profile_oauth2_token_service.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "content/public/browser/web_ui.h"

namespace chromeos {

class InlineLoginHandlerChromeOS::InlineLoginUIOAuth2Delegate
    : public OAuth2TokenFetcher::Delegate {
 public:
  explicit InlineLoginUIOAuth2Delegate(content::WebUI* web_ui)
      : web_ui_(web_ui) {}
  virtual ~InlineLoginUIOAuth2Delegate() {}

  // OAuth2TokenFetcher::Delegate overrides:
  virtual void OnOAuth2TokensAvailable(
      const GaiaAuthConsumer::ClientOAuthResult& oauth2_tokens) OVERRIDE {
    // Closes sign-in dialog before update token service. Token service update
    // might trigger a permission dialog and if this dialog does not close,
    // a DCHECK would be triggered because attempting to activate a window
    // while there is a modal dialog.
    web_ui_->CallJavascriptFunction("inline.login.closeDialog");

    Profile* profile = Profile::FromWebUI(web_ui_);
    ProfileOAuth2TokenService* token_service =
        ProfileOAuth2TokenServiceFactory::GetForProfile(profile);
    SigninManagerBase* signin_manager =
        SigninManagerFactory::GetForProfile(profile);
    token_service->UpdateCredentials(
        signin_manager->GetAuthenticatedAccountId(),
        oauth2_tokens.refresh_token);
  }

  virtual void OnOAuth2TokensFetchFailed() OVERRIDE {
    LOG(ERROR) << "Failed to fetch oauth2 token with inline login.";
    web_ui_->CallJavascriptFunction("inline.login.handleOAuth2TokenFailure");
  }

 private:
  content::WebUI* web_ui_;
};

InlineLoginHandlerChromeOS::InlineLoginHandlerChromeOS() {}

InlineLoginHandlerChromeOS::~InlineLoginHandlerChromeOS() {}

void InlineLoginHandlerChromeOS::CompleteLogin(const base::ListValue* args) {
  Profile* profile = Profile::FromWebUI(web_ui());

  oauth2_delegate_.reset(new InlineLoginUIOAuth2Delegate(web_ui()));
  oauth2_token_fetcher_.reset(new OAuth2TokenFetcher(
      oauth2_delegate_.get(), profile->GetRequestContext()));
  oauth2_token_fetcher_->StartExchangeFromCookies();
}

} // namespace chromeos
