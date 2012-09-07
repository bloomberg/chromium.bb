// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/enterprise_oauth_enrollment_screen_handler.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/metrics/histogram.h"
#include "chrome/browser/browsing_data/browsing_data_helper.h"
#include "chrome/browser/browsing_data/browsing_data_remover.h"
#include "chrome/browser/chromeos/login/login_utils.h"
#include "chrome/browser/net/gaia/gaia_oauth_fetcher.h"
#include "chrome/browser/policy/auto_enrollment_client.h"
#include "chrome/browser/policy/enterprise_metrics.h"
#include "chrome/browser/profiles/profile.h"
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

// A helper class that takes care of asynchronously revoking a given token. It
// will delete itself once done.
class TokenRevoker : public GaiaOAuthConsumer {
 public:
  TokenRevoker(const std::string& token,
               const std::string& secret,
               Profile* profile)
      : oauth_fetcher_(this, profile->GetRequestContext(),
                       kServiceScopeChromeOSDeviceManagement) {
    if (secret.empty())
      oauth_fetcher_.StartOAuthRevokeWrapToken(token);
    else
      oauth_fetcher_.StartOAuthRevokeAccessToken(token, secret);
  }

  virtual ~TokenRevoker() {}

  virtual void OnOAuthRevokeTokenSuccess() OVERRIDE {
    LOG(INFO) << "Successfully revoked OAuth token.";
    MessageLoop::current()->DeleteSoon(FROM_HERE, this);
  }

  virtual void OnOAuthRevokeTokenFailure(
      const GoogleServiceAuthError& error) OVERRIDE {
    LOG(ERROR) << "Failed to revoke OAuth token!";
    MessageLoop::current()->DeleteSoon(FROM_HERE, this);
  }

 private:
  GaiaOAuthFetcher oauth_fetcher_;
  std::string token_;

  DISALLOW_COPY_AND_ASSIGN(TokenRevoker);
};

void UMA(int sample) {
  UMA_HISTOGRAM_ENUMERATION(policy::kMetricEnrollment,
                            sample,
                            policy::kMetricEnrollmentSize);
}

}  // namespace

namespace chromeos {

// EnterpriseOAuthEnrollmentScreenHandler, public ------------------------------

EnterpriseOAuthEnrollmentScreenHandler::EnterpriseOAuthEnrollmentScreenHandler()
    : controller_(NULL),
      show_on_init_(false),
      is_auto_enrollment_(false),
      enrollment_failed_once_(false),
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

void EnterpriseOAuthEnrollmentScreenHandler::SetController(
    Controller* controller) {
  controller_ = controller;
}

void EnterpriseOAuthEnrollmentScreenHandler::PrepareToShow() {
}

void EnterpriseOAuthEnrollmentScreenHandler::Show() {
  if (!page_is_ready()) {
    show_on_init_ = true;
    return;
  }

  std::string user;
  is_auto_enrollment_ = controller_ && controller_->IsAutoEnrollment(&user);
  if (is_auto_enrollment_)
    user_ = gaia::SanitizeEmail(user);
  enrollment_failed_once_ = false;

  DoShow();
}

void EnterpriseOAuthEnrollmentScreenHandler::Hide() {
}

void EnterpriseOAuthEnrollmentScreenHandler::ShowConfirmationScreen() {
  UMA(is_auto_enrollment_ ? policy::kMetricEnrollmentAutoOK :
                            policy::kMetricEnrollmentOK);
  ShowStep(kEnrollmentStepSuccess);
  if (!is_auto_enrollment_ || enrollment_failed_once_)
    ResetAuth();
  NotifyObservers(true);
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
      LOG(ERROR) << "Auth error " << error.state();
      ShowEnrollmentError(FATAL_AUTH_ERROR);
      return;
    case GoogleServiceAuthError::USER_NOT_SIGNED_UP:
    case GoogleServiceAuthError::ACCOUNT_DELETED:
    case GoogleServiceAuthError::ACCOUNT_DISABLED:
      LOG(ERROR) << "Account error " << error.state();
      ShowEnrollmentError(ACCOUNT_ERROR);
      return;
    case GoogleServiceAuthError::CONNECTION_FAILED:
    case GoogleServiceAuthError::SERVICE_UNAVAILABLE:
      LOG(WARNING) << "Network error " << error.state();
      ShowEnrollmentError(NETWORK_ERROR);
      return;
  }
  UMAFailure(policy::kMetricEnrollmentOtherFailed);
  NOTREACHED();
}

void EnterpriseOAuthEnrollmentScreenHandler::ShowEnrollmentError(
    EnrollmentError error_code) {
  switch (error_code) {
    case ACCOUNT_ERROR:
      UMAFailure(policy::kMetricEnrollmentNotSupported);
      ShowError(IDS_ENTERPRISE_ENROLLMENT_ACCOUNT_ERROR, true);
      break;
    case SERIAL_NUMBER_ERROR:
      UMAFailure(policy::kMetricEnrollmentInvalidSerialNumber);
      ShowError(IDS_ENTERPRISE_ENROLLMENT_SERIAL_NUMBER_ERROR, true);
      break;
    case ENROLLMENT_MODE_ERROR:
      UMAFailure(policy::kMetricEnrollmentInvalidEnrollmentMode);
      ShowError(IDS_ENTERPRISE_ENROLLMENT_MODE_ERROR, false);
      break;
    case FATAL_AUTH_ERROR:
      UMAFailure(policy::kMetricEnrollmentLoginFailed);
      ShowError(IDS_ENTERPRISE_ENROLLMENT_FATAL_AUTH_ERROR, false);
      break;
    case AUTO_ENROLLMENT_ERROR: {
      UMAFailure(policy::kMetricEnrollmentAutoEnrollmentNotSupported);
      // The reason for showing this error is that we have been trying to
      // auto-enroll and this failed, so we have to verify whether
      // auto-enrollment is on, and if so switch it off, update the UI
      // accordingly and show the error message.
      std::string user;
      is_auto_enrollment_ = controller_ && controller_->IsAutoEnrollment(&user);
      base::FundamentalValue value(is_auto_enrollment_);
      web_ui()->CallJavascriptFunction(
          "oobe.OAuthEnrollmentScreen.setIsAutoEnrollment", value);

      ShowError(IDS_ENTERPRISE_AUTO_ENROLLMENT_ERROR, false);
      break;
    }
    case NETWORK_ERROR:
      UMAFailure(policy::kMetricEnrollmentNetworkFailed);
      ShowError(IDS_ENTERPRISE_ENROLLMENT_NETWORK_ENROLLMENT_ERROR, true);
      break;
    case LOCKBOX_TIMEOUT_ERROR:
      UMAFailure(policy::kMetricLockboxTimeoutError);
      ShowError(IDS_ENTERPRISE_LOCKBOX_TIMEOUT_ERROR, true);
      break;
    case DOMAIN_MISMATCH_ERROR:
      UMAFailure(policy::kMetricEnrollmentWrongUserError);
      ShowError(IDS_ENTERPRISE_ENROLLMENT_DOMAIN_MISMATCH_ERROR, true);
      break;
    case MISSING_LICENSES_ERROR:
      UMAFailure(policy::kMetricMissingLicensesError);
      ShowError(IDS_ENTERPRISE_ENROLLMENT_MISSING_LICENSES_ERROR, true);
      break;
    case FATAL_ERROR:
    default:
      UMAFailure(policy::kMetricEnrollmentOtherFailed);
      ShowError(IDS_ENTERPRISE_ENROLLMENT_FATAL_ENROLLMENT_ERROR, false);
  }
  NotifyObservers(false);
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
  ShowEnrollmentError(FATAL_AUTH_ERROR);
}

void EnterpriseOAuthEnrollmentScreenHandler::OnOAuthGetAccessTokenSuccess(
    const std::string& token,
    const std::string& secret) {
  access_token_ = token;
  access_token_secret_ = secret;
}

void EnterpriseOAuthEnrollmentScreenHandler::OnOAuthGetAccessTokenFailure(
    const GoogleServiceAuthError& error) {
  ShowAuthError(error);
}

void EnterpriseOAuthEnrollmentScreenHandler::OnOAuthWrapBridgeSuccess(
    const std::string& service_scope,
    const std::string& token,
    const std::string& expires_in) {
  DCHECK_EQ(service_scope, GaiaConstants::kDeviceManagementServiceOAuth);

  wrap_token_ = token;

  if (!controller_ || user_.empty()) {
    NOTREACHED();
    return;
  }

  controller_->OnOAuthTokenAvailable(user_, token);
}

void EnterpriseOAuthEnrollmentScreenHandler::OnOAuthWrapBridgeFailure(
    const std::string& service_scope,
    const GoogleServiceAuthError& error) {
  ShowAuthError(error);
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

  if (!action_on_browsing_data_removed_.is_null()) {
    action_on_browsing_data_removed_.Run();
    action_on_browsing_data_removed_.Reset();
  }
}

// EnterpriseOAuthEnrollmentScreenHandler, private -----------------------------

void EnterpriseOAuthEnrollmentScreenHandler::HandleClose(
    const base::ListValue* value) {
  bool back_to_signin = true;

  std::string reason;
  CHECK_EQ(1U, value->GetSize());
  CHECK(value->GetString(0, &reason));

  if (reason == "cancel") {
    DCHECK(!is_auto_enrollment_);
    UMA(policy::kMetricEnrollmentCancelled);
  } else if (reason == "autocancel") {
    // Store the user's decision so that the sign-in screen doesn't go
    // automatically to the enrollment screen again.
    policy::AutoEnrollmentClient::CancelAutoEnrollment();
    UMA(policy::kMetricEnrollmentAutoCancelled);
  } else if (reason == "done") {
    // If the user account used for enrollment is not whitelisted, send the user
    // back to the login screen. In that case, clear the profile data too.
    bool is_whitelisted = !user_.empty() && LoginUtils::IsWhitelisted(user_);

    // If enrollment failed at least once, the profile was cleared and the user
    // had to retry with another account, or even cancelled the whole thing.
    // In that case, go back to the sign-in screen; otherwise, if this was an
    // auto-enrollment, resume the pending signin.
    back_to_signin = !is_auto_enrollment_ ||
                     enrollment_failed_once_ ||
                     !is_whitelisted;
  } else {
    NOTREACHED();
  }

  RevokeTokens();

  if (back_to_signin) {
    // Clean the profile before going back to signin.
    action_on_browsing_data_removed_ =
        base::Bind(&EnterpriseOAuthEnrollmentScreenHandler::DoClose,
                   base::Unretained(this),
                   true);
    ResetAuth();
  } else {
    // Not going back to sign-in, letting the initial sign-in resume instead.
    // In that case, keep the profile data.
    DoClose(false);
  }
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

  user_ = gaia::SanitizeEmail(user);
  EnrollAfterLogin();
}

void EnterpriseOAuthEnrollmentScreenHandler::HandleRetry(
    const base::ListValue* value) {
  // Trigger browsing data removal to make sure we start from a clean slate.
  action_on_browsing_data_removed_ =
      base::Bind(&EnterpriseOAuthEnrollmentScreenHandler::DoShow,
                 base::Unretained(this));
  ResetAuth();
}

void EnterpriseOAuthEnrollmentScreenHandler::EnrollAfterLogin() {
  DCHECK(!user_.empty());
  if (is_auto_enrollment_) {
    UMA(enrollment_failed_once_ ? policy::kMetricEnrollmentAutoRetried :
                                  policy::kMetricEnrollmentAutoStarted);
  } else {
    UMA(policy::kMetricEnrollmentStarted);
  }
  Profile* profile = Profile::FromWebUI(web_ui());
  oauth_fetcher_.reset(
      new GaiaOAuthFetcher(this,
                           profile->GetRequestContext(),
                           GaiaConstants::kDeviceManagementServiceOAuth));
  oauth_fetcher_->SetAutoFetchLimit(
      GaiaOAuthFetcher::OAUTH2_SERVICE_ACCESS_TOKEN);
  oauth_fetcher_->StartGetOAuthTokenRequest();

  ShowWorking(IDS_ENTERPRISE_ENROLLMENT_WORKING);
}

void EnterpriseOAuthEnrollmentScreenHandler::ShowStep(const char* step) {
  RevokeTokens();

  base::StringValue step_value(step);
  web_ui()->CallJavascriptFunction("oobe.OAuthEnrollmentScreen.showStep",
                                   step_value);
}

void EnterpriseOAuthEnrollmentScreenHandler::ShowError(int message_id,
                                                       bool retry) {
  RevokeTokens();
  enrollment_failed_once_ = true;

  const std::string message(l10n_util::GetStringUTF8(message_id));
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

void EnterpriseOAuthEnrollmentScreenHandler::ResetAuth() {
  oauth_fetcher_.reset();

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

void EnterpriseOAuthEnrollmentScreenHandler::RevokeTokens() {
  Profile* profile = Profile::FromBrowserContext(
      web_ui()->GetWebContents()->GetBrowserContext());

  if (!access_token_.empty()) {
    new TokenRevoker(access_token_, access_token_secret_, profile);
    access_token_.clear();
  }

  if (!wrap_token_.empty()) {
    new TokenRevoker(wrap_token_, "", profile);
    wrap_token_.clear();
  }
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

  ShowScreen("oauth-enrollment", &screen_data);

  if (is_auto_enrollment_ && !enrollment_failed_once_)
    EnrollAfterLogin();
}

void EnterpriseOAuthEnrollmentScreenHandler::DoClose(bool back_to_signin) {
  if (!controller_) {
    NOTREACHED();
    return;
  }

  if (!back_to_signin) {
    // Show a progress spinner while profile creation is resuming.
    ShowWorking(IDS_ENTERPRISE_ENROLLMENT_RESUMING_LOGIN);
  }
  controller_->OnConfirmationClosed(back_to_signin);
}

void EnterpriseOAuthEnrollmentScreenHandler::UMAFailure(int sample) {
  if (is_auto_enrollment_)
    sample = policy::kMetricEnrollmentAutoFailed;
  UMA(sample);
}

}  // namespace chromeos
