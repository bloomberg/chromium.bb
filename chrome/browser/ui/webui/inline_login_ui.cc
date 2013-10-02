// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/inline_login_ui.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/signin/token_service.h"
#include "chrome/browser/signin/token_service_factory.h"
#include "chrome/browser/ui/sync/one_click_signin_sync_starter.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "google_apis/gaia/gaia_switches.h"
#include "google_apis/gaia/gaia_urls.h"
#include "grit/browser_resources.h"
#include "net/base/escape.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/oauth2_token_fetcher.h"
#endif

namespace {

content::WebUIDataSource* CreateWebUIDataSource() {
  content::WebUIDataSource* source =
        content::WebUIDataSource::Create(chrome::kChromeUIInlineLoginHost);
  source->SetUseJsonJSFormatV2();
  source->SetJsonPath("strings.js");

  source->SetDefaultResource(IDR_INLINE_LOGIN_HTML);
  source->AddResourcePath("inline_login.css", IDR_INLINE_LOGIN_CSS);
  source->AddResourcePath("inline_login.js", IDR_INLINE_LOGIN_JS);
  return source;
};

#if defined(OS_CHROMEOS)
class InlineLoginUIOAuth2Delegate
    : public chromeos::OAuth2TokenFetcher::Delegate {
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
    TokenService* token_service =
        TokenServiceFactory::GetForProfile(profile);
    token_service->UpdateCredentialsWithOAuth2(oauth2_tokens);
  }

  virtual void OnOAuth2TokensFetchFailed() OVERRIDE {
    LOG(ERROR) << "Failed to fetch oauth2 token with inline login.";
    web_ui_->CallJavascriptFunction("inline.login.handleOAuth2TokenFailure");
  }

 private:
  content::WebUI* web_ui_;
};
#endif // OS_CHROMEOS

class InlineLoginUIHandler : public content::WebUIMessageHandler {
 public:
  explicit InlineLoginUIHandler(Profile* profile) : profile_(profile) {}
  virtual ~InlineLoginUIHandler() {}

  // content::WebUIMessageHandler overrides:
  virtual void RegisterMessages() OVERRIDE {
    web_ui()->RegisterMessageCallback("initialize",
        base::Bind(&InlineLoginUIHandler::HandleInitialize,
                   base::Unretained(this)));
    web_ui()->RegisterMessageCallback("completeLogin",
        base::Bind(&InlineLoginUIHandler::HandleCompleteLogin,
                   base::Unretained(this)));
  }

 private:
  // Enum for gaia auth mode, must match AuthMode defined in
  // chrome/browser/resources/gaia_auth_host/gaia_auth_host.js.
  enum AuthMode {
    kDefaultAuthMode = 0,
    kOfflineAuthMode = 1,
    kInlineAuthMode = 2
  };

  void LoadAuthExtension() {
    base::DictionaryValue params;

    const std::string& app_locale = g_browser_process->GetApplicationLocale();
    params.SetString("hl", app_locale);

    GaiaUrls* gaiaUrls = GaiaUrls::GetInstance();
    params.SetString("gaiaUrl", gaiaUrls->gaia_url().spec());

    bool enable_inline = CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kEnableInlineSignin);
    params.SetInteger("authMode",
        enable_inline ? kInlineAuthMode : kDefaultAuthMode);
    // Set continueUrl param for the inline sign in flow. It should point to
    // the oauth2 auth code URL so that later we can grab the auth code from
    // the cookie jar of the embedded webview.
    if (enable_inline) {
      std::string scope = net::EscapeUrlEncodedData(
          gaiaUrls->oauth1_login_scope(), true);
      std::string client_id = net::EscapeUrlEncodedData(
          gaiaUrls->oauth2_chrome_client_id(), true);
      std::string encoded_continue_params = base::StringPrintf(
          "?scope=%s&client_id=%s", scope.c_str(), client_id.c_str());
      params.SetString("continueUrl",
          gaiaUrls->client_login_to_oauth2_url().Resolve(
              encoded_continue_params).spec());
    }

    web_ui()->CallJavascriptFunction("inline.login.loadAuthExtension", params);
  }

  // JS callback:
  void HandleInitialize(const base::ListValue* args) {
    LoadAuthExtension();
  }

  void HandleCompleteLogin(const base::ListValue* args) {
    // TODO(guohui, xiyuan): we should investigate if it is possible to unify
    // the signin-with-cookies flow across ChromeOS and Chrome.
#if defined(OS_CHROMEOS)
    oauth2_delegate_.reset(new InlineLoginUIOAuth2Delegate(web_ui()));
    oauth2_token_fetcher_.reset(new chromeos::OAuth2TokenFetcher(
        oauth2_delegate_.get(), profile_->GetRequestContext()));
    oauth2_token_fetcher_->StartExchangeFromCookies();
#elif !defined(OS_ANDROID)
    const base::DictionaryValue* dict = NULL;
    string16 email;
    string16 password;
    if (!args->GetDictionary(0, &dict) || !dict ||
        !dict->GetString("email", &email) ||
        !dict->GetString("password", &password)) {
      NOTREACHED();
      return;
    }

    // Call OneClickSigninSyncStarter to exchange cookies for oauth tokens.
    // OneClickSigninSyncStarter will delete itself once the job is done.
    // TODO(guohui): should collect from user whether they want to use
    // default sync settings or configure first.
    new OneClickSigninSyncStarter(
        profile_, NULL, "0" /* session_index 0 for the default user */,
        UTF16ToASCII(email), UTF16ToASCII(password),
        OneClickSigninSyncStarter::SYNC_WITH_DEFAULT_SETTINGS,
        web_ui()->GetWebContents(),
        OneClickSigninSyncStarter::NO_CONFIRMATION,
        signin::SOURCE_UNKNOWN,
        OneClickSigninSyncStarter::Callback());
    web_ui()->CallJavascriptFunction("inline.login.closeDialog");
#endif
  }

  Profile* profile_;
#if defined(OS_CHROMEOS)
  scoped_ptr<chromeos::OAuth2TokenFetcher> oauth2_token_fetcher_;
  scoped_ptr<InlineLoginUIOAuth2Delegate> oauth2_delegate_;
#endif

  DISALLOW_COPY_AND_ASSIGN(InlineLoginUIHandler);
};

}  // namespace

InlineLoginUI::InlineLoginUI(content::WebUI* web_ui)
    : WebDialogUI(web_ui),
      auth_extension_(Profile::FromWebUI(web_ui)) {
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile, CreateWebUIDataSource());

  web_ui->AddMessageHandler(new InlineLoginUIHandler(profile));
}

InlineLoginUI::~InlineLoginUI() {}
