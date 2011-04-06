// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/enterprise_enrollment_screen.h"

#include "base/logging.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/screen_observer.h"
#include "chrome/common/net/gaia/gaia_constants.h"

namespace chromeos {

EnterpriseEnrollmentScreen::EnterpriseEnrollmentScreen(
    WizardScreenDelegate* delegate)
    : ViewScreen<EnterpriseEnrollmentView>(delegate) {}

EnterpriseEnrollmentScreen::~EnterpriseEnrollmentScreen() {}

void EnterpriseEnrollmentScreen::Authenticate(const std::string& user,
                                              const std::string& password,
                                              const std::string& captcha,
                                              const std::string& access_code) {
  captcha_token_.clear();
  auth_fetcher_.reset(
      new GaiaAuthFetcher(this, GaiaConstants::kChromeSource,
                          g_browser_process->system_request_context()));

  if (access_code.empty()) {
    auth_fetcher_->StartClientLogin(user, password,
                                    GaiaConstants::kDeviceManagementService,
                                    captcha_token_, captcha,
                                    GaiaAuthFetcher::HostedAccountsAllowed);
  } else {
    auth_fetcher_->StartClientLogin(user, access_code,
                                    GaiaConstants::kDeviceManagementService,
                                    std::string(), std::string(),
                                    GaiaAuthFetcher::HostedAccountsAllowed);
  }
}

void EnterpriseEnrollmentScreen::CancelEnrollment() {
  auth_fetcher_.reset();
  ScreenObserver* observer = delegate()->GetObserver(this);
  observer->OnExit(ScreenObserver::ENTERPRISE_ENROLLMENT_CANCELLED);
}

void EnterpriseEnrollmentScreen::CloseConfirmation() {
  auth_fetcher_.reset();
  ScreenObserver* observer = delegate()->GetObserver(this);
  observer->OnExit(ScreenObserver::ENTERPRISE_ENROLLMENT_COMPLETED);
}

EnterpriseEnrollmentView* EnterpriseEnrollmentScreen::AllocateView() {
  return new EnterpriseEnrollmentView(this);
}

void EnterpriseEnrollmentScreen::OnClientLoginSuccess(
    const ClientLoginResult& result) {
  auth_fetcher_->StartIssueAuthToken(result.sid, result.lsid,
                                     GaiaConstants::kDeviceManagementService);
}

void EnterpriseEnrollmentScreen::OnClientLoginFailure(
    const GoogleServiceAuthError& error) {
  HandleAuthError(error);
}

void EnterpriseEnrollmentScreen::OnIssueAuthTokenSuccess(
    const std::string& service,
    const std::string& auth_token) {
  if (service != GaiaConstants::kDeviceManagementService) {
    NOTREACHED() << service;
    return;
  }

  auth_fetcher_.reset();

  // TODO(mnissler): Trigger actual enrollment here!

  if (view())
    view()->ShowConfirmationScreen();
}

void EnterpriseEnrollmentScreen::OnIssueAuthTokenFailure(
    const std::string& service,
    const GoogleServiceAuthError& error) {
  if (service != GaiaConstants::kDeviceManagementService) {
    NOTREACHED() << service;
    return;
  }

  HandleAuthError(error);
}

void EnterpriseEnrollmentScreen::HandleAuthError(
    const GoogleServiceAuthError& error) {
  scoped_ptr<GaiaAuthFetcher> scoped_killer(auth_fetcher_.release());

  if (!view())
    return;

  switch (error.state()) {
    case GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS:
    case GoogleServiceAuthError::CONNECTION_FAILED:
    case GoogleServiceAuthError::CAPTCHA_REQUIRED:
    case GoogleServiceAuthError::TWO_FACTOR:
      view()->ShowAuthError(error);
      return;
    case GoogleServiceAuthError::USER_NOT_SIGNED_UP:
    case GoogleServiceAuthError::ACCOUNT_DELETED:
    case GoogleServiceAuthError::ACCOUNT_DISABLED:
    case GoogleServiceAuthError::SERVICE_UNAVAILABLE:
      view()->ShowAccountError();
      return;
    case GoogleServiceAuthError::NONE:
    case GoogleServiceAuthError::HOSTED_NOT_ALLOWED:
      NOTREACHED() << error.state();
      // fall through.
    case GoogleServiceAuthError::REQUEST_CANCELED:
      LOG(ERROR) << "Unexpected GAIA auth error: " << error.state();
      view()->ShowFatalAuthError();
      return;
  }

  NOTREACHED() << error.state();
}

}  // namespace chromeos
