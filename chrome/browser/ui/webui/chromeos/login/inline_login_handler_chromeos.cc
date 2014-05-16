// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/inline_login_handler_chromeos.h"

#include "chrome/browser/chromeos/login/signin/oauth2_token_fetcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/common/url_constants.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/url_util.h"

namespace chromeos {

class InlineLoginHandlerChromeOS::InlineLoginUIOAuth2Delegate
    : public OAuth2TokenFetcher::Delegate {
 public:
  explicit InlineLoginUIOAuth2Delegate(content::WebUI* web_ui,
                                       const std::string& account_id)
      : web_ui_(web_ui), account_id_(account_id) {}

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
    token_service->UpdateCredentials(account_id_, oauth2_tokens.refresh_token);
  }

  virtual void OnOAuth2TokensFetchFailed() OVERRIDE {
    LOG(ERROR) << "Failed to fetch oauth2 token with inline login.";
    web_ui_->CallJavascriptFunction("inline.login.handleOAuth2TokenFailure");
  }

 private:
  content::WebUI* web_ui_;
  std::string account_id_;

  DISALLOW_COPY_AND_ASSIGN(InlineLoginUIOAuth2Delegate);
};

InlineLoginHandlerChromeOS::InlineLoginHandlerChromeOS() {}

InlineLoginHandlerChromeOS::~InlineLoginHandlerChromeOS() {}

void InlineLoginHandlerChromeOS::CompleteLogin(const base::ListValue* args) {
  Profile* profile = Profile::FromWebUI(web_ui());

  const base::DictionaryValue* dict = NULL;
  args->GetDictionary(0, &dict);

  std::string session_index;
  dict->GetString("sessionIndex", &session_index);
  CHECK(!session_index.empty()) << "Session index is empty.";

  std::string account_id;
  dict->GetString("email", &account_id);
  CHECK(!account_id.empty()) << "Account ID is empty.";

  oauth2_delegate_.reset(new InlineLoginUIOAuth2Delegate(web_ui(), account_id));
  net::URLRequestContextGetter* request_context =
      content::BrowserContext::GetStoragePartitionForSite(
          profile, GURL(chrome::kChromeUIChromeSigninURL))
          ->GetURLRequestContext();
  oauth2_token_fetcher_.reset(
      new OAuth2TokenFetcher(oauth2_delegate_.get(), request_context));
  oauth2_token_fetcher_->StartExchangeFromCookies(session_index);
}

} // namespace chromeos
