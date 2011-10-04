// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/enterprise_oauth_enrollment_screen_handler.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/message_loop.h"
#include "base/values.h"
#include "chrome/browser/net/gaia/gaia_oauth_fetcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/net/gaia/gaia_constants.h"
#include "chrome/common/net/gaia/gaia_urls.h"
#include "chrome/common/net/gaia/google_service_auth_error.h"
#include "content/browser/tab_contents/tab_contents.h"
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
      : oauth_fetcher_(this, profile->GetRequestContext(), profile,
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

}  // namespace

namespace chromeos {

// EnterpriseOAuthEnrollmentScreenHandler, public ------------------------------

EnterpriseOAuthEnrollmentScreenHandler::EnterpriseOAuthEnrollmentScreenHandler()
    : controller_(NULL),
      editable_user_(true),
      show_on_init_(false),
      browsing_data_remover_(NULL) {
}

EnterpriseOAuthEnrollmentScreenHandler::
    ~EnterpriseOAuthEnrollmentScreenHandler() {}

// EnterpriseOAuthEnrollmentScreenHandler, WebUIMessageHandler implementation --

void EnterpriseOAuthEnrollmentScreenHandler::RegisterMessages() {
  web_ui_->RegisterMessageCallback(
      "oauthEnrollClose",
      base::Bind(&EnterpriseOAuthEnrollmentScreenHandler::HandleClose,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback(
      "oauthEnrollCompleteLogin",
      base::Bind(&EnterpriseOAuthEnrollmentScreenHandler::HandleCompleteLogin,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback(
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

  // Trigger browsing data removal to make sure we start from a clean slate.
  action_on_browsing_data_removed_ =
      base::Bind(&EnterpriseOAuthEnrollmentScreenHandler::DoShow,
                 base::Unretained(this));
  ResetAuth();
}

void EnterpriseOAuthEnrollmentScreenHandler::Hide() {
}

void EnterpriseOAuthEnrollmentScreenHandler::SetEditableUser(bool editable) {
  editable_user_ = editable;
}

void EnterpriseOAuthEnrollmentScreenHandler::ShowConfirmationScreen() {
  ShowStep(kEnrollmentStepSuccess);
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
      ShowFatalAuthError();
      break;
    case GoogleServiceAuthError::USER_NOT_SIGNED_UP:
    case GoogleServiceAuthError::ACCOUNT_DELETED:
    case GoogleServiceAuthError::ACCOUNT_DISABLED:
      LOG(ERROR) << "Account error " << error.state();
      ShowAccountError();
      break;
    case GoogleServiceAuthError::CONNECTION_FAILED:
    case GoogleServiceAuthError::SERVICE_UNAVAILABLE:
      LOG(WARNING) << "Network error " << error.state();
      ShowNetworkEnrollmentError();
      break;
  }
  NotifyObservers(false);
}

void EnterpriseOAuthEnrollmentScreenHandler::ShowAccountError() {
  ShowError(IDS_ENTERPRISE_ENROLLMENT_ACCOUNT_ERROR, true);
  NotifyObservers(false);
}

void EnterpriseOAuthEnrollmentScreenHandler::ShowSerialNumberError() {
  ShowError(IDS_ENTERPRISE_ENROLLMENT_SERIAL_NUMBER_ERROR, true);
  NotifyObservers(false);
}

void EnterpriseOAuthEnrollmentScreenHandler::ShowFatalAuthError() {
  ShowError(IDS_ENTERPRISE_ENROLLMENT_FATAL_AUTH_ERROR, false);
  NotifyObservers(false);
}

void EnterpriseOAuthEnrollmentScreenHandler::ShowFatalEnrollmentError() {
  ShowError(IDS_ENTERPRISE_ENROLLMENT_FATAL_ENROLLMENT_ERROR, false);
  NotifyObservers(false);
}

void EnterpriseOAuthEnrollmentScreenHandler::ShowNetworkEnrollmentError() {
  ShowError(IDS_ENTERPRISE_ENROLLMENT_NETWORK_ENROLLMENT_ERROR, true);
  NotifyObservers(false);
}

// EnterpriseOAuthEnrollmentScreenHandler BaseScreenHandler implementation -----

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
      "oauthEnrollWorking",
      l10n_util::GetStringUTF16(IDS_ENTERPRISE_ENROLLMENT_WORKING));
}

void EnterpriseOAuthEnrollmentScreenHandler::OnGetOAuthTokenFailure(
    const GoogleServiceAuthError& error) {
  ShowFatalAuthError();
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

void EnterpriseOAuthEnrollmentScreenHandler::Initialize() {
  if (show_on_init_) {
    Show();
    show_on_init_ = false;
  }
}

// EnterpriseOAuthEnrollmentScreenHandler, private -----------------------------

void EnterpriseOAuthEnrollmentScreenHandler::HandleClose(
    const base::ListValue* value) {
  RevokeTokens();
  action_on_browsing_data_removed_ =
      base::Bind(&EnterpriseOAuthEnrollmentScreenHandler::DoClose,
                 base::Unretained(this));
  ResetAuth();
}

void EnterpriseOAuthEnrollmentScreenHandler::HandleCompleteLogin(
    const base::ListValue* value) {
  if (!controller_) {
    NOTREACHED();
    return;
  }

  if (!value->GetString(0, &user_)) {
    NOTREACHED() << "Invalid user parameter from UI.";
    return;
  }

  Profile* profile =
      Profile::FromBrowserContext(web_ui_->tab_contents()->browser_context());
  oauth_fetcher_.reset(
      new GaiaOAuthFetcher(this,
                           profile->GetRequestContext(),
                           profile,
                           GaiaConstants::kDeviceManagementServiceOAuth));
  oauth_fetcher_->SetAutoFetchLimit(
      GaiaOAuthFetcher::OAUTH2_SERVICE_ACCESS_TOKEN);
  oauth_fetcher_->StartGetOAuthTokenRequest();

  ShowStep(kEnrollmentStepWorking);
}

void EnterpriseOAuthEnrollmentScreenHandler::HandleRetry(
    const base::ListValue* value) {
  Show();
}

void EnterpriseOAuthEnrollmentScreenHandler::ShowStep(const char* step) {
  RevokeTokens();

  base::StringValue step_value(step);
  web_ui_->CallJavascriptFunction("oobe.OAuthEnrollmentScreen.showStep",
                                  step_value);
}

void EnterpriseOAuthEnrollmentScreenHandler::ShowError(int message_id,
                                                       bool retry) {
  RevokeTokens();

  const std::string message(l10n_util::GetStringUTF8(message_id));
  base::StringValue message_value(message);
  base::FundamentalValue retry_value(retry);
  web_ui_->CallJavascriptFunction("oobe.OAuthEnrollmentScreen.showError",
                                  message_value,
                                  retry_value);
}

void EnterpriseOAuthEnrollmentScreenHandler::ResetAuth() {
  oauth_fetcher_.reset();

  if (browsing_data_remover_)
    return;

  Profile* profile =
      Profile::FromBrowserContext(web_ui_->tab_contents()->browser_context());
  browsing_data_remover_ =
      new BrowsingDataRemover(profile,
                              BrowsingDataRemover::EVERYTHING,
                              base::Time());
  browsing_data_remover_->AddObserver(this);
  browsing_data_remover_->Remove(BrowsingDataRemover::REMOVE_SITE_DATA);
}

void EnterpriseOAuthEnrollmentScreenHandler::RevokeTokens() {
  Profile* profile =
      Profile::FromBrowserContext(web_ui_->tab_contents()->browser_context());

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
  ShowScreen("oauth-enrollment", &screen_data);
}

void EnterpriseOAuthEnrollmentScreenHandler::DoClose() {
  if (!controller_) {
    NOTREACHED();
    return;
  }

  controller_->OnConfirmationClosed();
}

}  // namespace chromeos
