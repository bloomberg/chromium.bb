// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/enterprise_oauth_enrollment_screen_handler.h"

#include <algorithm>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "chrome/browser/browsing_data/browsing_data_helper.h"
#include "chrome/browser/browsing_data/browsing_data_remover.h"
#include "chrome/browser/net/gaia/gaia_oauth_fetcher.h"
#include "chrome/browser/policy/message_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "content/public/browser/web_contents.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_switches.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

// Start page of GAIA authentication extension.
const char kGaiaExtStartPage[] =
    "chrome-extension://mfffpogegjflfpflabcdkioaeobkgjik/main.html";

// OAuth V2 service scope for device management.
const char kServiceScopeChromeOSDeviceManagement[] =
    "https://www.googleapis.com/auth/chromeosdevicemanagement";

// Enrollment step names.
const char kEnrollmentStepSignin[] = "signin";
const char kEnrollmentStepWorking[] = "working";
const char kEnrollmentStepError[] = "error";
const char kEnrollmentStepSuccess[] = "success";

}  // namespace

namespace chromeos {

// EnterpriseOAuthEnrollmentScreenHandler::TokenRevoker ------------------------

// A helper class that takes care of asynchronously revoking a given token.
class EnterpriseOAuthEnrollmentScreenHandler::TokenRevoker
    : public GaiaOAuthConsumer {
 public:
  TokenRevoker(const std::string& token,
               const std::string& secret,
               Profile* profile,
               EnterpriseOAuthEnrollmentScreenHandler* owner)
      : oauth_fetcher_(this, profile->GetRequestContext(),
                       kServiceScopeChromeOSDeviceManagement),
        owner_(owner) {
    if (secret.empty())
      oauth_fetcher_.StartOAuthRevokeWrapToken(token);
    else
      oauth_fetcher_.StartOAuthRevokeAccessToken(token, secret);
  }

  virtual ~TokenRevoker() {}

  virtual void OnOAuthRevokeTokenSuccess() OVERRIDE {
    LOG(INFO) << "Successfully revoked OAuth token.";
    owner_->OnTokenRevokerDone(this);
  }

  virtual void OnOAuthRevokeTokenFailure(
      const GoogleServiceAuthError& error) OVERRIDE {
    LOG(ERROR) << "Failed to revoke OAuth token!";
    owner_->OnTokenRevokerDone(this);
  }

 private:
  GaiaOAuthFetcher oauth_fetcher_;
  EnterpriseOAuthEnrollmentScreenHandler* owner_;

  DISALLOW_COPY_AND_ASSIGN(TokenRevoker);
};

// EnterpriseOAuthEnrollmentScreenHandler, public ------------------------------

EnterpriseOAuthEnrollmentScreenHandler::EnterpriseOAuthEnrollmentScreenHandler()
    : controller_(NULL),
      show_on_init_(false),
      is_auto_enrollment_(false),
      browsing_data_remover_(NULL) {
}

EnterpriseOAuthEnrollmentScreenHandler::
    ~EnterpriseOAuthEnrollmentScreenHandler() {
  if (browsing_data_remover_)
    browsing_data_remover_->RemoveObserver(this);
}

// EnterpriseOAuthEnrollmentScreenHandler, WebUIMessageHandler implementation --

void EnterpriseOAuthEnrollmentScreenHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "oauthEnrollClose",
      base::Bind(&EnterpriseOAuthEnrollmentScreenHandler::HandleClose,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "oauthEnrollCompleteLogin",
      base::Bind(&EnterpriseOAuthEnrollmentScreenHandler::HandleCompleteLogin,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "oauthEnrollRetry",
      base::Bind(&EnterpriseOAuthEnrollmentScreenHandler::HandleRetry,
                 base::Unretained(this)));
}

// EnterpriseOAuthEnrollmentScreenHandler
//      EnterpriseEnrollmentScreenActor implementation -------------------------

void EnterpriseOAuthEnrollmentScreenHandler::SetParameters(
    Controller* controller,
    bool is_auto_enrollment,
    const std::string& user) {
  controller_ = controller;
  is_auto_enrollment_ = is_auto_enrollment;
  if (is_auto_enrollment_)
    user_ = user;
}

void EnterpriseOAuthEnrollmentScreenHandler::PrepareToShow() {
}

void EnterpriseOAuthEnrollmentScreenHandler::Show() {
  if (!page_is_ready())
    show_on_init_ = true;
  else
    DoShow();
}

void EnterpriseOAuthEnrollmentScreenHandler::Hide() {
}

void EnterpriseOAuthEnrollmentScreenHandler::FetchOAuthToken() {
  Profile* profile = Profile::FromWebUI(web_ui());
  oauth_fetcher_.reset(
      new GaiaOAuthFetcher(this,
                           profile->GetRequestContext(),
                           GaiaConstants::kDeviceManagementServiceOAuth));
  oauth_fetcher_->SetAutoFetchLimit(
      GaiaOAuthFetcher::OAUTH2_SERVICE_ACCESS_TOKEN);
  oauth_fetcher_->StartGetOAuthTokenRequest();
}

void EnterpriseOAuthEnrollmentScreenHandler::ResetAuth(
    const base::Closure& callback) {
  oauth_fetcher_.reset();

  auth_reset_callbacks_.push_back(callback);
  if (browsing_data_remover_)
    return;

  Profile* profile = Profile::FromBrowserContext(
      web_ui()->GetWebContents()->GetBrowserContext());
  browsing_data_remover_ =
      BrowsingDataRemover::CreateForUnboundedRange(profile);
  browsing_data_remover_->AddObserver(this);
  browsing_data_remover_->Remove(BrowsingDataRemover::REMOVE_SITE_DATA,
                                 BrowsingDataHelper::UNPROTECTED_WEB);
}

void EnterpriseOAuthEnrollmentScreenHandler::ShowSigninScreen() {
  ShowStep(kEnrollmentStepSignin);
}

void EnterpriseOAuthEnrollmentScreenHandler::ShowEnrollmentSpinnerScreen() {
  ShowWorking(IDS_ENTERPRISE_ENROLLMENT_WORKING);
}

void EnterpriseOAuthEnrollmentScreenHandler::ShowLoginSpinnerScreen() {
  ShowWorking(IDS_ENTERPRISE_ENROLLMENT_RESUMING_LOGIN);
}

void EnterpriseOAuthEnrollmentScreenHandler::ShowAuthError(
    const GoogleServiceAuthError& error) {
  switch (error.state()) {
    case GoogleServiceAuthError::NONE:
    case GoogleServiceAuthError::CAPTCHA_REQUIRED:
    case GoogleServiceAuthError::TWO_FACTOR:
    case GoogleServiceAuthError::HOSTED_NOT_ALLOWED:
    case GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS:
    case GoogleServiceAuthError::REQUEST_CANCELED:
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

void EnterpriseOAuthEnrollmentScreenHandler::ShowUIError(UIError error) {
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

void EnterpriseOAuthEnrollmentScreenHandler::ShowEnrollmentStatus(
    policy::EnrollmentStatus status) {
  switch (status.status()) {
    case policy::EnrollmentStatus::STATUS_SUCCESS:
      ShowStep(kEnrollmentStepSuccess);
      return;
    case policy::EnrollmentStatus::STATUS_REGISTRATION_FAILED:
      // Some special cases for generating a nicer message that's more helpful.
      switch (status.client_status()) {
        case policy::DM_STATUS_SERVICE_MISSING_LICENSES:
          ShowError(IDS_ENTERPRISE_ENROLLMENT_MISSING_LICENSES_ERROR, true);
          break;
        case policy::DM_STATUS_SERVICE_MANAGEMENT_NOT_SUPPORTED:
          ShowError(IDS_ENTERPRISE_ENROLLMENT_ACCOUNT_ERROR, true);
          break;
        default:
          ShowErrorMessage(
              l10n_util::GetStringFUTF8(
                  IDS_ENTERPRISE_ENROLLMENT_STATUS_REGISTRATION_FAILED,
                  policy::FormatDeviceManagementStatus(status.client_status())),
              true);
      }
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
  }
  NOTREACHED();
}

void EnterpriseOAuthEnrollmentScreenHandler::SubmitTestCredentials(
    const std::string& email,
    const std::string& password) {
  test_email_ = email;
  test_password_ = password;
  DoShow();
}

// EnterpriseOAuthEnrollmentScreenHandler BaseScreenHandler implementation -----

void EnterpriseOAuthEnrollmentScreenHandler::Initialize() {
  if (show_on_init_) {
    Show();
    show_on_init_ = false;
  }
}

void EnterpriseOAuthEnrollmentScreenHandler::GetLocalizedStrings(
    base::DictionaryValue *localized_strings) {
  localized_strings->SetString(
      "oauthEnrollScreenTitle",
      l10n_util::GetStringUTF16(IDS_ENTERPRISE_ENROLLMENT_SCREEN_TITLE));
  localized_strings->SetString(
      "oauthEnrollRetry",
      l10n_util::GetStringUTF16(IDS_ENTERPRISE_ENROLLMENT_RETRY));
  localized_strings->SetString(
      "oauthEnrollCancel",
      l10n_util::GetStringUTF16(IDS_ENTERPRISE_ENROLLMENT_CANCEL));
  localized_strings->SetString(
      "oauthEnrollDone",
      l10n_util::GetStringUTF16(IDS_ENTERPRISE_ENROLLMENT_DONE));
  localized_strings->SetString(
      "oauthEnrollSuccess",
      l10n_util::GetStringUTF16(IDS_ENTERPRISE_ENROLLMENT_SUCCESS));
  localized_strings->SetString(
      "oauthEnrollExplain",
      l10n_util::GetStringUTF16(IDS_ENTERPRISE_ENROLLMENT_EXPLAIN));
  localized_strings->SetString(
      "oauthEnrollExplainLink",
      l10n_util::GetStringUTF16(IDS_ENTERPRISE_ENROLLMENT_EXPLAIN_LINK));
  localized_strings->SetString(
      "oauthEnrollExplainButton",
      l10n_util::GetStringUTF16(IDS_ENTERPRISE_ENROLLMENT_EXPLAIN_BUTTON));
  localized_strings->SetString(
      "oauthEnrollCancelAutoEnrollmentReally",
      l10n_util::GetStringUTF16(IDS_ENTERPRISE_ENROLLMENT_CANCEL_AUTO_REALLY));
  localized_strings->SetString(
      "oauthEnrollCancelAutoEnrollmentConfirm",
      l10n_util::GetStringUTF16(IDS_ENTERPRISE_ENROLLMENT_CANCEL_AUTO_CONFIRM));
  localized_strings->SetString(
      "oauthEnrollCancelAutoEnrollmentGoBack",
      l10n_util::GetStringUTF16(IDS_ENTERPRISE_ENROLLMENT_CANCEL_AUTO_GO_BACK));
}

void EnterpriseOAuthEnrollmentScreenHandler::OnGetOAuthTokenFailure(
    const GoogleServiceAuthError& error) {
  if (controller_)
    controller_->OnAuthError(error);
}

void EnterpriseOAuthEnrollmentScreenHandler::OnOAuthGetAccessTokenSuccess(
    const std::string& token,
    const std::string& secret) {
  access_token_ = token;
  access_token_secret_ = secret;
}

void EnterpriseOAuthEnrollmentScreenHandler::OnOAuthGetAccessTokenFailure(
    const GoogleServiceAuthError& error) {
  if (controller_)
    controller_->OnAuthError(error);
}

void EnterpriseOAuthEnrollmentScreenHandler::OnOAuthWrapBridgeSuccess(
    const std::string& service_scope,
    const std::string& token,
    const std::string& expires_in) {
  DCHECK_EQ(service_scope, GaiaConstants::kDeviceManagementServiceOAuth);

  wrap_token_ = token;

  if (!controller_) {
    NOTREACHED();
    return;
  }

  controller_->OnOAuthTokenAvailable(token);
}

void EnterpriseOAuthEnrollmentScreenHandler::OnOAuthWrapBridgeFailure(
    const std::string& service_scope,
    const GoogleServiceAuthError& error) {
  if (controller_)
    controller_->OnAuthError(error);
}

void EnterpriseOAuthEnrollmentScreenHandler::OnUserInfoSuccess(
    const std::string& email) {
  NOTREACHED();
}

void EnterpriseOAuthEnrollmentScreenHandler::OnUserInfoFailure(
    const GoogleServiceAuthError& error) {
  NOTREACHED();
}

void EnterpriseOAuthEnrollmentScreenHandler::OnBrowsingDataRemoverDone() {
  browsing_data_remover_->RemoveObserver(this);
  browsing_data_remover_ = NULL;

  std::vector<base::Closure> callbacks_to_run;
  callbacks_to_run.swap(auth_reset_callbacks_);
  for (std::vector<base::Closure>::iterator callback(callbacks_to_run.begin());
       callback != callbacks_to_run.end(); ++callback) {
    callback->Run();
  }
}

// EnterpriseOAuthEnrollmentScreenHandler, private -----------------------------

void EnterpriseOAuthEnrollmentScreenHandler::HandleClose(
    const base::ListValue* value) {
  std::string reason;
  CHECK_EQ(1U, value->GetSize());
  CHECK(value->GetString(0, &reason));

  if (!controller_) {
    NOTREACHED();
    return;
  }

  if (reason == "cancel" || reason == "autocancel")
    controller_->OnCancel();
  else if (reason == "done")
    controller_->OnConfirmationClosed();
  else
    NOTREACHED();

  RevokeTokens();
}

void EnterpriseOAuthEnrollmentScreenHandler::HandleCompleteLogin(
    const base::ListValue* value) {
  if (!controller_) {
    NOTREACHED();
    return;
  }

  std::string user;
  if (!value->GetString(0, &user)) {
    NOTREACHED() << "Invalid user parameter from UI.";
    return;
  }

  controller_->OnLoginDone(gaia::SanitizeEmail(user));
}

void EnterpriseOAuthEnrollmentScreenHandler::HandleRetry(
    const base::ListValue* value) {
  if (!controller_) {
    NOTREACHED();
    return;
  }

  controller_->OnRetry();
}

void EnterpriseOAuthEnrollmentScreenHandler::ShowStep(const char* step) {
  base::StringValue step_value(step);
  web_ui()->CallJavascriptFunction("oobe.OAuthEnrollmentScreen.showStep",
                                   step_value);
}

void EnterpriseOAuthEnrollmentScreenHandler::ShowError(int message_id,
                                                       bool retry) {
  ShowErrorMessage(l10n_util::GetStringUTF8(message_id), retry);
}

void EnterpriseOAuthEnrollmentScreenHandler::ShowErrorMessage(
    const std::string& message,
    bool retry) {
  RevokeTokens();

  base::StringValue message_value(message);
  base::FundamentalValue retry_value(retry);
  web_ui()->CallJavascriptFunction("oobe.OAuthEnrollmentScreen.showError",
                                   message_value,
                                   retry_value);
}

void EnterpriseOAuthEnrollmentScreenHandler::ShowWorking(int message_id) {
  const std::string message(l10n_util::GetStringUTF8(message_id));
  base::StringValue message_value(message);
  web_ui()->CallJavascriptFunction("oobe.OAuthEnrollmentScreen.showWorking",
                                   message_value);
}

void EnterpriseOAuthEnrollmentScreenHandler::RevokeTokens() {
  Profile* profile = Profile::FromBrowserContext(
      web_ui()->GetWebContents()->GetBrowserContext());

  if (!access_token_.empty()) {
    token_revokers_.push_back(
        new TokenRevoker(access_token_, access_token_secret_, profile, this));
    access_token_.clear();
  }

  if (!wrap_token_.empty()) {
    token_revokers_.push_back(new TokenRevoker(wrap_token_, "", profile, this));
    wrap_token_.clear();
  }
}

void EnterpriseOAuthEnrollmentScreenHandler::OnTokenRevokerDone(
    TokenRevoker* revoker) {
  ScopedVector<TokenRevoker>::iterator it =
      std::find(token_revokers_.begin(), token_revokers_.end(), revoker);
  if (it != token_revokers_.end())
    token_revokers_.erase(it);
  else
    NOTREACHED();
}

void EnterpriseOAuthEnrollmentScreenHandler::DoShow() {
  DictionaryValue screen_data;
  screen_data.SetString("signin_url", kGaiaExtStartPage);
  screen_data.SetString("gaiaOrigin",
                        GaiaUrls::GetInstance()->gaia_origin_url());
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kGaiaUrlPath)) {
    screen_data.SetString("gaiaUrlPath",
        CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kGaiaUrlPath));
  }
  screen_data.SetBoolean("is_auto_enrollment", is_auto_enrollment_);
  if (!test_email_.empty()) {
    screen_data.SetString("test_email", test_email_);
    screen_data.SetString("test_password", test_password_);
  }

  ShowScreen(OobeUI::kScreenOobeEnrollment, &screen_data);
}

}  // namespace chromeos
