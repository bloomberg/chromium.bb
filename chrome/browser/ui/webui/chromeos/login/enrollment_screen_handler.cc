// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/enrollment_screen_handler.h"

#include <algorithm>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browsing_data/browsing_data_helper.h"
#include "chrome/browser/browsing_data/browsing_data_remover.h"
#include "chrome/browser/chromeos/login/ui/login_display_host_impl.h"
#include "chrome/browser/chromeos/policy/policy_oauth2_token_fetcher.h"
#include "chrome/browser/extensions/signin/gaia_auth_extension_loader.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chromeos/login/authenticated_user_email_retriever.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "components/policy/core/browser/cloud/message_util.h"
#include "content/public/browser/web_contents.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/url_request/url_request_context_getter.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {
namespace {

const char kJsScreenPath[] = "login.OAuthEnrollmentScreen";

// Enrollment step names.
const char kEnrollmentStepSignin[] = "signin";
const char kEnrollmentStepSuccess[] = "success";

// Enrollment mode strings.
const char* const kModeStrings[EnrollmentScreenActor::ENROLLMENT_MODE_COUNT] =
    { "manual", "forced", "auto", "recovery" };

std::string EnrollmentModeToString(EnrollmentScreenActor::EnrollmentMode mode) {
  CHECK(0 <= mode && mode < EnrollmentScreenActor::ENROLLMENT_MODE_COUNT);
  return kModeStrings[mode];
}

// A helper class that takes care of asynchronously revoking a given token.
class TokenRevoker : public GaiaAuthConsumer {
 public:
  TokenRevoker()
      : gaia_fetcher_(this,
                      GaiaConstants::kChromeOSSource,
                      g_browser_process->system_request_context()) {}
  virtual ~TokenRevoker() {}

  void Start(const std::string& token) {
    gaia_fetcher_.StartRevokeOAuth2Token(token);
  }

  // GaiaAuthConsumer:
  virtual void OnOAuth2RevokeTokenCompleted() OVERRIDE {
    base::MessageLoop::current()->DeleteSoon(FROM_HERE, this);
  }

 private:
  GaiaAuthFetcher gaia_fetcher_;

  DISALLOW_COPY_AND_ASSIGN(TokenRevoker);
};

// Returns network name by service path.
std::string GetNetworkName(const std::string& service_path) {
  const NetworkState* network =
      NetworkHandler::Get()->network_state_handler()->GetNetworkState(
          service_path);
  if (!network)
    return std::string();
  return network->name();
}

bool IsBehindCaptivePortal(NetworkStateInformer::State state,
                           ErrorScreenActor::ErrorReason reason) {
  return state == NetworkStateInformer::CAPTIVE_PORTAL ||
         reason == ErrorScreenActor::ERROR_REASON_PORTAL_DETECTED;
}

bool IsProxyError(NetworkStateInformer::State state,
                  ErrorScreenActor::ErrorReason reason) {
  return state == NetworkStateInformer::PROXY_AUTH_REQUIRED ||
         reason == ErrorScreenActor::ERROR_REASON_PROXY_AUTH_CANCELLED ||
         reason == ErrorScreenActor::ERROR_REASON_PROXY_CONNECTION_FAILED;
}

}  // namespace

// EnrollmentScreenHandler, public ------------------------------

EnrollmentScreenHandler::EnrollmentScreenHandler(
    const scoped_refptr<NetworkStateInformer>& network_state_informer,
    ErrorScreenActor* error_screen_actor)
    : BaseScreenHandler(kJsScreenPath),
      controller_(NULL),
      show_on_init_(false),
      enrollment_mode_(ENROLLMENT_MODE_MANUAL),
      browsing_data_remover_(NULL),
      frame_error_(net::OK),
      network_state_informer_(network_state_informer),
      error_screen_actor_(error_screen_actor),
      weak_ptr_factory_(this) {
  set_async_assets_load_id(OobeUI::kScreenOobeEnrollment);
  DCHECK(network_state_informer_.get());
  DCHECK(error_screen_actor_);
  network_state_informer_->AddObserver(this);

  if (chromeos::LoginDisplayHostImpl::default_host()) {
    chromeos::WebUILoginView* login_view =
        chromeos::LoginDisplayHostImpl::default_host()->GetWebUILoginView();
    if (login_view)
      login_view->AddFrameObserver(this);
  }
}

EnrollmentScreenHandler::~EnrollmentScreenHandler() {
  if (browsing_data_remover_)
    browsing_data_remover_->RemoveObserver(this);
  network_state_informer_->RemoveObserver(this);

  if (chromeos::LoginDisplayHostImpl::default_host()) {
    chromeos::WebUILoginView* login_view =
        chromeos::LoginDisplayHostImpl::default_host()->GetWebUILoginView();
    if (login_view)
      login_view->RemoveFrameObserver(this);
  }
}

// EnrollmentScreenHandler, WebUIMessageHandler implementation --

void EnrollmentScreenHandler::RegisterMessages() {
  AddCallback("oauthEnrollRetrieveAuthenticatedUserEmail",
              &EnrollmentScreenHandler::HandleRetrieveAuthenticatedUserEmail);
  AddCallback("oauthEnrollClose",
              &EnrollmentScreenHandler::HandleClose);
  AddCallback("oauthEnrollCompleteLogin",
              &EnrollmentScreenHandler::HandleCompleteLogin);
  AddCallback("oauthEnrollRetry",
              &EnrollmentScreenHandler::HandleRetry);
  AddCallback("frameLoadingCompleted",
              &EnrollmentScreenHandler::HandleFrameLoadingCompleted);
}

// EnrollmentScreenHandler
//      EnrollmentScreenActor implementation -----------------------------------

void EnrollmentScreenHandler::SetParameters(
    Controller* controller,
    EnrollmentMode enrollment_mode,
    const std::string& management_domain) {
  controller_ = controller;
  enrollment_mode_ = enrollment_mode;
  management_domain_ = management_domain;
}

void EnrollmentScreenHandler::PrepareToShow() {
}

void EnrollmentScreenHandler::Show() {
  if (!page_is_ready())
    show_on_init_ = true;
  else
    DoShow();
}

void EnrollmentScreenHandler::Hide() {
}

void EnrollmentScreenHandler::FetchOAuthToken() {
  Profile* profile = Profile::FromWebUI(web_ui());
  oauth_fetcher_.reset(
      new policy::PolicyOAuth2TokenFetcher(
          profile->GetRequestContext(),
          g_browser_process->system_request_context(),
          base::Bind(&EnrollmentScreenHandler::OnTokenFetched,
                     base::Unretained(this))));
  oauth_fetcher_->Start();
}

void EnrollmentScreenHandler::ResetAuth(const base::Closure& callback) {
  auth_reset_callbacks_.push_back(callback);
  if (browsing_data_remover_)
    return;

  if (oauth_fetcher_) {
    if (!oauth_fetcher_->oauth2_access_token().empty())
      (new TokenRevoker())->Start(oauth_fetcher_->oauth2_access_token());

    if (!oauth_fetcher_->oauth2_refresh_token().empty())
      (new TokenRevoker())->Start(oauth_fetcher_->oauth2_refresh_token());
  }

  Profile* profile = Profile::FromBrowserContext(
      web_ui()->GetWebContents()->GetBrowserContext());
  browsing_data_remover_ =
      BrowsingDataRemover::CreateForUnboundedRange(profile);
  browsing_data_remover_->AddObserver(this);
  browsing_data_remover_->Remove(BrowsingDataRemover::REMOVE_SITE_DATA,
                                 BrowsingDataHelper::UNPROTECTED_WEB);
}

void EnrollmentScreenHandler::ShowSigninScreen() {
  ShowStep(kEnrollmentStepSignin);
}

void EnrollmentScreenHandler::ShowEnrollmentSpinnerScreen() {
  ShowWorking(IDS_ENTERPRISE_ENROLLMENT_WORKING);
}

void EnrollmentScreenHandler::ShowLoginSpinnerScreen() {
  ShowWorking(IDS_ENTERPRISE_ENROLLMENT_RESUMING_LOGIN);
}

void EnrollmentScreenHandler::ShowAuthError(
    const GoogleServiceAuthError& error) {
  switch (error.state()) {
    case GoogleServiceAuthError::NONE:
    case GoogleServiceAuthError::CAPTCHA_REQUIRED:
    case GoogleServiceAuthError::TWO_FACTOR:
    case GoogleServiceAuthError::HOSTED_NOT_ALLOWED:
    case GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS:
    case GoogleServiceAuthError::REQUEST_CANCELED:
    case GoogleServiceAuthError::UNEXPECTED_SERVICE_RESPONSE:
    case GoogleServiceAuthError::SERVICE_ERROR:
      ShowError(IDS_ENTERPRISE_ENROLLMENT_AUTH_FATAL_ERROR, false);
      return;
    case GoogleServiceAuthError::USER_NOT_SIGNED_UP:
    case GoogleServiceAuthError::ACCOUNT_DELETED:
    case GoogleServiceAuthError::ACCOUNT_DISABLED:
      ShowError(IDS_ENTERPRISE_ENROLLMENT_AUTH_ACCOUNT_ERROR, true);
      return;
    case GoogleServiceAuthError::CONNECTION_FAILED:
    case GoogleServiceAuthError::SERVICE_UNAVAILABLE:
      ShowError(IDS_ENTERPRISE_ENROLLMENT_AUTH_NETWORK_ERROR, true);
      return;
    case GoogleServiceAuthError::NUM_STATES:
      break;
  }
  NOTREACHED();
}

void EnrollmentScreenHandler::ShowUIError(UIError error) {
  switch (error) {
    case UI_ERROR_DOMAIN_MISMATCH:
      ShowError(IDS_ENTERPRISE_ENROLLMENT_STATUS_LOCK_WRONG_USER, true);
      return;
    case UI_ERROR_AUTO_ENROLLMENT_BAD_MODE:
      ShowError(IDS_ENTERPRISE_AUTO_ENROLLMENT_BAD_MODE, true);
      return;
    case UI_ERROR_FATAL:
      ShowError(IDS_ENTERPRISE_ENROLLMENT_FATAL_ENROLLMENT_ERROR, true);
      return;
  }
  NOTREACHED();
}

void EnrollmentScreenHandler::ShowEnrollmentStatus(
    policy::EnrollmentStatus status) {
  switch (status.status()) {
    case policy::EnrollmentStatus::STATUS_SUCCESS:
      ShowStep(kEnrollmentStepSuccess);
      return;
    case policy::EnrollmentStatus::STATUS_NO_STATE_KEYS:
      ShowError(IDS_ENTERPRISE_ENROLLMENT_STATUS_NO_STATE_KEYS, false);
      return;
    case policy::EnrollmentStatus::STATUS_REGISTRATION_FAILED:
      // Some special cases for generating a nicer message that's more helpful.
      switch (status.client_status()) {
        case policy::DM_STATUS_SERVICE_MANAGEMENT_NOT_SUPPORTED:
          ShowError(IDS_ENTERPRISE_ENROLLMENT_ACCOUNT_ERROR, true);
          break;
        case policy::DM_STATUS_SERVICE_MISSING_LICENSES:
          ShowError(IDS_ENTERPRISE_ENROLLMENT_MISSING_LICENSES_ERROR, true);
          break;
        case policy::DM_STATUS_SERVICE_DEPROVISIONED:
          ShowError(IDS_ENTERPRISE_ENROLLMENT_DEPROVISIONED_ERROR, true);
          break;
        case policy::DM_STATUS_SERVICE_DOMAIN_MISMATCH:
          ShowError(IDS_ENTERPRISE_ENROLLMENT_DOMAIN_MISMATCH_ERROR, true);
          break;
        default:
          ShowErrorMessage(
              l10n_util::GetStringFUTF8(
                  IDS_ENTERPRISE_ENROLLMENT_STATUS_REGISTRATION_FAILED,
                  policy::FormatDeviceManagementStatus(status.client_status())),
              true);
      }
      return;
    case policy::EnrollmentStatus::STATUS_ROBOT_AUTH_FETCH_FAILED:
      ShowError(IDS_ENTERPRISE_ENROLLMENT_ROBOT_AUTH_FETCH_FAILED, true);
      return;
    case policy::EnrollmentStatus::STATUS_ROBOT_REFRESH_FETCH_FAILED:
      ShowError(IDS_ENTERPRISE_ENROLLMENT_ROBOT_REFRESH_FETCH_FAILED, true);
      return;
    case policy::EnrollmentStatus::STATUS_ROBOT_REFRESH_STORE_FAILED:
      ShowError(IDS_ENTERPRISE_ENROLLMENT_ROBOT_REFRESH_STORE_FAILED, true);
      return;
    case policy::EnrollmentStatus::STATUS_REGISTRATION_BAD_MODE:
      ShowError(IDS_ENTERPRISE_ENROLLMENT_STATUS_REGISTRATION_BAD_MODE, false);
      return;
    case policy::EnrollmentStatus::STATUS_POLICY_FETCH_FAILED:
      ShowErrorMessage(
          l10n_util::GetStringFUTF8(
              IDS_ENTERPRISE_ENROLLMENT_STATUS_POLICY_FETCH_FAILED,
              policy::FormatDeviceManagementStatus(status.client_status())),
          true);
      return;
    case policy::EnrollmentStatus::STATUS_VALIDATION_FAILED:
      ShowErrorMessage(
          l10n_util::GetStringFUTF8(
              IDS_ENTERPRISE_ENROLLMENT_STATUS_VALIDATION_FAILED,
              policy::FormatValidationStatus(status.validation_status())),
          true);
      return;
    case policy::EnrollmentStatus::STATUS_LOCK_ERROR:
      ShowError(IDS_ENTERPRISE_ENROLLMENT_STATUS_LOCK_ERROR, false);
      return;
    case policy::EnrollmentStatus::STATUS_LOCK_TIMEOUT:
      ShowError(IDS_ENTERPRISE_ENROLLMENT_STATUS_LOCK_TIMEOUT, false);
      return;
    case policy::EnrollmentStatus::STATUS_LOCK_WRONG_USER:
      ShowError(IDS_ENTERPRISE_ENROLLMENT_STATUS_LOCK_WRONG_USER, true);
      return;
    case policy::EnrollmentStatus::STATUS_STORE_ERROR:
      ShowErrorMessage(
          l10n_util::GetStringFUTF8(
              IDS_ENTERPRISE_ENROLLMENT_STATUS_STORE_ERROR,
              policy::FormatStoreStatus(status.store_status(),
                                        status.validation_status())),
          true);
      return;
    case policy::EnrollmentStatus::STATUS_STORE_TOKEN_AND_ID_FAILED:
      // This error should not happen for enterprise enrollment.
      ShowError(IDS_ENTERPRISE_ENROLLMENT_STATUS_STORE_TOKEN_AND_ID_FAILED,
                true);
      NOTREACHED();
      return;
  }
  NOTREACHED();
}

// EnrollmentScreenHandler BaseScreenHandler implementation -----

void EnrollmentScreenHandler::Initialize() {
  if (show_on_init_) {
    Show();
    show_on_init_ = false;
  }
}

void EnrollmentScreenHandler::DeclareLocalizedValues(
    LocalizedValuesBuilder* builder) {
  builder->Add("oauthEnrollScreenTitle",
               IDS_ENTERPRISE_ENROLLMENT_SCREEN_TITLE);
  builder->Add("oauthEnrollDescription", IDS_ENTERPRISE_ENROLLMENT_DESCRIPTION);
  builder->Add("oauthEnrollReEnrollmentText",
               IDS_ENTERPRISE_ENROLLMENT_RE_ENROLLMENT_TEXT);
  builder->Add("oauthEnrollRetry", IDS_ENTERPRISE_ENROLLMENT_RETRY);
  builder->Add("oauthEnrollCancel", IDS_ENTERPRISE_ENROLLMENT_CANCEL);
  builder->Add("oauthEnrollDone", IDS_ENTERPRISE_ENROLLMENT_DONE);
  builder->Add("oauthEnrollSuccess", IDS_ENTERPRISE_ENROLLMENT_SUCCESS);
  builder->Add("oauthEnrollExplain", IDS_ENTERPRISE_ENROLLMENT_EXPLAIN);
  builder->Add("oauthEnrollExplainLink",
               IDS_ENTERPRISE_ENROLLMENT_EXPLAIN_LINK);
  builder->Add("oauthEnrollExplainButton",
               IDS_ENTERPRISE_ENROLLMENT_EXPLAIN_BUTTON);
  builder->Add("oauthEnrollCancelAutoEnrollmentReally",
               IDS_ENTERPRISE_ENROLLMENT_CANCEL_AUTO_REALLY);
  builder->Add("oauthEnrollCancelAutoEnrollmentConfirm",
               IDS_ENTERPRISE_ENROLLMENT_CANCEL_AUTO_CONFIRM);
  builder->Add("oauthEnrollCancelAutoEnrollmentGoBack",
               IDS_ENTERPRISE_ENROLLMENT_CANCEL_AUTO_GO_BACK);
}

void EnrollmentScreenHandler::OnBrowsingDataRemoverDone() {
  browsing_data_remover_->RemoveObserver(this);
  browsing_data_remover_ = NULL;

  std::vector<base::Closure> callbacks_to_run;
  callbacks_to_run.swap(auth_reset_callbacks_);
  for (std::vector<base::Closure>::iterator callback(callbacks_to_run.begin());
       callback != callbacks_to_run.end(); ++callback) {
    callback->Run();
  }
}

OobeUI::Screen EnrollmentScreenHandler::GetCurrentScreen() const {
  OobeUI::Screen screen = OobeUI::SCREEN_UNKNOWN;
  OobeUI* oobe_ui = static_cast<OobeUI*>(web_ui()->GetController());
  if (oobe_ui)
    screen = oobe_ui->current_screen();
  return screen;
}

bool EnrollmentScreenHandler::IsOnEnrollmentScreen() const {
  return (GetCurrentScreen() == OobeUI::SCREEN_OOBE_ENROLLMENT);
}

bool EnrollmentScreenHandler::IsEnrollmentScreenHiddenByError() const {
  return (GetCurrentScreen() == OobeUI::SCREEN_ERROR_MESSAGE &&
          error_screen_actor_->parent_screen() ==
              OobeUI::SCREEN_OOBE_ENROLLMENT);
}

// TODO(rsorokin): This function is mostly copied from SigninScreenHandler and
// should be refactored in the future.
void EnrollmentScreenHandler::UpdateState(
    ErrorScreenActor::ErrorReason reason) {
  if (!IsOnEnrollmentScreen() && !IsEnrollmentScreenHiddenByError())
    return;

  NetworkStateInformer::State state = network_state_informer_->state();
  const std::string network_path = network_state_informer_->network_path();
  const bool is_online = (state == NetworkStateInformer::ONLINE);
  const bool is_behind_captive_portal =
      (state == NetworkStateInformer::CAPTIVE_PORTAL);
  const bool is_frame_error =
      (frame_error() != net::OK) ||
      (reason == ErrorScreenActor::ERROR_REASON_FRAME_ERROR);

  LOG(WARNING) << "EnrollmentScreenHandler::UpdateState(): "
               << "state=" << NetworkStateInformer::StatusString(state) << ", "
               << "reason=" << ErrorScreenActor::ErrorReasonString(reason);

  if (is_online || !is_behind_captive_portal)
    error_screen_actor_->HideCaptivePortal();

  if (is_frame_error) {
    LOG(WARNING) << "Retry page load";
    // TODO(rsorokin): Too many consecutive reloads.
    CallJS("doReload");
  }

  if (!is_online || is_frame_error)
    SetupAndShowOfflineMessage(state, reason);
  else
    HideOfflineMessage(state, reason);
}

void EnrollmentScreenHandler::SetupAndShowOfflineMessage(
    NetworkStateInformer::State state,
    ErrorScreenActor::ErrorReason reason) {
  const std::string network_path = network_state_informer_->network_path();
  const bool is_behind_captive_portal = IsBehindCaptivePortal(state, reason);
  const bool is_proxy_error = IsProxyError(state, reason);
  const bool is_frame_error =
      (frame_error() != net::OK) ||
      (reason == ErrorScreenActor::ERROR_REASON_FRAME_ERROR);

  if (is_proxy_error) {
    error_screen_actor_->SetErrorState(ErrorScreen::ERROR_STATE_PROXY,
                                       std::string());
  } else if (is_behind_captive_portal) {
    // Do not bother a user with obsessive captive portal showing. This
    // check makes captive portal being shown only once: either when error
    // screen is shown for the first time or when switching from another
    // error screen (offline, proxy).
    if (IsOnEnrollmentScreen() || (error_screen_actor_->error_state() !=
                                   ErrorScreen::ERROR_STATE_PORTAL)) {
      error_screen_actor_->FixCaptivePortal();
    }
    const std::string network_name = GetNetworkName(network_path);
    error_screen_actor_->SetErrorState(ErrorScreen::ERROR_STATE_PORTAL,
                                       network_name);
  } else if (is_frame_error) {
    error_screen_actor_->SetErrorState(
        ErrorScreen::ERROR_STATE_AUTH_EXT_TIMEOUT, std::string());
  } else {
    error_screen_actor_->SetErrorState(ErrorScreen::ERROR_STATE_OFFLINE,
                                       std::string());
  }

  if (GetCurrentScreen() != OobeUI::SCREEN_ERROR_MESSAGE) {
    base::DictionaryValue params;
    const std::string network_type = network_state_informer_->network_type();
    params.SetString("lastNetworkType", network_type);
    error_screen_actor_->SetUIState(ErrorScreen::UI_STATE_SIGNIN);
    error_screen_actor_->Show(OobeUI::SCREEN_OOBE_ENROLLMENT,
                              &params,
                              base::Bind(&EnrollmentScreenHandler::DoShow,
                                         weak_ptr_factory_.GetWeakPtr()));
  }
}

void EnrollmentScreenHandler::HideOfflineMessage(
    NetworkStateInformer::State state,
    ErrorScreenActor::ErrorReason reason) {
  if (IsEnrollmentScreenHiddenByError())
    error_screen_actor_->Hide();
}

void EnrollmentScreenHandler::OnFrameError(
    const std::string& frame_unique_name) {
  if (frame_unique_name == "oauth-enroll-signin-frame") {
    HandleFrameLoadingCompleted(net::ERR_FAILED);
  }
}
// EnrollmentScreenHandler, private -----------------------------

void EnrollmentScreenHandler::HandleRetrieveAuthenticatedUserEmail(
    double attempt_token) {
  email_retriever_.reset(new AuthenticatedUserEmailRetriever(
      base::Bind(&EnrollmentScreenHandler::CallJS<double, std::string>,
                 base::Unretained(this),
                 "setAuthenticatedUserEmail",
                 attempt_token),
      Profile::FromWebUI(web_ui())->GetRequestContext()));
}

void EnrollmentScreenHandler::HandleClose(const std::string& reason) {
  DCHECK(controller_);

  if (reason == "cancel" || reason == "autocancel")
    controller_->OnCancel();
  else if (reason == "done")
    controller_->OnConfirmationClosed();
  else
    NOTREACHED();
}

void EnrollmentScreenHandler::HandleCompleteLogin(const std::string& user) {
  DCHECK(controller_);
  controller_->OnLoginDone(gaia::SanitizeEmail(user));
}

void EnrollmentScreenHandler::HandleRetry() {
  DCHECK(controller_);
  controller_->OnRetry();
}

void EnrollmentScreenHandler::HandleFrameLoadingCompleted(int status) {
  const net::Error frame_error = static_cast<net::Error>(status);
  frame_error_ = frame_error;

  if (network_state_informer_->state() != NetworkStateInformer::ONLINE)
    return;
  if (frame_error_)
    UpdateState(ErrorScreenActor::ERROR_REASON_FRAME_ERROR);
  else
    UpdateState(ErrorScreenActor::ERROR_REASON_UPDATE);
}

void EnrollmentScreenHandler::ShowStep(const char* step) {
  CallJS("showStep", std::string(step));
}

void EnrollmentScreenHandler::ShowError(int message_id, bool retry) {
  ShowErrorMessage(l10n_util::GetStringUTF8(message_id), retry);
}

void EnrollmentScreenHandler::ShowErrorMessage(const std::string& message,
                                               bool retry) {
  CallJS("showError", message, retry);
}

void EnrollmentScreenHandler::ShowWorking(int message_id) {
  CallJS("showWorking", l10n_util::GetStringUTF16(message_id));
}

void EnrollmentScreenHandler::OnTokenFetched(
    const std::string& token,
    const GoogleServiceAuthError& error) {
  if (!controller_)
    return;

  if (error.state() != GoogleServiceAuthError::NONE)
    controller_->OnAuthError(error);
  else
    controller_->OnOAuthTokenAvailable(token);
}

void EnrollmentScreenHandler::DoShow() {
  base::DictionaryValue screen_data;
  screen_data.SetString(
      "signin_url",
      base::StringPrintf("%s/main.html", extensions::kGaiaAuthExtensionOrigin));
  screen_data.SetString("gaiaUrl", GaiaUrls::GetInstance()->gaia_url().spec());
  screen_data.SetString("enrollment_mode",
                        EnrollmentModeToString(enrollment_mode_));
  screen_data.SetString("management_domain", management_domain_);

  ShowScreen(OobeUI::kScreenOobeEnrollment, &screen_data);
}

}  // namespace chromeos
