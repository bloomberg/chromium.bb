// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/inline_login_ui.h"

#include "base/atomic_sequence_num.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/profile_oauth2_token_service.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_global_error.h"
#include "chrome/browser/signin/signin_manager_cookie_helper.h"
#include "chrome/browser/signin/signin_names_io_thread.h"
#include "chrome/browser/signin/signin_oauth_helper.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/signin/token_service.h"
#include "chrome/browser/signin/token_service_factory.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/sync/one_click_signin_sync_starter.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "google_apis/gaia/gaia_switches.h"
#include "google_apis/gaia/gaia_urls.h"
#include "grit/browser_resources.h"
#include "net/base/escape.h"
#include "net/base/url_util.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/oauth2_token_fetcher.h"
#endif

namespace {

content::WebUIDataSource* CreateWebUIDataSource() {
  content::WebUIDataSource* source =
        content::WebUIDataSource::Create(chrome::kChromeUIChromeSigninHost);
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
#elif !defined(OS_ANDROID)
// Global SequenceNumber used for generating unique webview partition IDs.
base::StaticAtomicSequenceNumber next_partition_id;
#endif

class InlineLoginUIHandler : public content::WebUIMessageHandler {
 public:
  explicit InlineLoginUIHandler(Profile* profile)
      : profile_(profile), weak_factory_(this), choose_what_to_sync_(false),
        partition_id_("") {}
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

    // Set parameters specific for inline signin flow.
#if !defined(OS_CHROMEOS) && !defined(OS_ANDROID)
    if (enable_inline) {
      // Set continueUrl param for the inline sign in flow. It should point to
      // the oauth2 auth code URL so that later we can grab the auth code from
      // the cookie jar of the embedded webview.
      std::string scope = net::EscapeUrlEncodedData(
          gaiaUrls->oauth1_login_scope(), true);
      std::string client_id = net::EscapeUrlEncodedData(
          gaiaUrls->oauth2_chrome_client_id(), true);
      std::string encoded_continue_params = base::StringPrintf(
          "?scope=%s&client_id=%s", scope.c_str(), client_id.c_str());

      const GURL& current_url = web_ui()->GetWebContents()->GetURL();
      signin::Source source = signin::GetSourceForPromoURL(current_url);
      DCHECK(source != signin::SOURCE_UNKNOWN);
      if (source == signin::SOURCE_AVATAR_BUBBLE_ADD_ACCOUNT ||
          source == signin::SOURCE_AVATAR_BUBBLE_SIGN_IN) {
        // Drop the leading slash in the path.
        params.SetString("gaiaPath",
            gaiaUrls->embedded_signin_url().path().substr(1));
      }

      params.SetString("service", "chromiumsync");
      base::StringAppendF(
          &encoded_continue_params, "&%s=%d", "source",
          static_cast<int>(source));

      params.SetString("continueUrl",
          gaiaUrls->client_login_to_oauth2_url().Resolve(
              encoded_continue_params).spec());

      std::string email;
      net::GetValueForKeyInQuery(current_url, "Email", &email);
      if (!email.empty())
        params.SetString("email", email);

      partition_id_ =
          "gaia-webview-" + base::IntToString(next_partition_id.GetNext());
      params.SetString("partitionId", partition_id_);
    }
#endif

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
        !dict->GetString("email", &email)) {
      NOTREACHED();
      return;
    }
    dict->GetString("password", &password);
    dict->GetBoolean("chooseWhatToSync", &choose_what_to_sync_);

    content::WebContents* web_contents = web_ui()->GetWebContents();
    content::StoragePartition* partition =
        content::BrowserContext::GetStoragePartitionForSite(
            web_contents->GetBrowserContext(),
            GURL("chrome-guest://mfffpogegjflfpflabcdkioaeobkgjik/?" +
                 partition_id_));

    scoped_refptr<SigninManagerCookieHelper> cookie_helper(
        new SigninManagerCookieHelper(partition->GetURLRequestContext()));
    cookie_helper->StartFetchingCookiesOnUIThread(
        GURL(GaiaUrls::GetInstance()->client_login_to_oauth2_url()),
        base::Bind(&InlineLoginUIHandler::OnGaiaCookiesFetched,
                   weak_factory_.GetWeakPtr(), email, password));
#endif
  }

  void OnGaiaCookiesFetched(
      const string16 email,
      const string16 password,
      const net::CookieList& cookie_list) {
    net::CookieList::const_iterator it;
    std::string oauth_code;
    for (it = cookie_list.begin(); it != cookie_list.end(); ++it) {
      if (it->Name() == "oauth_code") {
        oauth_code = it->Value();
        break;
      }
    }

    DCHECK(!oauth_code.empty());
    content::WebContents* contents = web_ui()->GetWebContents();
    ProfileSyncService* sync_service =
        ProfileSyncServiceFactory::GetForProfile(profile_);
    const GURL& current_url = contents->GetURL();
    signin::Source source = signin::GetSourceForPromoURL(current_url);

    if (source == signin::SOURCE_AVATAR_BUBBLE_ADD_ACCOUNT) {
      // SigninOAuthHelper will delete itself.
      SigninOAuthHelper* helper = new SigninOAuthHelper(profile_);
      helper->StartAddingAccount(oauth_code);
    } else {
      OneClickSigninSyncStarter::StartSyncMode start_mode =
          source == signin::SOURCE_SETTINGS || choose_what_to_sync_ ?
              (SigninGlobalError::GetForProfile(profile_)->HasMenuItem() &&
                sync_service && sync_service->HasSyncSetupCompleted()) ?
                  OneClickSigninSyncStarter::SHOW_SETTINGS_WITHOUT_CONFIGURE :
                  OneClickSigninSyncStarter::CONFIGURE_SYNC_FIRST :
              OneClickSigninSyncStarter::SYNC_WITH_DEFAULT_SETTINGS;
      OneClickSigninSyncStarter::ConfirmationRequired confirmation_required =
          source == signin::SOURCE_SETTINGS ||
          source == signin::SOURCE_WEBSTORE_INSTALL ||
          choose_what_to_sync_?
              OneClickSigninSyncStarter::NO_CONFIRMATION :
              OneClickSigninSyncStarter::CONFIRM_AFTER_SIGNIN;
      // Call OneClickSigninSyncStarter to exchange oauth code for tokens.
      // OneClickSigninSyncStarter will delete itself once the job is done.
      new OneClickSigninSyncStarter(
          profile_, NULL, "" /* session_index, not used */,
          UTF16ToASCII(email), UTF16ToASCII(password), oauth_code,
          start_mode,
          contents,
          confirmation_required,
          base::Bind(&InlineLoginUIHandler::SyncStarterCallback,
                      weak_factory_.GetWeakPtr()));
    }

    web_ui()->CallJavascriptFunction("inline.login.closeDialog");
  }

  void SyncStarterCallback(OneClickSigninSyncStarter::SyncSetupResult result) {
    content::WebContents* contents = web_ui()->GetWebContents();
    const GURL& current_url = contents->GetURL();
    bool auto_close = signin::IsAutoCloseEnabledInURL(current_url);
    signin::Source source = signin::GetSourceForPromoURL(current_url);
    if (auto_close) {
      base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(
            &InlineLoginUIHandler::CloseTab, weak_factory_.GetWeakPtr()));
    } else if (source != signin::SOURCE_UNKNOWN &&
        source != signin::SOURCE_SETTINGS &&
        source != signin::SOURCE_WEBSTORE_INSTALL) {
      // Redirect to NTP/Apps page and display a confirmation bubble.
      // TODO(guohui): should redirect to the given continue url for webstore
      // install flows.
      GURL url(source == signin::SOURCE_APPS_PAGE_LINK ?
               chrome::kChromeUIAppsURL : chrome::kChromeUINewTabURL);
      content::OpenURLParams params(url,
                                    content::Referrer(),
                                    CURRENT_TAB,
                                    content::PAGE_TRANSITION_AUTO_TOPLEVEL,
                                    false);
      contents->OpenURL(params);
    }
  }

  void CloseTab() {
    content::WebContents* tab = web_ui()->GetWebContents();
    Browser* browser = chrome::FindBrowserWithWebContents(tab);
    if (browser) {
      TabStripModel* tab_strip_model = browser->tab_strip_model();
      if (tab_strip_model) {
        int index = tab_strip_model->GetIndexOfWebContents(tab);
        if (index != TabStripModel::kNoTab) {
          tab_strip_model->ExecuteContextMenuCommand(
              index, TabStripModel::CommandCloseTab);
        }
      }
    }
  }

  Profile* profile_;
  base::WeakPtrFactory<InlineLoginUIHandler> weak_factory_;
  bool choose_what_to_sync_;
  // Partition id for the gaia webview;
  std::string partition_id_;

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
  // Required for intercepting extension function calls when the page is loaded
  // in a bubble (not a full tab, thus tab helpers are not registered
  // automatically).
  extensions::TabHelper::CreateForWebContents(web_ui->GetWebContents());
}

InlineLoginUI::~InlineLoginUI() {}
