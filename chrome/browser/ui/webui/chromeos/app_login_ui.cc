// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/app_login_ui.h"

#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/oauth2_token_fetcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/token_service.h"
#include "chrome/browser/signin/token_service_factory.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "google_apis/gaia/gaia_switches.h"
#include "google_apis/gaia/gaia_urls.h"
#include "grit/browser_resources.h"

namespace chromeos {

namespace {

content::WebUIDataSource* CreateWebUIDataSource() {
  content::WebUIDataSource* source =
        content::WebUIDataSource::Create(chrome::kChromeUIAppLoginHost);
  source->SetUseJsonJSFormatV2();
  source->SetJsonPath("strings.js");

  source->SetDefaultResource(IDR_APP_LOGIN_HTML);
  source->AddResourcePath("app_login.css", IDR_APP_LOGIN_CSS);
  source->AddResourcePath("app_login.js", IDR_APP_LOGIN_JS);
  return source;
};

class AppLoginUIHandler : public content::WebUIMessageHandler,
                          public OAuth2TokenFetcher::Delegate {
 public:
  explicit AppLoginUIHandler(Profile* profile) : profile_(profile) {}
  virtual ~AppLoginUIHandler() {}

  // content::WebUIMessageHandler overrides:
  virtual void RegisterMessages() OVERRIDE {
    web_ui()->RegisterMessageCallback("initialize",
        base::Bind(&AppLoginUIHandler::HandleInitialize,
                   base::Unretained(this)));
    web_ui()->RegisterMessageCallback("completeLogin",
        base::Bind(&AppLoginUIHandler::HandleCompleteLogin,
                   base::Unretained(this)));
  }

 private:
  void LoadAuthExtension() {
    base::DictionaryValue params;

    const std::string& app_locale = g_browser_process->GetApplicationLocale();
    params.SetString("hl", app_locale);

    params.SetString("gaiaOrigin", GaiaUrls::GetInstance()->gaia_origin_url());
    const CommandLine* command_line = CommandLine::ForCurrentProcess();
    if (command_line->HasSwitch(::switches::kGaiaUrlPath)) {
      params.SetString(
          "gaiaUrlPath",
          command_line->GetSwitchValueASCII(::switches::kGaiaUrlPath));
    }

    web_ui()->CallJavascriptFunction("app.login.loadAuthExtension", params);
  }

  // JS callback:
  void HandleInitialize(const base::ListValue* args) {
    LoadAuthExtension();
  }

  void HandleCompleteLogin(const base::ListValue* args) {
    oauth2_token_fetcher_.reset(
        new OAuth2TokenFetcher(this, profile_->GetRequestContext()));
    oauth2_token_fetcher_->StartExchangeFromCookies();
  }

  // OAuth2TokenFetcher::Delegate overrides:
  virtual void OnOAuth2TokensAvailable(
      const GaiaAuthConsumer::ClientOAuthResult& oauth2_tokens) OVERRIDE {
    // Closes sign-in dialog before update token service. Token service update
    // might trigger a permission dialog and if this dialog does not close,
    // a DCHECK would be triggered because attempting to activate a window
    // while there is a modal dialog.
    web_ui()->CallJavascriptFunction("app.login.closeDialog");

    TokenService* token_service =
        TokenServiceFactory::GetForProfile(profile_);
    token_service->UpdateCredentialsWithOAuth2(oauth2_tokens);
  }

  virtual void OnOAuth2TokensFetchFailed() OVERRIDE {
    LOG(ERROR) << "Failed to fetch oauth2 token for app.";
    web_ui()->CallJavascriptFunction("app.login.handleOAuth2TokenFailure");
  }

  Profile* profile_;
  scoped_ptr<OAuth2TokenFetcher> oauth2_token_fetcher_;

  DISALLOW_COPY_AND_ASSIGN(AppLoginUIHandler);
};

}  // namespace

AppLoginUI::AppLoginUI(content::WebUI* web_ui)
    : WebDialogUI(web_ui),
      auth_extension_(Profile::FromWebUI(web_ui)) {
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile, CreateWebUIDataSource());

  web_ui->AddMessageHandler(new AppLoginUIHandler(profile));
}

AppLoginUI::~AppLoginUI() {}

}  // namespace chromeos
