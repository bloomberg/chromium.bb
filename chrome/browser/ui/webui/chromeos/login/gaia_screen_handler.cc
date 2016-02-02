// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/gaia_screen_handler.h"

#include "ash/system/chromeos/devicetype_utils.h"
#include "base/bind.h"
#include "base/guid.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/chromeos/input_method/input_method_util.h"
#include "chrome/browser/chromeos/language_preferences.h"
#include "chrome/browser/chromeos/login/screens/network_error.h"
#include "chrome/browser/chromeos/login/ui/user_adding_screen.h"
#include "chrome/browser/chromeos/login/users/chrome_user_manager.h"
#include "chrome/browser/chromeos/net/network_portal_detector_impl.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/io_thread.h"
#include "chrome/browser/ui/webui/chromeos/login/signin_screen_handler.h"
#include "chrome/browser/ui/webui/signin/get_auth_frame.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/login/auth/user_context.h"
#include "chromeos/settings/cros_settings_names.h"
#include "chromeos/system/devicetype.h"
#include "chromeos/system/version_loader.h"
#include "components/login/localized_values_builder.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user_manager.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_urls.h"
#include "grit/components_strings.h"
#include "ui/base/ime/chromeos/input_method_manager.h"
#include "ui/base/l10n/l10n_util.h"

using content::BrowserThread;

namespace chromeos {

namespace {

const char kJsScreenPath[] = "login.GaiaSigninScreen";
const char kAuthIframeParentName[] = "signin-frame";

const char kRestrictiveProxyURL[] = "https://www.google.com/generate_204";

const char kEndpointGen[] = "1.0";

std::string GetChromeType() {
  switch (chromeos::GetDeviceType()) {
    case chromeos::DeviceType::kChromebox:
      return "chromebox";
    case chromeos::DeviceType::kChromebase:
      return "chromebase";
    case chromeos::DeviceType::kChromebit:
      return "chromebit";
    case chromeos::DeviceType::kChromebook:
      return "chromebook";
    default:
      return "chromedevice";
  }
}

void UpdateAuthParams(base::DictionaryValue* params,
                      bool is_restrictive_proxy) {
  CrosSettings* cros_settings = CrosSettings::Get();
  bool allow_new_user = true;
  cros_settings->GetBoolean(kAccountsPrefAllowNewUser, &allow_new_user);
  bool allow_guest = true;
  cros_settings->GetBoolean(kAccountsPrefAllowGuest, &allow_guest);
  params->SetBoolean("guestSignin", allow_guest);

  // nosignup flow if new users are not allowed.
  if (!allow_new_user || is_restrictive_proxy)
    params->SetString("flow", "nosignup");

  // Allow supervised user creation only if:
  // 1. Enterprise managed device > is allowed by policy.
  // 2. Consumer device > owner exists.
  // 3. New users are allowed by owner.
  // 4. Supervised users are allowed by owner.
  ChromeUserManager* user_manager = ChromeUserManager::Get();
  bool supervised_users_can_create =
      user_manager->AreSupervisedUsersAllowed() && allow_new_user &&
      !user_manager->GetUsersAllowedForSupervisedUsersCreation().empty();
  params->SetBoolean("supervisedUsersCanCreate", supervised_users_can_create);

  // Now check whether we're in multi-profiles user adding scenario and
  // disable GAIA right panel features if that's the case.
  if (UserAddingScreen::Get()->IsRunning()) {
    params->SetBoolean("guestSignin", false);
    params->SetBoolean("supervisedUsersCanCreate", false);
  }
}

void RecordSAMLScrapingVerificationResultInHistogram(bool success) {
  UMA_HISTOGRAM_BOOLEAN("ChromeOS.SAML.Scraping.VerificationResult", success);
}

// The Task posted to PostTaskAndReply in StartClearingDnsCache on the IO
// thread.
void ClearDnsCache(IOThread* io_thread) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (browser_shutdown::IsTryingToQuit())
    return;

  io_thread->ClearHostCache();
}

void PushFrontIMIfNotExists(const std::string& input_method,
                            std::vector<std::string>* input_methods) {
  if (input_method.empty())
    return;

  if (std::find(input_methods->begin(), input_methods->end(), input_method) ==
      input_methods->end())
    input_methods->insert(input_methods->begin(), input_method);
}

bool IsOnline(NetworkPortalDetector::CaptivePortalStatus status) {
  return status == NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE;
}

}  // namespace

// A class that's used to specify the way how Gaia should be loaded.
struct GaiaScreenHandler::GaiaContext {
  GaiaContext();

  // Forces Gaia to reload.
  bool force_reload = false;

  // Whether Gaia should be loaded in offline mode.
  bool use_offline = false;

  // Email of the current user.
  std::string email;

  // GAIA ID of the current user.
  std::string gaia_id;

  // GAPS cookie.
  std::string gaps_cookie;
};

GaiaScreenHandler::GaiaContext::GaiaContext() {}

GaiaScreenHandler::GaiaScreenHandler(
    CoreOobeActor* core_oobe_actor,
    const scoped_refptr<NetworkStateInformer>& network_state_informer)
    : BaseScreenHandler(kJsScreenPath),
      network_state_informer_(network_state_informer),
      core_oobe_actor_(core_oobe_actor),
      weak_factory_(this) {
  DCHECK(network_state_informer_.get());
}

GaiaScreenHandler::~GaiaScreenHandler() {
  if (network_portal_detector_)
    network_portal_detector_->RemoveObserver(this);
}

void GaiaScreenHandler::LoadGaia(const GaiaContext& context) {
  base::PostTaskAndReplyWithResult(
      content::BrowserThread::GetBlockingPool(), FROM_HERE,
      base::Bind(&version_loader::GetVersion, version_loader::VERSION_SHORT),
      base::Bind(&GaiaScreenHandler::LoadGaiaWithVersion,
                 weak_factory_.GetWeakPtr(), context));
}

void GaiaScreenHandler::LoadGaiaWithVersion(
    const GaiaContext& context,
    const std::string& platform_version) {
  base::DictionaryValue params;

  params.SetBoolean("forceReload", context.force_reload);
  params.SetBoolean("useOffline", context.use_offline);
  params.SetString("gaiaId", context.gaia_id);
  params.SetBoolean("readOnlyEmail", true);
  params.SetString("email", context.email);
  params.SetString("gapsCookie", context.gaps_cookie);

  UpdateAuthParams(&params, IsRestrictiveProxy());

  if (!context.use_offline) {
    const std::string app_locale = g_browser_process->GetApplicationLocale();
    if (!app_locale.empty())
      params.SetString("hl", app_locale);
  } else {
    policy::BrowserPolicyConnectorChromeOS* connector =
        g_browser_process->platform_part()->browser_policy_connector_chromeos();
    std::string enterprise_domain(connector->GetEnterpriseDomain());
    if (!enterprise_domain.empty()) {
      params.SetString(
          "enterpriseInfoMessage",
          l10n_util::GetStringFUTF16(IDS_OFFLINE_LOGIN_DEVICE_MANAGED_BY_NOTICE,
                                     base::UTF8ToUTF16(enterprise_domain)));
    }
  }

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  std::string enterprise_domain(connector->GetEnterpriseDomain());
  if (!enterprise_domain.empty())
    params.SetString("enterpriseDomain", enterprise_domain);

  params.SetString("chromeType", GetChromeType());
  params.SetString("clientId",
                   GaiaUrls::GetInstance()->oauth2_chrome_client_id());
  params.SetString("clientVersion", version_info::GetVersionNumber());
  if (!platform_version.empty())
    params.SetString("platformVersion", platform_version);
  params.SetString("releaseChannel", chrome::GetChannelString());
  params.SetString("endpointGen", kEndpointGen);

  std::string email_domain;
  if (CrosSettings::Get()->GetString(kAccountsPrefLoginScreenDomainAutoComplete,
                                     &email_domain) &&
      !email_domain.empty()) {
    params.SetString("emailDomain", email_domain);
  }

  params.SetString("gaiaUrl", GaiaUrls::GetInstance()->gaia_url().spec());

  if (use_easy_bootstrap_) {
    params.SetBoolean("useEafe", true);
    // Easy login overrides.
    std::string eafe_url = "https://easylogin.corp.google.com/";
    if (command_line->HasSwitch(switches::kEafeUrl))
      eafe_url = command_line->GetSwitchValueASCII(switches::kEafeUrl);
    std::string eafe_path = "planters/cbaudioChrome";
    if (command_line->HasSwitch(switches::kEafePath))
      eafe_path = command_line->GetSwitchValueASCII(switches::kEafePath);

    params.SetString("gaiaUrl", eafe_url);
    params.SetString("gaiaPath", eafe_path);
    params.SetString("clientId",
                     GaiaUrls::GetInstance()->oauth2_chrome_client_id());
  }

  frame_state_ = FRAME_STATE_LOADING;
  CallJS("loadAuthExtension", params);
}

void GaiaScreenHandler::ReloadGaia(bool force_reload) {
  if (frame_state_ == FRAME_STATE_LOADING && !force_reload) {
    VLOG(1) << "Skipping reloading of Gaia since gaia is loading.";
    return;
  }
  NetworkStateInformer::State state = network_state_informer_->state();
  if (state != NetworkStateInformer::ONLINE) {
    VLOG(1) << "Skipping reloading of Gaia since network state="
            << NetworkStateInformer::StatusString(state);
    return;
  }
  VLOG(1) << "Reloading Gaia.";
  frame_state_ = FRAME_STATE_LOADING;
  CallJS("doReload");
}

void GaiaScreenHandler::MonitorOfflineIdle(bool is_online) {
  CallJS("monitorOfflineIdle", is_online);
}

void GaiaScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("signinScreenTitle", IDS_SIGNIN_SCREEN_TITLE_TAB_PROMPT);
  builder->Add("guestSignin", IDS_BROWSE_WITHOUT_SIGNING_IN_HTML);
  builder->Add("backButton", IDS_ACCNAME_BACK);
  builder->Add("closeButton", IDS_CLOSE);
  builder->Add("whitelistErrorConsumer", IDS_LOGIN_ERROR_WHITELIST);
  builder->Add("whitelistErrorEnterprise",
               IDS_ENTERPRISE_LOGIN_ERROR_WHITELIST);
  builder->Add("tryAgainButton", IDS_WHITELIST_ERROR_TRY_AGAIN_BUTTON);
  builder->Add("learnMoreButton", IDS_WHITELIST_ERROR_LEARN_MORE_BUTTON);
  builder->Add("gaiaLoading", IDS_LOGIN_GAIA_LOADING_MESSAGE);

  // Strings used by the SAML fatal error dialog.
  builder->Add("fatalErrorMessageNoAccountDetails",
               IDS_LOGIN_FATAL_ERROR_NO_ACCOUNT_DETAILS);
  builder->Add("fatalErrorMessageNoPassword",
               IDS_LOGIN_FATAL_ERROR_NO_PASSWORD);
  builder->Add("fatalErrorMessageVerificationFailed",
               IDS_LOGIN_FATAL_ERROR_PASSWORD_VERIFICATION);
  builder->Add("fatalErrorMessageInsecureURL",
               IDS_LOGIN_FATAL_ERROR_TEXT_INSECURE_URL);
  builder->Add("fatalErrorDoneButton", IDS_DONE);
  builder->Add("fatalErrorTryAgainButton",
               IDS_LOGIN_FATAL_ERROR_TRY_AGAIN_BUTTON);

  builder->AddF("offlineLoginWelcome", IDS_OFFLINE_LOGIN_WELCOME,
                ash::GetChromeOSDeviceTypeResourceId());
  builder->Add("offlineLoginEmail", IDS_OFFLINE_LOGIN_EMAIL);
  builder->Add("offlineLoginPassword", IDS_OFFLINE_LOGIN_PASSWORD);
  builder->Add("offlineLoginInvalidEmail", IDS_OFFLINE_LOGIN_INVALID_EMAIL);
  builder->Add("offlineLoginInvalidPassword",
               IDS_OFFLINE_LOGIN_INVALID_PASSWORD);
  builder->Add("offlineLoginNextBtn", IDS_OFFLINE_LOGIN_NEXT_BUTTON_TEXT);
  builder->Add("offlineLoginForgotPasswordBtn",
               IDS_OFFLINE_LOGIN_FORGOT_PASSWORD_BUTTON_TEXT);
  builder->Add("offlineLoginForgotPasswordDlg",
               IDS_OFFLINE_LOGIN_FORGOT_PASSWORD_DIALOG_TEXT);
  builder->Add("offlineLoginCloseBtn", IDS_OFFLINE_LOGIN_CLOSE_BUTTON_TEXT);
}

void GaiaScreenHandler::Initialize() {
}

void GaiaScreenHandler::RegisterMessages() {
  AddCallback("webviewLoadAborted",
              &GaiaScreenHandler::HandleWebviewLoadAborted);
  AddCallback("completeLogin", &GaiaScreenHandler::HandleCompleteLogin);
  AddCallback("completeAuthentication",
              &GaiaScreenHandler::HandleCompleteAuthentication);
  AddCallback("completeAuthenticationAuthCodeOnly",
              &GaiaScreenHandler::HandleCompleteAuthenticationAuthCodeOnly);
  AddCallback("usingSAMLAPI", &GaiaScreenHandler::HandleUsingSAMLAPI);
  AddCallback("scrapedPasswordCount",
              &GaiaScreenHandler::HandleScrapedPasswordCount);
  AddCallback("scrapedPasswordVerificationFailed",
              &GaiaScreenHandler::HandleScrapedPasswordVerificationFailed);
  AddCallback("loginWebuiReady", &GaiaScreenHandler::HandleGaiaUIReady);
  AddCallback("toggleEasyBootstrap",
              &GaiaScreenHandler::HandleToggleEasyBootstrap);
  AddCallback("identifierEntered", &GaiaScreenHandler::HandleIdentifierEntered);
  AddCallback("updateOfflineLogin",
              &GaiaScreenHandler::set_offline_login_is_active);
}

OobeUI::Screen GaiaScreenHandler::GetCurrentScreen() const {
  OobeUI::Screen screen = OobeUI::SCREEN_UNKNOWN;
  OobeUI* oobe_ui = static_cast<OobeUI*>(web_ui()->GetController());
  if (oobe_ui)
    screen = oobe_ui->current_screen();
  return screen;
}

void GaiaScreenHandler::OnPortalDetectionCompleted(
    const NetworkState* network,
    const NetworkPortalDetector::CaptivePortalState& state) {
  VLOG(1) << "OnPortalDetectionCompleted "
          << NetworkPortalDetector::CaptivePortalStatusString(state.status);

  const NetworkPortalDetector::CaptivePortalStatus previous_status =
      captive_portal_status_;
  captive_portal_status_ = state.status;
  if (offline_login_is_active() ||
      IsOnline(captive_portal_status_) == IsOnline(previous_status) ||
      disable_restrictive_proxy_check_for_test_ ||
      GetCurrentScreen() != OobeUI::SCREEN_GAIA_SIGNIN)
    return;

  LoadAuthExtension(true /* force */, false /* offline */);
}

void GaiaScreenHandler::HandleIdentifierEntered(const std::string& user_email) {
  if (!Delegate()->IsUserWhitelisted(user_manager::known_user::GetAccountId(
          user_email, std::string() /* gaia_id */)))
    ShowWhitelistCheckFailedError();
}

void GaiaScreenHandler::HandleWebviewLoadAborted(
    const std::string& error_reason_str) {
  // TODO(nkostylev): Switch to int code once webview supports that.
  // http://crbug.com/470483
  if (error_reason_str == "ERR_ABORTED") {
    LOG(WARNING) << "Ignoring Gaia webview error: " << error_reason_str;
    return;
  }

  // TODO(nkostylev): Switch to int code once webview supports that.
  // http://crbug.com/470483
  // Extract some common codes used by SigninScreenHandler for now.
  if (error_reason_str == "ERR_NAME_NOT_RESOLVED")
    frame_error_ = net::ERR_NAME_NOT_RESOLVED;
  else if (error_reason_str == "ERR_INTERNET_DISCONNECTED")
    frame_error_ = net::ERR_INTERNET_DISCONNECTED;
  else if (error_reason_str == "ERR_NETWORK_CHANGED")
    frame_error_ = net::ERR_NETWORK_CHANGED;
  else if (error_reason_str == "ERR_INTERNET_DISCONNECTED")
    frame_error_ = net::ERR_INTERNET_DISCONNECTED;
  else if (error_reason_str == "ERR_PROXY_CONNECTION_FAILED")
    frame_error_ = net::ERR_PROXY_CONNECTION_FAILED;
  else if (error_reason_str == "ERR_TUNNEL_CONNECTION_FAILED")
    frame_error_ = net::ERR_TUNNEL_CONNECTION_FAILED;
  else
    frame_error_ = net::ERR_INTERNET_DISCONNECTED;

  LOG(ERROR) << "Gaia webview error: " << error_reason_str;
  NetworkError::ErrorReason error_reason =
      NetworkError::ERROR_REASON_FRAME_ERROR;
  frame_state_ = FRAME_STATE_ERROR;
  UpdateState(error_reason);
}

AccountId GaiaScreenHandler::GetAccountId(
    const std::string& authenticated_email,
    const std::string& gaia_id) const {
  const std::string canonicalized_email =
      gaia::CanonicalizeEmail(gaia::SanitizeEmail(authenticated_email));

  const AccountId account_id =
      user_manager::known_user::GetAccountId(authenticated_email, gaia_id);

  if (account_id.GetUserEmail() != canonicalized_email) {
    LOG(WARNING) << "Existing user '" << account_id.GetUserEmail()
                 << "' authenticated by alias '" << canonicalized_email << "'.";
  }

  return account_id;
}

void GaiaScreenHandler::HandleCompleteAuthentication(
    const std::string& gaia_id,
    const std::string& email,
    const std::string& password,
    const std::string& auth_code,
    bool using_saml,
    const std::string& gaps_cookie) {
  if (!Delegate())
    return;

  DCHECK(!email.empty());
  DCHECK(!gaia_id.empty());
  const std::string sanitized_email = gaia::SanitizeEmail(email);
  Delegate()->SetDisplayEmail(sanitized_email);

  UserContext user_context(GetAccountId(email, gaia_id));
  user_context.SetGaiaID(gaia_id);
  user_context.SetKey(Key(password));
  user_context.SetAuthCode(auth_code);
  user_context.SetAuthFlow(using_saml
                               ? UserContext::AUTH_FLOW_GAIA_WITH_SAML
                               : UserContext::AUTH_FLOW_GAIA_WITHOUT_SAML);
  user_context.SetGAPSCookie(gaps_cookie);
  Delegate()->CompleteLogin(user_context);
}

void GaiaScreenHandler::HandleCompleteAuthenticationAuthCodeOnly(
    const std::string& auth_code) {
  if (!Delegate())
    return;

  UserContext user_context;
  user_context.SetAuthFlow(UserContext::AUTH_FLOW_EASY_BOOTSTRAP);
  user_context.SetAuthCode(auth_code);
  Delegate()->CompleteLogin(user_context);
}

void GaiaScreenHandler::HandleCompleteLogin(const std::string& gaia_id,
                                            const std::string& typed_email,
                                            const std::string& password,
                                            bool using_saml) {
  DoCompleteLogin(gaia_id, typed_email, password, using_saml);
}

void GaiaScreenHandler::HandleUsingSAMLAPI() {
  SetSAMLPrincipalsAPIUsed(true);
}

void GaiaScreenHandler::HandleScrapedPasswordCount(int password_count) {
  SetSAMLPrincipalsAPIUsed(false);
  // Use a histogram that has 11 buckets, one for each of the values in [0, 9]
  // and an overflow bucket at the end.
  UMA_HISTOGRAM_ENUMERATION(
      "ChromeOS.SAML.Scraping.PasswordCount", std::min(password_count, 10), 11);
  if (password_count == 0)
    HandleScrapedPasswordVerificationFailed();
}

void GaiaScreenHandler::HandleScrapedPasswordVerificationFailed() {
  RecordSAMLScrapingVerificationResultInHistogram(false);
}

void GaiaScreenHandler::HandleToggleEasyBootstrap() {
  use_easy_bootstrap_ = !use_easy_bootstrap_;
  LoadAuthExtension(true /* force */, false /* offline */);
}

void GaiaScreenHandler::HandleGaiaUIReady() {
  VLOG(1) << "Gaia is loaded";

  // As we could miss and window.onload could already be called, restore
  // focus to current pod (see crbug/175243).
  if (gaia_silent_load_)
    signin_screen_handler_->RefocusCurrentPod();

  frame_error_ = net::OK;
  frame_state_ = FRAME_STATE_LOADED;

  if (network_state_informer_->state() == NetworkStateInformer::ONLINE)
    UpdateState(NetworkError::ERROR_REASON_UPDATE);

  if (test_expects_complete_login_)
    SubmitLoginFormForTest();
}

void GaiaScreenHandler::DoCompleteLogin(const std::string& gaia_id,
                                        const std::string& typed_email,
                                        const std::string& password,
                                        bool using_saml) {
  if (!Delegate())
    return;

  if (using_saml && !using_saml_api_)
    RecordSAMLScrapingVerificationResultInHistogram(true);

  DCHECK(!typed_email.empty());
  DCHECK(!gaia_id.empty());
  const std::string sanitized_email = gaia::SanitizeEmail(typed_email);
  Delegate()->SetDisplayEmail(sanitized_email);
  UserContext user_context(GetAccountId(typed_email, gaia_id));
  user_context.SetGaiaID(gaia_id);
  user_context.SetKey(Key(password));
  user_context.SetAuthFlow(using_saml
                               ? UserContext::AUTH_FLOW_GAIA_WITH_SAML
                               : UserContext::AUTH_FLOW_GAIA_WITHOUT_SAML);
  Delegate()->CompleteLogin(user_context);

  if (test_expects_complete_login_) {
    VLOG(2) << "Complete test login for " << typed_email
            << ", requested=" << test_user_;

    test_expects_complete_login_ = false;
    test_user_.clear();
    test_pass_.clear();
  }
}

void GaiaScreenHandler::StartClearingDnsCache() {
  if (dns_clear_task_running_ || !g_browser_process->io_thread())
    return;

  dns_cleared_ = false;
  BrowserThread::PostTaskAndReply(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&ClearDnsCache, g_browser_process->io_thread()),
      base::Bind(&GaiaScreenHandler::OnDnsCleared, weak_factory_.GetWeakPtr()));
  dns_clear_task_running_ = true;
}

void GaiaScreenHandler::OnDnsCleared() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  dns_clear_task_running_ = false;
  dns_cleared_ = true;
  ShowGaiaScreenIfReady();
}

void GaiaScreenHandler::StartClearingCookies(
    const base::Closure& on_clear_callback) {
  cookies_cleared_ = false;
  ProfileHelper* profile_helper = ProfileHelper::Get();
  LOG_ASSERT(Profile::FromWebUI(web_ui()) ==
             profile_helper->GetSigninProfile());
  profile_helper->ClearSigninProfile(
      base::Bind(&GaiaScreenHandler::OnCookiesCleared,
                 weak_factory_.GetWeakPtr(), on_clear_callback));
}

void GaiaScreenHandler::OnCookiesCleared(
    const base::Closure& on_clear_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  cookies_cleared_ = true;
  on_clear_callback.Run();
}

void GaiaScreenHandler::ShowSigninScreenForTest(const std::string& username,
                                                 const std::string& password) {
  VLOG(2) << "ShowSigninScreenForTest for user " << username
          << ", frame_state=" << frame_state();

  test_user_ = username;
  test_pass_ = password;
  test_expects_complete_login_ = true;

  // Submit login form for test if gaia is ready. If gaia is loading, login
  // will be attempted in HandleLoginWebuiReady after gaia is ready. Otherwise,
  // reload gaia then follow the loading case.
  if (frame_state() == GaiaScreenHandler::FRAME_STATE_LOADED) {
    SubmitLoginFormForTest();
  } else if (frame_state() != GaiaScreenHandler::FRAME_STATE_LOADING) {
    signin_screen_handler_->OnShowAddUser();
  }
}

void GaiaScreenHandler::SubmitLoginFormForTest() {
  VLOG(2) << "Submit login form for test, user=" << test_user_;

  content::RenderFrameHost* frame =
      signin::GetAuthFrame(web_ui()->GetWebContents(), kAuthIframeParentName);

  std::string code =
      "document.getElementById('identifier').value = '" + test_user_ + "';"
      "document.getElementById('nextButton').click();";
  frame->ExecuteJavaScriptForTests(base::ASCIIToUTF16(code));

  if (!test_pass_.empty()) {
    code = "document.getElementById('password').value = '" + test_pass_ + "';";
    code += "document.getElementById('nextButton').click();";
    frame->ExecuteJavaScriptForTests(base::ASCIIToUTF16(code));
  }

  // Test properties are cleared in HandleCompleteLogin because the form
  // submission might fail and login will not be attempted after reloading
  // if they are cleared here.
}

void GaiaScreenHandler::SetSAMLPrincipalsAPIUsed(bool api_used) {
  using_saml_api_ = api_used;
  UMA_HISTOGRAM_BOOLEAN("ChromeOS.SAML.APIUsed", api_used);
}

void GaiaScreenHandler::ShowGaiaAsync() {
  show_when_dns_and_cookies_cleared_ = true;
  if (gaia_silent_load_ && populated_email_.empty()) {
    dns_cleared_ = true;
    cookies_cleared_ = true;
    ShowGaiaScreenIfReady();
  } else {
    StartClearingDnsCache();
    StartClearingCookies(base::Bind(&GaiaScreenHandler::ShowGaiaScreenIfReady,
                                    weak_factory_.GetWeakPtr()));
  }
}

void GaiaScreenHandler::CancelShowGaiaAsync() {
  show_when_dns_and_cookies_cleared_ = false;
}

void GaiaScreenHandler::ShowGaiaScreenIfReady() {
  if (!dns_cleared_ ||
      !cookies_cleared_ ||
      !show_when_dns_and_cookies_cleared_ ||
      !Delegate()) {
    return;
  }

  std::string active_network_path = network_state_informer_->network_path();
  if (gaia_silent_load_ &&
      (network_state_informer_->state() != NetworkStateInformer::ONLINE ||
       gaia_silent_load_network_ != active_network_path)) {
    // Network has changed. Force Gaia reload.
    gaia_silent_load_ = false;
  }

  // Note that LoadAuthExtension clears |populated_email_|.
  if (populated_email_.empty()) {
    Delegate()->LoadSigninWallpaper();
  } else {
    Delegate()->LoadWallpaper(user_manager::known_user::GetAccountId(
        populated_email_, std::string()));
  }

  input_method::InputMethodManager* imm =
      input_method::InputMethodManager::Get();

  scoped_refptr<input_method::InputMethodManager::State> gaia_ime_state =
      imm->GetActiveIMEState()->Clone();
  imm->SetState(gaia_ime_state);

  // Set Least Recently Used input method for the user.
  if (!populated_email_.empty()) {
    SigninScreenHandler::SetUserInputMethod(populated_email_,
                                            gaia_ime_state.get());
  } else {
    std::vector<std::string> input_methods =
        imm->GetInputMethodUtil()->GetHardwareLoginInputMethodIds();
    const std::string owner_im = SigninScreenHandler::GetUserLRUInputMethod(
        user_manager::UserManager::Get()->GetOwnerAccountId().GetUserEmail());
    const std::string system_im = g_browser_process->local_state()->GetString(
        language_prefs::kPreferredKeyboardLayout);

    PushFrontIMIfNotExists(owner_im, &input_methods);
    PushFrontIMIfNotExists(system_im, &input_methods);

    gaia_ime_state->EnableLoginLayouts(
        g_browser_process->GetApplicationLocale(), input_methods);

    if (!system_im.empty()) {
      gaia_ime_state->ChangeInputMethod(system_im, false /* show_message */);
    } else if (!owner_im.empty()) {
      gaia_ime_state->ChangeInputMethod(owner_im, false /* show_message */);
    }
  }

  LoadAuthExtension(!gaia_silent_load_ /* force */, false /* offline */);
  signin_screen_handler_->UpdateUIState(
      SigninScreenHandler::UI_STATE_GAIA_SIGNIN, nullptr);

  if (gaia_silent_load_) {
    // The variable is assigned to false because silently loaded Gaia page was
    // used.
    gaia_silent_load_ = false;
  }
  UpdateState(NetworkError::ERROR_REASON_UPDATE);

  if (core_oobe_actor_) {
    PrefService* prefs = g_browser_process->local_state();
    if (prefs->GetBoolean(prefs::kFactoryResetRequested)) {
      core_oobe_actor_->ShowDeviceResetScreen();
    } else if (prefs->GetBoolean(prefs::kDebuggingFeaturesRequested)) {
      core_oobe_actor_->ShowEnableDebuggingScreen();
    }
  }
}

void GaiaScreenHandler::MaybePreloadAuthExtension() {
  VLOG(1) << "MaybePreloadAuthExtension";

  if (!network_portal_detector_) {
    NetworkPortalDetectorImpl* detector = new NetworkPortalDetectorImpl(
        g_browser_process->system_request_context(), false);
    detector->set_portal_test_url(GURL(kRestrictiveProxyURL));
    network_portal_detector_.reset(detector);
    network_portal_detector_->AddObserver(this);
    network_portal_detector_->Enable(true);
  }

  // If cookies clearing was initiated or |dns_clear_task_running_| then auth
  // extension showing has already been initiated and preloading is pointless.
  if (signin_screen_handler_->ShouldLoadGaia() &&
      !gaia_silent_load_ &&
      !cookies_cleared_ &&
      !dns_clear_task_running_ &&
      network_state_informer_->state() == NetworkStateInformer::ONLINE) {
    gaia_silent_load_ = true;
    gaia_silent_load_network_ = network_state_informer_->network_path();
    LoadAuthExtension(true /* force */, false /* offline */);
  }
}

void GaiaScreenHandler::ShowWhitelistCheckFailedError() {
  base::DictionaryValue params;
  params.SetBoolean("enterpriseManaged",
                    g_browser_process->platform_part()
                        ->browser_policy_connector_chromeos()
                        ->IsEnterpriseManaged());
  CallJS("showWhitelistCheckFailedError", true, params);
}

void GaiaScreenHandler::LoadAuthExtension(bool force,
                                          bool offline) {
  VLOG(1) << "LoadAuthExtension, force: " << force
          << ", offline: " << offline;
  GaiaContext context;
  context.force_reload = force;
  context.use_offline = offline;
  context.email = populated_email_;

  std::string gaia_id;
  if (!context.email.empty() &&
      user_manager::known_user::FindGaiaID(
          AccountId::FromUserEmail(context.email), &gaia_id)) {
    context.gaia_id = gaia_id;
  }

  if (!context.email.empty()) {
    context.gaps_cookie = user_manager::known_user::GetGAPSCookie(
        AccountId::FromUserEmail(gaia::CanonicalizeEmail(context.email)));
  }

  populated_email_.clear();

  LoadGaia(context);
}

void GaiaScreenHandler::UpdateState(NetworkError::ErrorReason reason) {
  if (signin_screen_handler_)
    signin_screen_handler_->UpdateState(reason);
}

SigninScreenHandlerDelegate* GaiaScreenHandler::Delegate() {
  return signin_screen_handler_->delegate_;
}

bool GaiaScreenHandler::IsRestrictiveProxy() const {
  return !disable_restrictive_proxy_check_for_test_ &&
         !IsOnline(captive_portal_status_);
}

void GaiaScreenHandler::DisableRestrictiveProxyCheckForTest() {
  disable_restrictive_proxy_check_for_test_ = true;
}

}  // namespace chromeos
