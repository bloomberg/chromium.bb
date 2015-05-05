// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/signin/oauth2_login_verifier.h"

#include <vector>

#include "base/logging.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace {

bool IsConnectionOrServiceError(const GoogleServiceAuthError& error) {
  return error.state() == GoogleServiceAuthError::CONNECTION_FAILED ||
         error.state() == GoogleServiceAuthError::SERVICE_UNAVAILABLE ||
         error.state() == GoogleServiceAuthError::REQUEST_CANCELED;
}

}  // namespace

namespace chromeos {

OAuth2LoginVerifier::OAuth2LoginVerifier(
    OAuth2LoginVerifier::Delegate* delegate,
    GaiaCookieManagerService* cookie_manager_service,
    const std::string& primary_account_id,
    const std::string& oauthlogin_access_token)
    : delegate_(delegate),
      cookie_manager_service_(cookie_manager_service),
      primary_account_id_(primary_account_id),
      access_token_(oauthlogin_access_token) {
  DCHECK(delegate);
  cookie_manager_service_->AddObserver(this);
}

OAuth2LoginVerifier::~OAuth2LoginVerifier() {
  cookie_manager_service_->RemoveObserver(this);
}

void OAuth2LoginVerifier::VerifyUserCookies(Profile* profile) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  std::vector<std::pair<std::string, bool> > accounts;
  if (cookie_manager_service_->ListAccounts(&accounts)) {
    OnGaiaAccountsInCookieUpdated(
        accounts, GoogleServiceAuthError(GoogleServiceAuthError::NONE));
  }
}

void OAuth2LoginVerifier::VerifyProfileTokens(Profile* profile) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // TODO(xiyuan,mlerman): Use |access_token_| to cut down one round trip.
  // See http://crbug.com/483596
  cookie_manager_service_->AddAccountToCookie(primary_account_id_);
}

void OAuth2LoginVerifier::OnAddAccountToCookieCompleted(
    const std::string& account_id,
    const GoogleServiceAuthError& error) {
  if (account_id != primary_account_id_)
    return;

  if (error.state() == GoogleServiceAuthError::State::NONE) {
    VLOG(1) << "MergeSession successful.";
    delegate_->OnSessionMergeSuccess();
    return;
  }

  LOG(WARNING) << "Failed MergeSession request,"
               << " error: " << error.state();
  delegate_->OnSessionMergeFailure(IsConnectionOrServiceError(error));
}

void OAuth2LoginVerifier::OnGaiaAccountsInCookieUpdated(
      const std::vector<std::pair<std::string, bool> >& accounts,
      const GoogleServiceAuthError& error) {
  if (error.state() == GoogleServiceAuthError::State::NONE) {
    VLOG(1) << "ListAccounts successful.";
    delegate_->OnListAccountsSuccess(accounts);
    return;
  }

  LOG(WARNING) << "Failed to get list of session accounts, "
               << " error: " << error.state();
  delegate_->OnListAccountsFailure(IsConnectionOrServiceError(error));
}

}  // namespace chromeos
