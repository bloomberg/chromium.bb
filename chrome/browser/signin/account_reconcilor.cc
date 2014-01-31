// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/account_reconcilor.h"

#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/net/chrome_cookie_notification_details.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/profile_oauth2_token_service.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_oauth_client.h"
#include "google_apis/gaia/gaia_urls.h"

namespace {

// Fetches a refresh token from the given session in the GAIA cookie.  This is
// a best effort only.  If it should fail, another reconcile action will occur
// shortly anyway.
class RefreshTokenFetcher : public GaiaAuthConsumer {
 public:
  RefreshTokenFetcher(Profile* profile,
                      const std::string& account_id,
                      int session_index);
  virtual ~RefreshTokenFetcher() {}

 private:
  // Overridden from GaiaAuthConsumer:
  virtual void OnClientOAuthSuccess(const ClientOAuthResult& result) OVERRIDE;
  virtual void OnClientOAuthFailure(
      const GoogleServiceAuthError& error) OVERRIDE;

  Profile* profile_;
  GaiaAuthFetcher fetcher_;
  const std::string account_id_;
  int session_index_;

  DISALLOW_COPY_AND_ASSIGN(RefreshTokenFetcher);
};

RefreshTokenFetcher::RefreshTokenFetcher(
    Profile* profile,
    const std::string& account_id,
    int session_index)
    : profile_(profile),
      fetcher_(this, GaiaConstants::kChromeSource,
               profile_->GetRequestContext()),
      account_id_(account_id),
      session_index_(session_index) {
  DCHECK(profile_);
  DCHECK(!account_id.empty());
  fetcher_.StartCookieForOAuthLoginTokenExchange(
      base::IntToString(session_index_));
}

void RefreshTokenFetcher::OnClientOAuthSuccess(
    const ClientOAuthResult& result) {
  DVLOG(1) << "RefreshTokenFetcher::OnClientOAuthSuccess:"
           << " account=" << account_id_
           << " session_index=" << session_index_;
  ProfileOAuth2TokenService* token_service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile_);
  token_service->UpdateCredentials(account_id_, result.refresh_token);
  base::MessageLoopProxy::current()->DeleteSoon(FROM_HERE, this);
}

void RefreshTokenFetcher::OnClientOAuthFailure(
    const GoogleServiceAuthError& error) {
  DVLOG(1) << "RefreshTokenFetcher::OnClientOAuthFailure:"
           << " account=" << account_id_
           << " session_index=" << session_index_;
  base::MessageLoopProxy::current()->DeleteSoon(FROM_HERE, this);
}

}  // namespace


class AccountReconcilor::UserIdFetcher
    : public gaia::GaiaOAuthClient::Delegate {
 public:
  UserIdFetcher(AccountReconcilor* reconcilor,
                const std::string& access_token,
                const std::string& account_id);

 private:
  // Overriden from gaia::GaiaOAuthClient::Delegate.
  virtual void OnGetUserIdResponse(const std::string& user_id) OVERRIDE;
  virtual void OnOAuthError() OVERRIDE;
  virtual void OnNetworkError(int response_code) OVERRIDE;

  AccountReconcilor* const reconcilor_;
  const std::string account_id_;
  const std::string access_token_;
  gaia::GaiaOAuthClient gaia_auth_client_;

  DISALLOW_COPY_AND_ASSIGN(UserIdFetcher);
};

AccountReconcilor::UserIdFetcher::UserIdFetcher(AccountReconcilor* reconcilor,
                                                const std::string& access_token,
                                                const std::string& account_id)
    : reconcilor_(reconcilor),
      account_id_(account_id),
      access_token_(access_token),
      gaia_auth_client_(reconcilor_->profile()->GetRequestContext()) {
  DCHECK(reconcilor_);
  DCHECK(!account_id_.empty());

  const int kMaxRetries = 5;
  gaia_auth_client_.GetUserId(access_token_, kMaxRetries, this);
}

void AccountReconcilor::UserIdFetcher::OnGetUserIdResponse(
    const std::string& user_id) {
  DVLOG(1) << "AccountReconcilor::OnGetUserIdResponse: " << account_id_;
  reconcilor_->HandleSuccessfulAccountIdCheck(account_id_);
}

void AccountReconcilor::UserIdFetcher::OnOAuthError() {
  DVLOG(1) << "AccountReconcilor::OnOAuthError: " << account_id_;
  reconcilor_->HandleFailedAccountIdCheck(account_id_);

  // Invalidate the access token to force a refetch next time.
  ProfileOAuth2TokenService* token_service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(reconcilor_->profile());
  token_service->InvalidateToken(account_id_, OAuth2TokenService::ScopeSet(),
                                 access_token_);
}

void AccountReconcilor::UserIdFetcher::OnNetworkError(int response_code) {
  DVLOG(1) << "AccountReconcilor::OnNetworkError: " << account_id_
           << " response_code=" << response_code;

  // TODO(rogerta): some response error should not be treated like
  // permanent errors.  Figure out appropriate ones.
  reconcilor_->HandleFailedAccountIdCheck(account_id_);
}

AccountReconcilor::AccountReconcilor(Profile* profile)
    : OAuth2TokenService::Consumer("account_reconcilor"),
      profile_(profile),
      merge_session_helper_(
          ProfileOAuth2TokenServiceFactory::GetForProfile(profile),
          profile->GetRequestContext(),
          NULL),
      registered_with_token_service_(false),
      is_reconcile_started_(false),
      are_gaia_accounts_set_(false),
      requests_(NULL) {
  DVLOG(1) << "AccountReconcilor::AccountReconcilor";
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
  DCHECK(!registered_with_token_service_);
  DCHECK(registrar_.IsEmpty());
  DCHECK(!reconciliation_timer_.IsRunning());
  DCHECK(!requests_);
  DCHECK_EQ(0u, user_id_fetchers_.size());
}

void AccountReconcilor::Shutdown() {
  DVLOG(1) << "AccountReconcilor::Shutdown";
  merge_session_helper_.CancelAll();
  gaia_fetcher_.reset();
  DeleteAccessTokenRequestsAndUserIdFetchers();
  UnregisterWithSigninManager();
  UnregisterWithTokenService();
  UnregisterWithCookieMonster();
  StopPeriodicReconciliation();
}

void AccountReconcilor::AddMergeSessionObserver(
    MergeSessionHelper::Observer* observer) {
  merge_session_helper_.AddObserver(observer);
}

void AccountReconcilor::RemoveMergeSessionObserver(
    MergeSessionHelper::Observer* observer) {
  merge_session_helper_.RemoveObserver(observer);
}

void AccountReconcilor::DeleteAccessTokenRequestsAndUserIdFetchers() {
  delete[] requests_;
  requests_ = NULL;

  user_id_fetchers_.clear();
}

bool AccountReconcilor::AreAllRefreshTokensChecked() const {
  return chrome_accounts_.size() ==
      (valid_chrome_accounts_.size() + invalid_chrome_accounts_.size());
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
  SigninManagerBase* signin_manager =
      SigninManagerFactory::GetForProfile(profile_);
  signin_manager->AddObserver(this);
}

void AccountReconcilor::UnregisterWithSigninManager() {
  SigninManagerBase* signin_manager =
      SigninManagerFactory::GetForProfile(profile_);
  signin_manager->RemoveObserver(this);
}

void AccountReconcilor::RegisterWithTokenService() {
  DVLOG(1) << "AccountReconcilor::RegisterWithTokenService";
  // During re-auth, the reconcilor will get a GOOGLE_SIGNIN_SUCCESSFUL
  // even when the profile is already connected.  Avoid re-registering
  // with the token service since this will DCHECK.
  if (registered_with_token_service_)
    return;

  ProfileOAuth2TokenService* token_service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile_);
  token_service->AddObserver(this);
  registered_with_token_service_ = true;
}

void AccountReconcilor::UnregisterWithTokenService() {
  if (!registered_with_token_service_)
    return;

  ProfileOAuth2TokenService* token_service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile_);
  token_service->RemoveObserver(this);
  registered_with_token_service_ = false;
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
  StartReconcile();
}

void AccountReconcilor::Observe(int type,
                                const content::NotificationSource& source,
                                const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_COOKIE_CHANGED:
      OnCookieChanged(content::Details<ChromeCookieDetails>(details).ptr());
      break;
    default:
      NOTREACHED();
      break;
  }
}

void AccountReconcilor::OnCookieChanged(ChromeCookieDetails* details) {
  if (details->cookie->Name() == "LSID" &&
      details->cookie->Domain() == GaiaUrls::GetInstance()->gaia_url().host() &&
      details->cookie->IsSecure() &&
      details->cookie->IsHttpOnly()) {
    DVLOG(1) << "AccountReconcilor::OnCookieChanged: LSID changed";
    StartReconcile();
  }
}

void AccountReconcilor::OnRefreshTokenAvailable(const std::string& account_id) {
  DVLOG(1) << "AccountReconcilor::OnRefreshTokenAvailable: " << account_id;
  PerformMergeAction(account_id);
}

void AccountReconcilor::OnRefreshTokenRevoked(const std::string& account_id) {
  DVLOG(1) << "AccountReconcilor::OnRefreshTokenRevoked: " << account_id;
  StartRemoveAction(account_id);
}

void AccountReconcilor::OnRefreshTokensLoaded() {}

void AccountReconcilor::GoogleSigninSucceeded(
    const std::string& username, const std::string& password) {
  DVLOG(1) << "AccountReconcilor::GoogleSigninSucceeded: signed in";
  RegisterWithTokenService();
  StartPeriodicReconciliation();
}

void AccountReconcilor::GoogleSignedOut(const std::string& username) {
  DVLOG(1) << "AccountReconcilor::GoogleSignedOut: signed out";
  UnregisterWithTokenService();
  StopPeriodicReconciliation();
}

void AccountReconcilor::PerformMergeAction(const std::string& account_id) {
  DVLOG(1) << "AccountReconcilor::PerformMergeAction: " << account_id;
  merge_session_helper_.LogIn(account_id);
}

void AccountReconcilor::StartRemoveAction(const std::string& account_id) {
  DVLOG(1) << "AccountReconcilor::StartRemoveAction: " << account_id;
  GetAccountsFromCookie(
      base::Bind(&AccountReconcilor::FinishRemoveAction,
                 base::Unretained(this),
                 account_id));
}

void AccountReconcilor::FinishRemoveAction(
    const std::string& account_id,
    const GoogleServiceAuthError& error,
    const std::vector<std::string>& accounts) {
  DVLOG(1) << "AccountReconcilor::FinishRemoveAction:"
           << " account=" << account_id
           << " error=" << error.ToString();
  if (error.state() == GoogleServiceAuthError::NONE) {
    merge_session_helper_.LogOut(account_id, accounts);
  }
  // Wait for the next ReconcileAction if there is an error.
}

void AccountReconcilor::PerformAddToChromeAction(
    const std::string& account_id,
    int session_index) {
  DVLOG(1) << "AccountReconcilor::PerformAddToChromeAction:"
           << " account=" << account_id
           << " session_index=" << session_index;

#if !defined(OS_ANDROID) && !defined(OS_IOS)
  // RefreshTokenFetcher deletes itself when done.
  new RefreshTokenFetcher(profile_, account_id, session_index);
#endif
}

void AccountReconcilor::PerformLogoutAllAccountsAction() {
  DVLOG(1) << "AccountReconcilor::PerformLogoutAllAccountsAction";
  merge_session_helper_.LogOutAllAccounts();
}

void AccountReconcilor::StartReconcile() {
  if (!IsProfileConnected() || is_reconcile_started_)
    return;

  is_reconcile_started_ = true;

  // Reset state for validating gaia cookie.
  are_gaia_accounts_set_ = false;
  gaia_accounts_.clear();
  GetAccountsFromCookie(base::Bind(
      &AccountReconcilor::ContinueReconcileActionAfterGetGaiaAccounts,
          base::Unretained(this)));

  // Reset state for validating oauth2 tokens.
  primary_account_.clear();
  chrome_accounts_.clear();
  DeleteAccessTokenRequestsAndUserIdFetchers();
  valid_chrome_accounts_.clear();
  invalid_chrome_accounts_.clear();
  ValidateAccountsFromTokenService();
}

void AccountReconcilor::GetAccountsFromCookie(
    GetAccountsFromCookieCallback callback) {
  get_gaia_accounts_callbacks_.push_back(callback);
  if (!gaia_fetcher_) {
    // There is no list account request in flight.
    gaia_fetcher_.reset(new GaiaAuthFetcher(this, GaiaConstants::kChromeSource,
                                            profile_->GetRequestContext()));
    gaia_fetcher_->StartListAccounts();
  }
}

void AccountReconcilor::OnListAccountsSuccess(const std::string& data) {
  gaia_fetcher_.reset();

  // Get account information from response data.
  std::vector<std::string> gaia_accounts = gaia::ParseListAccountsData(data);
  if (gaia_accounts.size() > 0) {
    DVLOG(1) << "AccountReconcilor::OnListAccountsSuccess: "
             << "Gaia " << gaia_accounts.size() << " accounts, "
             << "Primary is '" << gaia_accounts[0] << "'";
  } else {
    DVLOG(1) << "AccountReconcilor::OnListAccountsSuccess: No accounts";
  }

  // There must be at least one callback waiting for result.
  DCHECK(!get_gaia_accounts_callbacks_.empty());

  get_gaia_accounts_callbacks_.front().Run(
      GoogleServiceAuthError::AuthErrorNone(), gaia_accounts);
  get_gaia_accounts_callbacks_.pop_front();

  MayBeDoNextListAccounts();
}

void AccountReconcilor::OnListAccountsFailure(
    const GoogleServiceAuthError& error) {
  gaia_fetcher_.reset();
  DVLOG(1) << "AccountReconcilor::OnListAccountsFailure: " << error.ToString();
  std::vector<std::string> empty_accounts;

  // There must be at least one callback waiting for result.
  DCHECK(!get_gaia_accounts_callbacks_.empty());

  get_gaia_accounts_callbacks_.front().Run(error, empty_accounts);
  get_gaia_accounts_callbacks_.pop_front();

  MayBeDoNextListAccounts();
}

void AccountReconcilor::MayBeDoNextListAccounts() {
  if (!get_gaia_accounts_callbacks_.empty()) {
    gaia_fetcher_.reset(new GaiaAuthFetcher(this, GaiaConstants::kChromeSource,
                                            profile_->GetRequestContext()));
    gaia_fetcher_->StartListAccounts();
  }
}

void AccountReconcilor::ContinueReconcileActionAfterGetGaiaAccounts(
    const GoogleServiceAuthError& error,
    const std::vector<std::string>& accounts) {
  if (error.state() == GoogleServiceAuthError::NONE) {
    gaia_accounts_ = accounts;
  }
  are_gaia_accounts_set_ = true;
  FinishReconcile();
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

  DCHECK_EQ(0u, user_id_fetchers_.size());
  user_id_fetchers_.resize(chrome_accounts_.size());
}

void AccountReconcilor::OnGetTokenSuccess(
    const OAuth2TokenService::Request* request,
    const std::string& access_token,
    const base::Time& expiration_time) {
  size_t index;
  for (index = 0; index < chrome_accounts_.size(); ++index) {
    if (request == requests_[index].get())
      break;
  }
  DCHECK(index < chrome_accounts_.size());

  const std::string& account_id = chrome_accounts_[index];

  DVLOG(1) << "AccountReconcilor::OnGetTokenSuccess: valid " << account_id;

  DCHECK(!user_id_fetchers_[index]);
  user_id_fetchers_[index] =
      new UserIdFetcher(this, access_token, account_id);
}

void AccountReconcilor::OnGetTokenFailure(
    const OAuth2TokenService::Request* request,
    const GoogleServiceAuthError& error) {
  size_t index;
  for (index = 0; index < chrome_accounts_.size(); ++index) {
    if (request == requests_[index].get())
      break;
  }
  DCHECK(index < chrome_accounts_.size());

  const std::string& account_id = chrome_accounts_[index];

  DVLOG(1) << "AccountReconcilor::OnGetTokenFailure: invalid "
           << account_id;
  HandleFailedAccountIdCheck(account_id);
}

void AccountReconcilor::FinishReconcile() {
  // Make sure that the process of validating the gaia cookie and the oauth2
  // tokens individually is done before proceeding with reconciliation.
  if (!are_gaia_accounts_set_ || !AreAllRefreshTokensChecked())
    return;

  DVLOG(1) << "AccountReconcilor::FinishReconcile";

  DeleteAccessTokenRequestsAndUserIdFetchers();

  std::vector<std::string> add_to_cookie;
  std::vector<std::pair<std::string, int> > add_to_chrome;
  bool are_primaries_equal =
      gaia_accounts_.size() > 0 && primary_account_ == gaia_accounts_[0];

  if (are_primaries_equal) {
    // Determine if we need to merge accounts from gaia cookie to chrome.
    for (size_t i = 0; i < gaia_accounts_.size(); ++i) {
      const std::string& gaia_account = gaia_accounts_[i];
      if (valid_chrome_accounts_.find(gaia_account) ==
          valid_chrome_accounts_.end()) {
        add_to_chrome.push_back(std::make_pair(gaia_account, i));
      }
    }

    // Determine if we need to merge accounts from chrome into gaia cookie.
    for (std::set<std::string>::const_iterator i =
             valid_chrome_accounts_.begin();
         i != valid_chrome_accounts_.end(); ++i) {
      if (std::find(gaia_accounts_.begin(), gaia_accounts_.end(), *i) ==
          gaia_accounts_.end()) {
        add_to_cookie.push_back(*i);
      }
    }
  }

  if (!are_primaries_equal) {
    DVLOG(1) << "AccountReconcilor::FinishReconcile: rebuild cookie";
    // Really messed up state.  Blow away the gaia cookie completely and
    // rebuild it, making sure the primary account as specified by the
    // SigninManager is the first session in the gaia cookie.
    PerformLogoutAllAccountsAction();
    PerformMergeAction(primary_account_);
    for (std::set<std::string>::const_iterator i =
             valid_chrome_accounts_.begin();
         i != valid_chrome_accounts_.end(); ++i) {
      if (*i != primary_account_)
        PerformMergeAction(*i);
    }
  } else {
    // For each account known to chrome but not in the gaia cookie,
    // PerformMergeAction().
    for (size_t i = 0; i < add_to_cookie.size(); ++i)
      PerformMergeAction(add_to_cookie[i]);

    // For each account in the gaia cookie not known to chrome,
    // PerformAddToChromeAction.
    for (std::vector<std::pair<std::string, int> >::const_iterator i =
             add_to_chrome.begin();
         i != add_to_chrome.end(); ++i) {
      PerformAddToChromeAction(i->first, i->second);
    }
  }

  is_reconcile_started_ = false;
}

void AccountReconcilor::HandleSuccessfulAccountIdCheck(
    const std::string& account_id) {
  valid_chrome_accounts_.insert(account_id);
  FinishReconcile();
}

void AccountReconcilor::HandleFailedAccountIdCheck(
    const std::string& account_id) {
  invalid_chrome_accounts_.insert(account_id);
  FinishReconcile();
}
