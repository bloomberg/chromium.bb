// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/enterprise_oauth_enrollment_screen_handler.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/net/gaia/gaia_oauth_fetcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/net/gaia/gaia_constants.h"
#include "chrome/common/net/gaia/gaia_urls.h"
#include "chrome/common/net/gaia/google_service_auth_error.h"
#include "chrome/common/url_constants.h"
#include "content/browser/renderer_host/render_view_host.h"
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
      NewCallback(
          this,
          &EnterpriseOAuthEnrollmentScreenHandler::HandleClose));
  web_ui_->RegisterMessageCallback(
      "oauthEnrollCompleteLogin",
      NewCallback(
          this,
          &EnterpriseOAuthEnrollmentScreenHandler::HandleCompleteLogin));
  web_ui_->RegisterMessageCallback(
      "oauthEnrollRetry",
      NewCallback(
          this,
          &EnterpriseOAuthEnrollmentScreenHandler::HandleRetry));
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

void EnterpriseOAuthEnrollmentScreenHandler::OnOAuthGetAccessTokenFailure(
    const GoogleServiceAuthError& error) {
  ShowAuthError(error);
}

void EnterpriseOAuthEnrollmentScreenHandler::OnOAuthWrapBridgeSuccess(
    const std::string& service_scope,
    const std::string& token,
    const std::string& expires_in) {
  DCHECK_EQ(service_scope, GaiaConstants::kDeviceManagementServiceOAuth);

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
  base::StringValue step_value(step);
  web_ui_->CallJavascriptFunction("oobe.OAuthEnrollmentScreen.showStep",
                                  step_value);
}

void EnterpriseOAuthEnrollmentScreenHandler::ShowError(int message_id,
                                                       bool retry) {
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

  browsing_data_remover_ = new BrowsingDataRemover(
      Profile::FromBrowserContext(web_ui_->tab_contents()->browser_context()),
      BrowsingDataRemover::EVERYTHING,
      base::Time());
  browsing_data_remover_->AddObserver(this);
  browsing_data_remover_->Remove(BrowsingDataRemover::REMOVE_SITE_DATA);
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
