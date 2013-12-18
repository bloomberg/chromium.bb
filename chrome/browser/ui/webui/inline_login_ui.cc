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
#include "chrome/browser/signin/signin_names_io_thread.h"
#include "chrome/browser/signin/signin_oauth_helper.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/sync/one_click_signin_helper.h"
#include "chrome/browser/ui/sync/one_click_signin_sync_starter.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "google_apis/gaia/gaia_auth_consumer.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "google_apis/gaia/gaia_constants.h"
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
    ProfileOAuth2TokenService* token_service =
        ProfileOAuth2TokenServiceFactory::GetForProfile(profile);
    token_service->UpdateCredentials(token_service->GetPrimaryAccountId(),
                                     oauth2_tokens.refresh_token);
  }

  virtual void OnOAuth2TokensFetchFailed() OVERRIDE {
    LOG(ERROR) << "Failed to fetch oauth2 token with inline login.";
    web_ui_->CallJavascriptFunction("inline.login.handleOAuth2TokenFailure");
  }

 private:
  content::WebUI* web_ui_;
};
#else
// Global SequenceNumber used for generating unique webview partition IDs.
base::StaticAtomicSequenceNumber next_partition_id;
#endif // OS_CHROMEOS

class InlineLoginUIHandler : public GaiaAuthConsumer,
                             public content::WebUIMessageHandler {
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
    web_ui()->RegisterMessageCallback("switchToFullTab",
        base::Bind(&InlineLoginUIHandler::HandleSwitchToFullTab,
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
#if !defined(OS_CHROMEOS)
    if (enable_inline) {

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
      params.SetString("continueUrl",
          signin::GetLandingURL("source", static_cast<int>(source)).spec());

      std::string email;
      net::GetValueForKeyInQuery(current_url, "Email", &email);
      if (!email.empty())
        params.SetString("email", email);

      std::string frame_url;
      net::GetValueForKeyInQuery(current_url, "frameUrl", &frame_url);
      if (!frame_url.empty())
        params.SetString("frameUrl", frame_url);

      std::string is_constrained;
      net::GetValueForKeyInQuery(current_url, "constrained", &is_constrained);
      if (!is_constrained.empty())
        params.SetString("constrained", is_constrained);

      net::GetValueForKeyInQuery(current_url, "partitionId", &partition_id_);
      if (partition_id_.empty()) {
        partition_id_ =
            "gaia-webview-" + base::IntToString(next_partition_id.GetNext());
      }
      params.SetString("partitionId", partition_id_);
    }
#endif // OS_CHROMEOS

    web_ui()->CallJavascriptFunction("inline.login.loadAuthExtension", params);
  }

  // JS callback:
  void HandleInitialize(const base::ListValue* args) {
    LoadAuthExtension();
  }

  // JS callback:
  void HandleSwitchToFullTab(const base::ListValue* args) {
    base::string16 url_str;
    CHECK(args->GetString(0, &url_str));

    content::WebContents* web_contents = web_ui()->GetWebContents();
    GURL main_frame_url(web_contents->GetURL());
    main_frame_url = net::AppendOrReplaceQueryParameter(
        main_frame_url, "frameUrl", UTF16ToASCII(url_str));
    main_frame_url = net::AppendOrReplaceQueryParameter(
        main_frame_url, "partitionId", partition_id_);
    chrome::NavigateParams params(
        profile_,
        net::AppendOrReplaceQueryParameter(main_frame_url, "constrained", "0"),
        content::PAGE_TRANSITION_AUTO_TOPLEVEL);
    chrome::Navigate(&params);

    web_ui()->CallJavascriptFunction("inline.login.closeDialog");
  }

  void HandleCompleteLogin(const base::ListValue* args) {
    // TODO(guohui, xiyuan): we should investigate if it is possible to unify
    // the signin-with-cookies flow across ChromeOS and Chrome.
    DCHECK(email_.empty() && password_.empty());

#if defined(OS_CHROMEOS)
    oauth2_delegate_.reset(new InlineLoginUIOAuth2Delegate(web_ui()));
    oauth2_token_fetcher_.reset(new chromeos::OAuth2TokenFetcher(
        oauth2_delegate_.get(), profile_->GetRequestContext()));
    oauth2_token_fetcher_->StartExchangeFromCookies();
#else
    const base::DictionaryValue* dict = NULL;
    base::string16 email;
    if (!args->GetDictionary(0, &dict) || !dict ||
        !dict->GetString("email", &email)) {
      // User cancelled the signin by clicking 'skip for now'.
      bool skip_for_now = false;
      DCHECK(dict->GetBoolean("skipForNow", &skip_for_now) && skip_for_now);

      signin::SetUserSkippedPromo(profile_);
      SyncStarterCallback(OneClickSigninSyncStarter::SYNC_SETUP_FAILURE);
      return;
    }

    email_ = UTF16ToASCII(email);
    base::string16 password;
    dict->GetString("password", &password);
    password_ = UTF16ToASCII(password);

    dict->GetBoolean("chooseWhatToSync", &choose_what_to_sync_);

    content::WebContents* contents = web_ui()->GetWebContents();
    signin::Source source = signin::GetSourceForPromoURL(contents->GetURL());
    OneClickSigninHelper::CanOfferFor can_offer =
        source == signin::SOURCE_AVATAR_BUBBLE_ADD_ACCOUNT ?
        OneClickSigninHelper::CAN_OFFER_FOR_SECONDARY_ACCOUNT :
        OneClickSigninHelper::CAN_OFFER_FOR_ALL;
    std::string error_msg;
    OneClickSigninHelper::CanOffer(
        contents, can_offer, email_, &error_msg);
    if (!error_msg.empty()) {
      HandleLoginError(error_msg);
      return;
    }

    content::StoragePartition* partition =
        content::BrowserContext::GetStoragePartitionForSite(
            contents->GetBrowserContext(),
            GURL("chrome-guest://mfffpogegjflfpflabcdkioaeobkgjik/?" +
                 partition_id_));

    auth_fetcher_.reset(new GaiaAuthFetcher(this,
                                            GaiaConstants::kChromeSource,
                                            partition->GetURLRequestContext()));
    auth_fetcher_->StartCookieForOAuthCodeExchange("0");
#endif // OS_CHROMEOS
  }

  // GaiaAuthConsumer override.
  virtual void OnClientOAuthCodeSuccess(
      const std::string& oauth_code) OVERRIDE {
#if !defined(OS_CHROMEOS)
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
      OneClickSigninSyncStarter::Callback sync_callback = base::Bind(
          &InlineLoginUIHandler::SyncStarterCallback,
          weak_factory_.GetWeakPtr());

      bool cross_account_error_handled =
          OneClickSigninHelper::HandleCrossAccountError(
              contents, "" /* session_index, not used */,
              email_, password_, oauth_code,
              OneClickSigninHelper::AUTO_ACCEPT_EXPLICIT,
              source, start_mode, sync_callback);

      if (!cross_account_error_handled) {
        // Call OneClickSigninSyncStarter to exchange oauth code for tokens.
        // OneClickSigninSyncStarter will delete itself once the job is done.
        new OneClickSigninSyncStarter(
            profile_, NULL, "" /* session_index, not used */,
            email_, password_, oauth_code,
            start_mode,
            contents,
            confirmation_required,
            sync_callback);
      }
    }

    email_.clear();
    password_.clear();
    web_ui()->CallJavascriptFunction("inline.login.closeDialog");
#endif // OS_CHROMEOS
  }

  // GaiaAuthConsumer override.
  virtual void OnClientOAuthCodeFailure(const GoogleServiceAuthError& error)
      OVERRIDE {
#if !defined(OS_CHROMEOS)
    LOG(ERROR) << "InlineLoginUI::OnClientOAuthCodeFailure";
    HandleLoginError(error.ToString());
#endif // OS_CHROMEOS
  }

  void HandleLoginError(const std::string& error_msg) {
    SyncStarterCallback(
        OneClickSigninSyncStarter::SYNC_SETUP_FAILURE);

    Browser* browser = chrome::FindBrowserWithWebContents(
        web_ui()->GetWebContents());
    if (!browser) {
      browser = chrome::FindLastActiveWithProfile(
          profile_, chrome::GetActiveDesktop());
    }
    if (browser)
      OneClickSigninHelper::ShowSigninErrorBubble(browser, error_msg);

    email_.clear();
    password_.clear();
  }

  void SyncStarterCallback(OneClickSigninSyncStarter::SyncSetupResult result) {
    content::WebContents* contents = web_ui()->GetWebContents();
    const GURL& current_url = contents->GetURL();

    if (signin::IsAutoCloseEnabledInURL(current_url)) {
      base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(
            &InlineLoginUIHandler::CloseTab, weak_factory_.GetWeakPtr()));
    } else {
      signin::Source source = signin::GetSourceForPromoURL(current_url);
      DCHECK(source != signin::SOURCE_UNKNOWN);
      OneClickSigninHelper::RedirectToNtpOrAppsPageIfNecessary(
          contents, source);
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
  scoped_ptr<GaiaAuthFetcher> auth_fetcher_;
  std::string email_;
  std::string password_;
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
