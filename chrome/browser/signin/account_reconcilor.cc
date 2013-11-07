// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/net/chrome_cookie_notification_details.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/account_reconcilor.h"
#include "chrome/browser/signin/google_auto_login_helper.h"
#include "chrome/browser/signin/profile_oauth2_token_service.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "google_apis/gaia/gaia_constants.h"

AccountReconcilor::AccountReconcilor(Profile* profile)
    : profile_(profile),
      are_gaia_accounts_set_(false),
      requests_(NULL) {
  RegisterWithSigninManager();
  RegisterWithCookieMonster();

  // If this profile is not connected, the reconcilor should do nothing but
  // wait for the connection.
  if (IsProfileConnected()) {
    RegisterWithTokenService();
    StartPeriodicReconciliation();
  }
}

AccountReconcilor::~AccountReconcilor() {
  // Make sure shutdown was called first.
  DCHECK(registrar_.IsEmpty());
  DCHECK(!reconciliation_timer_.IsRunning());
  DCHECK(!requests_);
}

void AccountReconcilor::Shutdown() {
  DVLOG(1) << "AccountReconcilor::Shutdown";
  DeleteAccessTokenRequests();
  UnregisterWithSigninManager();
  UnregisterWithTokenService();
  UnregisterWithCookieMonster();
  StopPeriodicReconciliation();
}

void AccountReconcilor::DeleteAccessTokenRequests() {
  delete[] requests_;
  requests_ = NULL;
}

void AccountReconcilor::RegisterWithCookieMonster() {
  content::Source<Profile> source(profile_);
  registrar_.Add(this, chrome::NOTIFICATION_COOKIE_CHANGED, source);
}

void AccountReconcilor::UnregisterWithCookieMonster() {
  content::Source<Profile> source(profile_);
  registrar_.Remove(this, chrome::NOTIFICATION_COOKIE_CHANGED, source);
}

void AccountReconcilor::RegisterWithSigninManager() {
  content::Source<Profile> source(profile_);
  registrar_.Add(this, chrome::NOTIFICATION_GOOGLE_SIGNIN_SUCCESSFUL, source);
  registrar_.Add(this, chrome::NOTIFICATION_GOOGLE_SIGNED_OUT, source);
}

void AccountReconcilor::UnregisterWithSigninManager() {
  content::Source<Profile> source(profile_);
  registrar_.Remove(
      this, chrome::NOTIFICATION_GOOGLE_SIGNIN_SUCCESSFUL, source);
  registrar_.Remove(this, chrome::NOTIFICATION_GOOGLE_SIGNED_OUT, source);
}

void AccountReconcilor::RegisterWithTokenService() {
  ProfileOAuth2TokenService* token_service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile_);
  token_service->AddObserver(this);
}

void AccountReconcilor::UnregisterWithTokenService() {
  ProfileOAuth2TokenService* token_service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile_);
  token_service->RemoveObserver(this);
}

bool AccountReconcilor::IsProfileConnected() {
  return !SigninManagerFactory::GetForProfile(profile_)->
      GetAuthenticatedUsername().empty();
}

void AccountReconcilor::StartPeriodicReconciliation() {
  DVLOG(1) << "AccountReconcilor::StartPeriodicReconciliation";
  // TODO(rogerta): pick appropriate thread and timeout value.
  reconciliation_timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromSeconds(300),
      this,
      &AccountReconcilor::PeriodicReconciliation);
}

void AccountReconcilor::StopPeriodicReconciliation() {
  DVLOG(1) << "AccountReconcilor::StopPeriodicReconciliation";
  reconciliation_timer_.Stop();
}

void AccountReconcilor::PeriodicReconciliation() {
  DVLOG(1) << "AccountReconcilor::PeriodicReconciliation";
  StartReconcileAction();
}

void AccountReconcilor::Observe(int type,
                                const content::NotificationSource& source,
                                const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_GOOGLE_SIGNIN_SUCCESSFUL:
      DVLOG(1) << "AccountReconcilor::Observe: signed in";
      RegisterWithTokenService();
      StartPeriodicReconciliation();
      break;
    case chrome::NOTIFICATION_GOOGLE_SIGNED_OUT:
      DVLOG(1) << "AccountReconcilor::Observe: signed out";
      UnregisterWithTokenService();
      StopPeriodicReconciliation();
      break;
    case chrome::NOTIFICATION_COOKIE_CHANGED:
      OnCookieChanged(content::Details<ChromeCookieDetails>(details).ptr());
      break;
    default:
      NOTREACHED();
      break;
  }
}

void AccountReconcilor::OnCookieChanged(ChromeCookieDetails* details) {
  // TODO(acleung): Filter out cookies by looking at the domain.
  // StartReconcileAction();
}

void AccountReconcilor::OnRefreshTokenAvailable(const std::string& account_id) {
  DVLOG(1) << "AccountReconcilor::OnRefreshTokenAvailable: " << account_id;
  PerformMergeAction(account_id);
}

void AccountReconcilor::OnRefreshTokenRevoked(const std::string& account_id) {
  DVLOG(1) << "AccountReconcilor::OnRefreshTokenRevoked: " << account_id;
  PerformRemoveAction(account_id);
}

void AccountReconcilor::OnRefreshTokensLoaded() {}

void AccountReconcilor::PerformMergeAction(const std::string& account_id) {
  // GoogleAutoLoginHelper deletes itself upon success / failure.
  GoogleAutoLoginHelper* helper = new GoogleAutoLoginHelper(profile_);
  helper->LogIn(account_id);
}

void AccountReconcilor::PerformRemoveAction(const std::string& account_id) {
  // TODO(acleung): Implement this:
}

void AccountReconcilor::StartReconcileAction() {
  if (!IsProfileConnected())
    return;

  // Reset state for validating gaia cookie.
  are_gaia_accounts_set_ = false;
  gaia_accounts_.clear();
  GetAccountsFromCookie();

  // Reset state for validating oauth2 tokens.
  primary_account_.clear();
  chrome_accounts_.clear();
  DeleteAccessTokenRequests();
  valid_chrome_accounts_.clear();
  invalid_chrome_accounts_.clear();
  ValidateAccountsFromTokenService();
}

void AccountReconcilor::GetAccountsFromCookie() {
  gaia_fetcher_.reset(new GaiaAuthFetcher(this, GaiaConstants::kChromeSource,
                                          profile_->GetRequestContext()));
  gaia_fetcher_->StartListAccounts();
}

void AccountReconcilor::OnListAccountsSuccess(const std::string& data) {
  gaia_fetcher_.reset();

  // Get account information from response data.
  gaia_accounts_ = ParseListAccountsData(data);
  if (gaia_accounts_.size() > 0) {
    DVLOG(1) << "AccountReconcilor::OnListAccountsSuccess: "
             << "Gaia " << gaia_accounts_.size() << " accounts, "
             << "Primary is '" << gaia_accounts_[0] << "'";
  } else {
    DVLOG(1) << "AccountReconcilor::OnListAccountsSuccess: No accounts";
  }

  are_gaia_accounts_set_ = true;
  FinishReconcileAction();
}

// static
std::vector<std::string> AccountReconcilor::ParseListAccountsData(
    const std::string& data) {
  std::vector<std::string> account_ids;

  // Parse returned data and make sure we have data.
  scoped_ptr<base::Value> value(base::JSONReader::Read(data));
  if (!value)
    return account_ids;

  base::ListValue* list;
  if (!value->GetAsList(&list) || list->GetSize() < 2)
    return account_ids;

  // Get list of account info.
  base::ListValue* accounts;
  if (!list->GetList(1, &accounts) || accounts == NULL)
    return account_ids;

  // Build a vector of accounts from the cookie.  Order is important: the first
  // account in the list is the primary account.
  for (size_t i = 0; i < accounts->GetSize(); ++i) {
    base::ListValue* account;
    if (accounts->GetList(i, &account) && account != NULL) {
      std::string email;
      if (account->GetString(3, &email) && !email.empty())
        account_ids.push_back(email);
    }
  }

  return account_ids;
}

void AccountReconcilor::OnListAccountsFailure(
    const GoogleServiceAuthError& error) {
  gaia_fetcher_.reset();
  DVLOG(1) << "AccountReconcilor::OnListAccountsFailure: " << error.ToString();

  are_gaia_accounts_set_ = true;
  FinishReconcileAction();
}

void AccountReconcilor::ValidateAccountsFromTokenService() {
  primary_account_ =
      SigninManagerFactory::GetForProfile(profile_)->GetAuthenticatedUsername();
  DCHECK(!primary_account_.empty());

  ProfileOAuth2TokenService* token_service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile_);
  chrome_accounts_ = token_service->GetAccounts();
  DCHECK(chrome_accounts_.size() > 0);

  DVLOG(1) << "AccountReconcilor::ValidateAccountsFromTokenService: "
            << "Chrome " << chrome_accounts_.size() << " accounts, "
            << "Primary is '" << primary_account_ << "'";

  DCHECK(!requests_);
  requests_ =
      new scoped_ptr<OAuth2TokenService::Request>[chrome_accounts_.size()];
  for (size_t i = 0; i < chrome_accounts_.size(); ++i) {
    requests_[i] = token_service->StartRequest(chrome_accounts_[i],
                                               OAuth2TokenService::ScopeSet(),
                                               this);
  }
}

void AccountReconcilor::OnGetTokenSuccess(
    const OAuth2TokenService::Request* request,
    const std::string& access_token,
    const base::Time& expiration_time) {
  DVLOG(1) << "AccountReconcilor::OnGetTokenSuccess: valid "
           << request->GetAccountId();
  valid_chrome_accounts_.insert(request->GetAccountId());
  FinishReconcileAction();
}

void AccountReconcilor::OnGetTokenFailure(
    const OAuth2TokenService::Request* request,
    const GoogleServiceAuthError& error) {
  DVLOG(1) << "AccountReconcilor::OnGetTokenSuccess: invalid "
           << request->GetAccountId();
  invalid_chrome_accounts_.insert(request->GetAccountId());
  FinishReconcileAction();
}

void AccountReconcilor::FinishReconcileAction() {
  // Make sure that the process of validating the gaia cookie and the oauth2
  // tokens individually is done before proceeding with reconciliation.
  if (!are_gaia_accounts_set_ ||
      (chrome_accounts_.size() != (valid_chrome_accounts_.size() +
                                   invalid_chrome_accounts_.size()))) {
    return;
  }

  DVLOG(1) << "AccountReconcilor::FinishReconcileAction";

  bool are_primaries_equal =
      gaia_accounts_.size() > 0 && primary_account_ == gaia_accounts_[0];
  bool have_same_accounts = chrome_accounts_.size() == gaia_accounts_.size();
  if (have_same_accounts) {
    for (size_t i = 0; i < gaia_accounts_.size(); ++i) {
      if (std::find(chrome_accounts_.begin(), chrome_accounts_.end(),
              gaia_accounts_[i]) == chrome_accounts_.end()) {
        have_same_accounts = false;
        break;
      }
    }
  }

  if (!are_primaries_equal || !have_same_accounts) {
    // TODO(rogerta): fix things up.
  }
}
