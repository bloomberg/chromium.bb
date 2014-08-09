// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/account_reconcilor.h"

#include <algorithm>

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_client.h"
#include "components/signin/core/browser/signin_metrics.h"
#include "components/signin/core/browser/signin_oauth_helper.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_oauth_client.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/cookies/canonical_cookie.h"


namespace {

class EmailEqualToFunc : public std::equal_to<std::pair<std::string, bool> > {
 public:
  bool operator()(const std::pair<std::string, bool>& p1,
                  const std::pair<std::string, bool>& p2) const;
};

bool EmailEqualToFunc::operator()(
    const std::pair<std::string, bool>& p1,
    const std::pair<std::string, bool>& p2) const {
  return p1.second == p2.second && gaia::AreEmailsSame(p1.first, p2.first);
}

}  // namespace


// Fetches a refresh token from the given session in the GAIA cookie.  This is
// a best effort only.  If it should fail, another reconcile action will occur
// shortly anyway.
class AccountReconcilor::RefreshTokenFetcher
    : public SigninOAuthHelper,
      public SigninOAuthHelper::Consumer {
 public:
  RefreshTokenFetcher(AccountReconcilor* reconcilor,
                      const std::string& account_id,
                      int session_index,
                      const std::string& signin_scoped_device_id);
  virtual ~RefreshTokenFetcher() {}

 private:
  // Overridden from GaiaAuthConsumer:
  virtual void OnSigninOAuthInformationAvailable(
      const std::string& email,
      const std::string& display_email,
      const std::string& refresh_token) OVERRIDE;

  // Called when an error occurs while getting the information.
  virtual void OnSigninOAuthInformationFailure(
      const GoogleServiceAuthError& error) OVERRIDE;

  AccountReconcilor* reconcilor_;
  const std::string account_id_;
  int session_index_;

  DISALLOW_COPY_AND_ASSIGN(RefreshTokenFetcher);
};

AccountReconcilor::RefreshTokenFetcher::RefreshTokenFetcher(
    AccountReconcilor* reconcilor,
    const std::string& account_id,
    int session_index,
    const std::string& signin_scoped_device_id)
    : SigninOAuthHelper(reconcilor->client()->GetURLRequestContext(),
                        base::IntToString(session_index),
                        signin_scoped_device_id,
                        this),
      reconcilor_(reconcilor),
      account_id_(account_id),
      session_index_(session_index) {
  DCHECK(reconcilor_);
  DCHECK(!account_id.empty());
}

void AccountReconcilor::RefreshTokenFetcher::OnSigninOAuthInformationAvailable(
    const std::string& email,
    const std::string& display_email,
    const std::string& refresh_token) {
  VLOG(1) << "RefreshTokenFetcher::OnSigninOAuthInformationAvailable:"
          << " account=" << account_id_ << " email=" << email
          << " displayEmail=" << display_email;

  // TODO(rogerta): because of the problem with email vs displayEmail and
  // emails that have been canonicalized, the argument |email| is used here
  // to make sure the correct string is used when calling the token service.
  // This will be cleaned up when chrome moves to using gaia obfuscated id.
  reconcilor_->HandleRefreshTokenFetched(email, refresh_token);
}

void AccountReconcilor::RefreshTokenFetcher::OnSigninOAuthInformationFailure(
    const GoogleServiceAuthError& error) {
  VLOG(1) << "RefreshTokenFetcher::OnSigninOAuthInformationFailure:"
          << " account=" << account_id_ << " session_index=" << session_index_;
  reconcilor_->HandleRefreshTokenFetched(account_id_, std::string());
}

bool AccountReconcilor::EmailLessFunc::operator()(const std::string& s1,
                                                  const std::string& s2) const {
  return gaia::CanonicalizeEmail(s1) < gaia::CanonicalizeEmail(s2);
}

class AccountReconcilor::UserIdFetcher
    : public gaia::GaiaOAuthClient::Delegate {
 public:
  UserIdFetcher(AccountReconcilor* reconcilor,
                const std::string& access_token,
                const std::string& account_id);

  // Returns the scopes needed by the UserIdFetcher.
  static OAuth2TokenService::ScopeSet GetScopes();

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
      gaia_auth_client_(reconcilor_->client()->GetURLRequestContext()) {
  DCHECK(reconcilor_);
  DCHECK(!account_id_.empty());

  const int kMaxRetries = 5;
  gaia_auth_client_.GetUserId(access_token_, kMaxRetries, this);
}

// static
OAuth2TokenService::ScopeSet AccountReconcilor::UserIdFetcher::GetScopes() {
  OAuth2TokenService::ScopeSet scopes;
  scopes.insert("https://www.googleapis.com/auth/userinfo.profile");
  return scopes;
}

void AccountReconcilor::UserIdFetcher::OnGetUserIdResponse(
    const std::string& user_id) {
  VLOG(1) << "AccountReconcilor::OnGetUserIdResponse: " << account_id_;

  // HandleSuccessfulAccountIdCheck() may delete |this|, so call it last.
  reconcilor_->HandleSuccessfulAccountIdCheck(account_id_);
}

void AccountReconcilor::UserIdFetcher::OnOAuthError() {
  VLOG(1) << "AccountReconcilor::OnOAuthError: " << account_id_;

  // Invalidate the access token to force a refetch next time.
  reconcilor_->token_service()->InvalidateToken(
      account_id_, GetScopes(), access_token_);

  // HandleFailedAccountIdCheck() may delete |this|, so call it last.
  reconcilor_->HandleFailedAccountIdCheck(account_id_);
}

void AccountReconcilor::UserIdFetcher::OnNetworkError(int response_code) {
  VLOG(1) << "AccountReconcilor::OnNetworkError: " << account_id_
          << " response_code=" << response_code;

  // TODO(rogerta): some response error should not be treated like
  // permanent errors.  Figure out appropriate ones.
  // HandleFailedAccountIdCheck() may delete |this|, so call it last.
  reconcilor_->HandleFailedAccountIdCheck(account_id_);
}

AccountReconcilor::AccountReconcilor(ProfileOAuth2TokenService* token_service,
                                     SigninManagerBase* signin_manager,
                                     SigninClient* client)
    : OAuth2TokenService::Consumer("account_reconcilor"),
      token_service_(token_service),
      signin_manager_(signin_manager),
      client_(client),
      merge_session_helper_(token_service_,
                            client->GetURLRequestContext(),
                            this),
      registered_with_token_service_(false),
      is_reconcile_started_(false),
      first_execution_(true),
      are_gaia_accounts_set_(false),
      requests_(NULL) {
  VLOG(1) << "AccountReconcilor::AccountReconcilor";
}

AccountReconcilor::~AccountReconcilor() {
  VLOG(1) << "AccountReconcilor::~AccountReconcilor";
  // Make sure shutdown was called first.
  DCHECK(!registered_with_token_service_);
  DCHECK(!requests_);
  DCHECK_EQ(0u, user_id_fetchers_.size());
  DCHECK_EQ(0u, refresh_token_fetchers_.size());
}

void AccountReconcilor::Initialize(bool start_reconcile_if_tokens_available) {
  VLOG(1) << "AccountReconcilor::Initialize";
  RegisterWithSigninManager();

  // If this user is not signed in, the reconcilor should do nothing but
  // wait for signin.
  if (IsProfileConnected()) {
    RegisterForCookieChanges();
    RegisterWithTokenService();

    // Start a reconcile if the tokens are already loaded.
    if (start_reconcile_if_tokens_available &&
        token_service_->GetAccounts().size() > 0) {
      StartReconcile();
    }
  }
}

void AccountReconcilor::Shutdown() {
  VLOG(1) << "AccountReconcilor::Shutdown";
  merge_session_helper_.CancelAll();
  merge_session_helper_.RemoveObserver(this);
  gaia_fetcher_.reset();
  get_gaia_accounts_callbacks_.clear();
  DeleteFetchers();
  UnregisterWithSigninManager();
  UnregisterWithTokenService();
  UnregisterForCookieChanges();
}

void AccountReconcilor::AddMergeSessionObserver(
    MergeSessionHelper::Observer* observer) {
  merge_session_helper_.AddObserver(observer);
}

void AccountReconcilor::RemoveMergeSessionObserver(
    MergeSessionHelper::Observer* observer) {
  merge_session_helper_.RemoveObserver(observer);
}

void AccountReconcilor::DeleteFetchers() {
  delete[] requests_;
  requests_ = NULL;

  user_id_fetchers_.clear();
  refresh_token_fetchers_.clear();
}

bool AccountReconcilor::AreAllRefreshTokensChecked() const {
  return chrome_accounts_.size() ==
         (valid_chrome_accounts_.size() + invalid_chrome_accounts_.size());
}

void AccountReconcilor::RegisterForCookieChanges() {
  // First clear any existing registration to avoid DCHECKs that can otherwise
  // go off in some embedders on reauth (e.g., ChromeSigninClient).
  UnregisterForCookieChanges();
  cookie_changed_subscription_ = client_->AddCookieChangedCallback(
      base::Bind(&AccountReconcilor::OnCookieChanged, base::Unretained(this)));
}

void AccountReconcilor::UnregisterForCookieChanges() {
  cookie_changed_subscription_.reset();
}

void AccountReconcilor::RegisterWithSigninManager() {
  signin_manager_->AddObserver(this);
}

void AccountReconcilor::UnregisterWithSigninManager() {
  signin_manager_->RemoveObserver(this);
}

void AccountReconcilor::RegisterWithTokenService() {
  VLOG(1) << "AccountReconcilor::RegisterWithTokenService";
  // During re-auth, the reconcilor will get a callback about successful signin
  // even when the profile is already connected.  Avoid re-registering
  // with the token service since this will DCHECK.
  if (registered_with_token_service_)
    return;

  token_service_->AddObserver(this);
  registered_with_token_service_ = true;
}

void AccountReconcilor::UnregisterWithTokenService() {
  if (!registered_with_token_service_)
    return;

  token_service_->RemoveObserver(this);
  registered_with_token_service_ = false;
}

bool AccountReconcilor::IsProfileConnected() {
  return !signin_manager_->GetAuthenticatedUsername().empty();
}

void AccountReconcilor::OnCookieChanged(const net::CanonicalCookie* cookie) {
  if (cookie->Name() == "LSID" &&
      cookie->Domain() == GaiaUrls::GetInstance()->gaia_url().host() &&
      cookie->IsSecure() && cookie->IsHttpOnly()) {
    VLOG(1) << "AccountReconcilor::OnCookieChanged: LSID changed";

    // It is possible that O2RT is not available at this moment.
    if (!token_service_->GetAccounts().size()) {
      VLOG(1) << "AccountReconcilor::OnCookieChanged: cookie change is ingored"
                 "because O2RT is not available yet.";
      return;
    }

    StartReconcile();
  }
}

void AccountReconcilor::OnRefreshTokenRevoked(const std::string& account_id) {
  VLOG(1) << "AccountReconcilor::OnRefreshTokenRevoked: " << account_id;
  PerformStartRemoveAction(account_id);
}

void AccountReconcilor::OnEndBatchChanges() {
  VLOG(1) << "AccountReconcilor::OnEndBatchChanges";
  StartReconcile();
}

void AccountReconcilor::GoogleSigninSucceeded(const std::string& username,
                                              const std::string& password) {
  VLOG(1) << "AccountReconcilor::GoogleSigninSucceeded: signed in";
  RegisterForCookieChanges();
  RegisterWithTokenService();
}

void AccountReconcilor::GoogleSignedOut(const std::string& username) {
  VLOG(1) << "AccountReconcilor::GoogleSignedOut: signed out";
  gaia_fetcher_.reset();
  get_gaia_accounts_callbacks_.clear();
  AbortReconcile();
  UnregisterWithTokenService();
  UnregisterForCookieChanges();
  PerformLogoutAllAccountsAction();
}

void AccountReconcilor::PerformMergeAction(const std::string& account_id) {
  if (!switches::IsEnableAccountConsistency()) {
    MarkAccountAsAddedToCookie(account_id);
    return;
  }
  VLOG(1) << "AccountReconcilor::PerformMergeAction: " << account_id;
  merge_session_helper_.LogIn(account_id);
}

void AccountReconcilor::PerformStartRemoveAction(
    const std::string& account_id) {
  VLOG(1) << "AccountReconcilor::PerformStartRemoveAction: " << account_id;
  GetAccountsFromCookie(base::Bind(
      &AccountReconcilor::PerformFinishRemoveAction,
      base::Unretained(this),
      account_id));
}

void AccountReconcilor::PerformFinishRemoveAction(
    const std::string& account_id,
    const GoogleServiceAuthError& error,
    const std::vector<std::pair<std::string, bool> >& accounts) {
  if (!switches::IsEnableAccountConsistency())
    return;
  VLOG(1) << "AccountReconcilor::PerformFinishRemoveAction:"
          << " account=" << account_id << " error=" << error.ToString();
  if (error.state() == GoogleServiceAuthError::NONE) {
    AbortReconcile();
    std::vector<std::string> accounts_only;
    for (std::vector<std::pair<std::string, bool> >::const_iterator i =
             accounts.begin();
         i != accounts.end();
         ++i) {
      accounts_only.push_back(i->first);
    }
    merge_session_helper_.LogOut(account_id, accounts_only);
  }
  // Wait for the next ReconcileAction if there is an error.
}

void AccountReconcilor::PerformAddToChromeAction(
    const std::string& account_id,
    int session_index,
    const std::string& signin_scoped_device_id) {
  if (!switches::IsEnableAccountConsistency()) {
    MarkAccountAsAddedToChrome(account_id);
    return;
  }
  VLOG(1) << "AccountReconcilor::PerformAddToChromeAction:"
          << " account=" << account_id << " session_index=" << session_index;

#if !defined(OS_ANDROID) && !defined(OS_IOS)
  refresh_token_fetchers_.push_back(new RefreshTokenFetcher(
      this, account_id, session_index, signin_scoped_device_id));
#endif
}

void AccountReconcilor::PerformLogoutAllAccountsAction() {
  if (!switches::IsEnableAccountConsistency())
    return;
  VLOG(1) << "AccountReconcilor::PerformLogoutAllAccountsAction";
  merge_session_helper_.LogOutAllAccounts();
}

void AccountReconcilor::StartReconcile() {
  if (!IsProfileConnected() || is_reconcile_started_ ||
      get_gaia_accounts_callbacks_.size() > 0 ||
      merge_session_helper_.is_running())
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
  DeleteFetchers();
  valid_chrome_accounts_.clear();
  invalid_chrome_accounts_.clear();
  add_to_cookie_.clear();
  add_to_chrome_.clear();
  ValidateAccountsFromTokenService();
}

void AccountReconcilor::GetAccountsFromCookie(
    GetAccountsFromCookieCallback callback) {
  get_gaia_accounts_callbacks_.push_back(callback);
  if (!gaia_fetcher_) {
    // There is no list account request in flight.
    gaia_fetcher_.reset(new GaiaAuthFetcher(
        this, GaiaConstants::kChromeSource, client_->GetURLRequestContext()));
    gaia_fetcher_->StartListAccounts();
  }
}

void AccountReconcilor::OnListAccountsSuccess(const std::string& data) {
  gaia_fetcher_.reset();

  // Get account information from response data.
  std::vector<std::pair<std::string, bool> > gaia_accounts;
  bool valid_json = gaia::ParseListAccountsData(data, &gaia_accounts);
  if (!valid_json) {
    VLOG(1) << "AccountReconcilor::OnListAccountsSuccess: parsing error";
  } else if (gaia_accounts.size() > 0) {
    VLOG(1) << "AccountReconcilor::OnListAccountsSuccess: "
            << "Gaia " << gaia_accounts.size() << " accounts, "
            << "Primary is '" << gaia_accounts[0].first << "'";
  } else {
    VLOG(1) << "AccountReconcilor::OnListAccountsSuccess: No accounts";
  }

  // There must be at least one callback waiting for result.
  DCHECK(!get_gaia_accounts_callbacks_.empty());

  GoogleServiceAuthError error =
      !valid_json ? GoogleServiceAuthError(
                        GoogleServiceAuthError::UNEXPECTED_SERVICE_RESPONSE)
                  : GoogleServiceAuthError::AuthErrorNone();
  get_gaia_accounts_callbacks_.front().Run(error, gaia_accounts);
  get_gaia_accounts_callbacks_.pop_front();

  MayBeDoNextListAccounts();
}

void AccountReconcilor::OnListAccountsFailure(
    const GoogleServiceAuthError& error) {
  gaia_fetcher_.reset();
  VLOG(1) << "AccountReconcilor::OnListAccountsFailure: " << error.ToString();
  std::vector<std::pair<std::string, bool> > empty_accounts;

  // There must be at least one callback waiting for result.
  DCHECK(!get_gaia_accounts_callbacks_.empty());

  get_gaia_accounts_callbacks_.front().Run(error, empty_accounts);
  get_gaia_accounts_callbacks_.pop_front();

  MayBeDoNextListAccounts();
}

void AccountReconcilor::MayBeDoNextListAccounts() {
  if (!get_gaia_accounts_callbacks_.empty()) {
    gaia_fetcher_.reset(new GaiaAuthFetcher(
        this, GaiaConstants::kChromeSource, client_->GetURLRequestContext()));
    gaia_fetcher_->StartListAccounts();
  }
}

void AccountReconcilor::ContinueReconcileActionAfterGetGaiaAccounts(
    const GoogleServiceAuthError& error,
    const std::vector<std::pair<std::string, bool> >& accounts) {
  if (error.state() == GoogleServiceAuthError::NONE) {
    gaia_accounts_ = accounts;
    are_gaia_accounts_set_ = true;
    FinishReconcile();
  } else {
    AbortReconcile();
  }
}

void AccountReconcilor::ValidateAccountsFromTokenService() {
  primary_account_ = signin_manager_->GetAuthenticatedUsername();
  DCHECK(!primary_account_.empty());

  chrome_accounts_ = token_service_->GetAccounts();

  VLOG(1) << "AccountReconcilor::ValidateAccountsFromTokenService: "
          << "Chrome " << chrome_accounts_.size() << " accounts, "
          << "Primary is '" << primary_account_ << "'";

  DCHECK(!requests_);
  requests_ =
      new scoped_ptr<OAuth2TokenService::Request>[chrome_accounts_.size()];
  const OAuth2TokenService::ScopeSet scopes =
      AccountReconcilor::UserIdFetcher::GetScopes();
  for (size_t i = 0; i < chrome_accounts_.size(); ++i) {
    requests_[i] =
        token_service_->StartRequest(chrome_accounts_[i], scopes, this);
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

  VLOG(1) << "AccountReconcilor::OnGetTokenSuccess: valid " << account_id;

  DCHECK(!user_id_fetchers_[index]);
  user_id_fetchers_[index] = new UserIdFetcher(this, access_token, account_id);
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

  VLOG(1) << "AccountReconcilor::OnGetTokenFailure: invalid " << account_id;
  HandleFailedAccountIdCheck(account_id);
}

void AccountReconcilor::OnNewProfileManagementFlagChanged(
    bool new_flag_status) {
  if (new_flag_status) {
    // The reconciler may have been newly created just before this call, or may
    // have already existed and in mid-reconcile. To err on the safe side, force
    // a restart.
    Shutdown();
    Initialize(true);
  } else {
    Shutdown();
  }
}

void AccountReconcilor::FinishReconcile() {
  // Make sure that the process of validating the gaia cookie and the oauth2
  // tokens individually is done before proceeding with reconciliation.
  if (!are_gaia_accounts_set_ || !AreAllRefreshTokensChecked())
    return;

  VLOG(1) << "AccountReconcilor::FinishReconcile";

  DeleteFetchers();

  DCHECK(add_to_cookie_.empty());
  DCHECK(add_to_chrome_.empty());
  int number_gaia_accounts = gaia_accounts_.size();
  bool are_primaries_equal =
      number_gaia_accounts > 0 &&
      gaia::AreEmailsSame(primary_account_, gaia_accounts_[0].first);


  if (are_primaries_equal) {
    // Determine if we need to merge accounts from gaia cookie to chrome.
    for (size_t i = 0; i < gaia_accounts_.size(); ++i) {
      const std::string& gaia_account = gaia_accounts_[i].first;
      if (gaia_accounts_[i].second &&
          valid_chrome_accounts_.find(gaia_account) ==
              valid_chrome_accounts_.end()) {
        add_to_chrome_.push_back(std::make_pair(gaia_account, i));
      }
    }
  } else {
    VLOG(1) << "AccountReconcilor::FinishReconcile: rebuild cookie";
    // Really messed up state.  Blow away the gaia cookie completely and
    // rebuild it, making sure the primary account as specified by the
    // SigninManager is the first session in the gaia cookie.
    PerformLogoutAllAccountsAction();
    gaia_accounts_.clear();
  }

  // Create a list of accounts that need to be added to the gaia cookie.
  // The primary account must be first to make sure it becomes the default
  // account in the case where chrome is completely rebuilding the cookie.
  add_to_cookie_.push_back(primary_account_);
  for (EmailSet::const_iterator i = valid_chrome_accounts_.begin();
        i != valid_chrome_accounts_.end();
        ++i) {
    if (*i != primary_account_)
      add_to_cookie_.push_back(*i);
  }

  // For each account known to chrome, PerformMergeAction() if the account is
  // not already in the cookie jar or its state is invalid, or signal merge
  // completed otherwise.  Make a copy of |add_to_cookie_| since calls to
  // SignalComplete() will change the array.
  std::vector<std::string> add_to_cookie_copy = add_to_cookie_;
  int added_to_cookie = 0;
  for (size_t i = 0; i < add_to_cookie_copy.size(); ++i) {
    if (gaia_accounts_.end() !=
            std::find_if(gaia_accounts_.begin(),
                         gaia_accounts_.end(),
                         std::bind1st(EmailEqualToFunc(),
                                      std::make_pair(add_to_cookie_copy[i],
                                                     true)))) {
      merge_session_helper_.SignalComplete(
          add_to_cookie_copy[i],
          GoogleServiceAuthError::AuthErrorNone());
    } else {
      PerformMergeAction(add_to_cookie_copy[i]);
      added_to_cookie++;
    }
  }

  std::string signin_scoped_device_id = client_->GetSigninScopedDeviceId();
  // For each account in the gaia cookie not known to chrome,
  // PerformAddToChromeAction. Make a copy of |add_to_chrome| since calls to
  // PerformAddToChromeAction() may modify this array.
  std::vector<std::pair<std::string, int> > add_to_chrome_copy = add_to_chrome_;
  for (std::vector<std::pair<std::string, int> >::const_iterator i =
           add_to_chrome_copy.begin();
       i != add_to_chrome_copy.end();
       ++i) {
    PerformAddToChromeAction(i->first, i->second, signin_scoped_device_id);
  }

  signin_metrics::LogSigninAccountReconciliation(valid_chrome_accounts_.size(),
                                                 added_to_cookie,
                                                 add_to_chrome_.size(),
                                                 are_primaries_equal,
                                                 first_execution_,
                                                 number_gaia_accounts);
  first_execution_ = false;
  CalculateIfReconcileIsDone();
  ScheduleStartReconcileIfChromeAccountsChanged();
}

void AccountReconcilor::AbortReconcile() {
  VLOG(1) << "AccountReconcilor::AbortReconcile: we'll try again later";
  DeleteFetchers();
  add_to_cookie_.clear();
  add_to_chrome_.clear();
  CalculateIfReconcileIsDone();
}

void AccountReconcilor::CalculateIfReconcileIsDone() {
  is_reconcile_started_ = !add_to_cookie_.empty() || !add_to_chrome_.empty();
  if (!is_reconcile_started_)
    VLOG(1) << "AccountReconcilor::CalculateIfReconcileIsDone: done";
}

void AccountReconcilor::ScheduleStartReconcileIfChromeAccountsChanged() {
  if (is_reconcile_started_)
    return;

  // Start a reconcile as the token accounts have changed.
  VLOG(1) << "AccountReconcilor::StartReconcileIfChromeAccountsChanged";
  std::vector<std::string> reconciled_accounts(chrome_accounts_);
  std::vector<std::string> new_chrome_accounts(token_service_->GetAccounts());
  std::sort(reconciled_accounts.begin(), reconciled_accounts.end());
  std::sort(new_chrome_accounts.begin(), new_chrome_accounts.end());
  if (reconciled_accounts != new_chrome_accounts) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&AccountReconcilor::StartReconcile, base::Unretained(this)));
  }
}

// Remove the account from the list that is being merged.
bool AccountReconcilor::MarkAccountAsAddedToCookie(
    const std::string& account_id) {
  for (std::vector<std::string>::iterator i = add_to_cookie_.begin();
       i != add_to_cookie_.end();
       ++i) {
    if (account_id == *i) {
      add_to_cookie_.erase(i);
      return true;
    }
  }
  return false;
}

void AccountReconcilor::MergeSessionCompleted(
    const std::string& account_id,
    const GoogleServiceAuthError& error) {
  VLOG(1) << "AccountReconcilor::MergeSessionCompleted: account_id="
          << account_id;

  if (MarkAccountAsAddedToCookie(account_id)) {
    CalculateIfReconcileIsDone();
    ScheduleStartReconcileIfChromeAccountsChanged();
  }
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

void AccountReconcilor::PerformAddAccountToTokenService(
    const std::string& account_id,
    const std::string& refresh_token) {
  // The flow should never get to this method if new_profile_management is
  // false, but better safe than sorry.
  if (!switches::IsEnableAccountConsistency())
    return;
  token_service_->UpdateCredentials(account_id, refresh_token);
}

// Remove the account from the list that is being updated.
void AccountReconcilor::MarkAccountAsAddedToChrome(
    const std::string& account_id) {
  for (std::vector<std::pair<std::string, int> >::iterator i =
           add_to_chrome_.begin();
       i != add_to_chrome_.end();
       ++i) {
    if (gaia::AreEmailsSame(account_id, i->first)) {
      add_to_chrome_.erase(i);
      break;
    }
  }
}

void AccountReconcilor::HandleRefreshTokenFetched(
    const std::string& account_id,
    const std::string& refresh_token) {
  if (!refresh_token.empty())
    PerformAddAccountToTokenService(account_id, refresh_token);

  MarkAccountAsAddedToChrome(account_id);
  CalculateIfReconcileIsDone();
}
