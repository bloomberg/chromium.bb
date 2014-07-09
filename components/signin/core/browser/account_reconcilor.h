// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_RECONCILOR_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_RECONCILOR_H_

#include <deque>
#include <functional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "google_apis/gaia/gaia_auth_consumer.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/merge_session_helper.h"
#include "google_apis/gaia/oauth2_token_service.h"

class GaiaAuthFetcher;
class ProfileOAuth2TokenService;
class SigninClient;
class SigninOAuthHelper;

namespace net {
class CanonicalCookie;
}

class AccountReconcilor : public KeyedService,
                          public GaiaAuthConsumer,
                          public MergeSessionHelper::Observer,
                          public OAuth2TokenService::Consumer,
                          public OAuth2TokenService::Observer,
                          public SigninManagerBase::Observer {
 public:
  AccountReconcilor(ProfileOAuth2TokenService* token_service,
                    SigninManagerBase* signin_manager,
                    SigninClient* client);
  virtual ~AccountReconcilor();

  void Initialize(bool start_reconcile_if_tokens_available);

  // Signal that the status of the new_profile_management flag has changed.
  // Pass the new status as an explicit parameter since disabling the flag
  // doesn't remove it from the CommandLine::ForCurrentProcess().
  void OnNewProfileManagementFlagChanged(bool new_flag_status);

  // KeyedService implementation.
  virtual void Shutdown() OVERRIDE;

  // Add or remove observers for the merge session notification.
  void AddMergeSessionObserver(MergeSessionHelper::Observer* observer);
  void RemoveMergeSessionObserver(MergeSessionHelper::Observer* observer);

  ProfileOAuth2TokenService* token_service() { return token_service_; }
  SigninClient* client() { return client_; }

 protected:
  // Used during GetAccountsFromCookie.
  // Stores a callback for the next action to perform.
  typedef base::Callback<
      void(const GoogleServiceAuthError& error,
           const std::vector<std::pair<std::string, bool> >&)>
      GetAccountsFromCookieCallback;

  virtual void GetAccountsFromCookie(GetAccountsFromCookieCallback callback);

 private:
  // An std::set<> for use with email addresses that uses
  // gaia::CanonicalizeEmail() during comparisons.
  // TODO(rogerta): this is a workaround for the fact that SigninManager and
  // SigninOAuthHelper use the gaia "email" property when adding accounts to
  // the token service, whereas gaia::ParseListAccountsData() returns email
  // addresses that have been passed through gaia::CanonicalizeEmail().  These
  // two types of email addresses are not directly comparable.
  class EmailLessFunc : public std::less<std::string> {
   public:
    bool operator()(const std::string& s1, const std::string& s2) const;
  };
  typedef std::set<std::string, EmailLessFunc> EmailSet;

  class RefreshTokenFetcher;
  class UserIdFetcher;

  bool IsRegisteredWithTokenService() const {
    return registered_with_token_service_;
  }

  bool AreGaiaAccountsSet() const { return are_gaia_accounts_set_; }

  bool AreAllRefreshTokensChecked() const;

  const std::vector<std::pair<std::string, bool> >& GetGaiaAccountsForTesting()
      const {
    return gaia_accounts_;
  }

  const EmailSet& GetValidChromeAccountsForTesting() const {
    return valid_chrome_accounts_;
  }

  const EmailSet& GetInvalidChromeAccountsForTesting() const {
    return invalid_chrome_accounts_;
  }


  friend class AccountReconcilorTest;
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorTest, SigninManagerRegistration);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorTest, Reauth);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorTest, ProfileAlreadyConnected);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorTest, GetAccountsFromCookieSuccess);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorTest, GetAccountsFromCookieFailure);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorTest, ValidateAccountsFromTokens);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorTest,
                           ValidateAccountsFromTokensFailedUserInfo);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorTest,
                           ValidateAccountsFromTokensFailedTokenRequest);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorTest, StartReconcileNoop);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorTest, StartReconcileNoopWithDots);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorTest, StartReconcileNoopMultiple);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorTest, StartReconcileAddToCookie);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorTest,
                           StartReconcileAddToCookieTwice);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorTest, StartReconcileAddToChrome);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorTest, StartReconcileBadPrimary);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorTest, StartReconcileOnlyOnce);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorTest,
                           StartReconcileWithSessionInfoExpiredDefault);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorTest,
                           MergeSessionCompletedWithBogusAccount);

  // Register and unregister with dependent services.
  void RegisterForCookieChanges();
  void UnregisterForCookieChanges();
  void RegisterWithSigninManager();
  void UnregisterWithSigninManager();
  void RegisterWithTokenService();
  void UnregisterWithTokenService();

  bool IsProfileConnected();

  void DeleteFetchers();

  // All actions with side effects.  Virtual so that they can be overridden
  // in tests.
  virtual void PerformMergeAction(const std::string& account_id);
  virtual void PerformAddToChromeAction(
      const std::string& account_id,
      int session_index,
      const std::string& signin_scoped_device_id);
  virtual void PerformLogoutAllAccountsAction();
  virtual void PerformAddAccountToTokenService(
      const std::string& account_id,
      const std::string& refresh_token);

  // Used to remove an account from chrome and the cookie jar.
  virtual void PerformStartRemoveAction(const std::string& account_id);
  virtual void PerformFinishRemoveAction(
      const std::string& account_id,
      const GoogleServiceAuthError& error,
      const std::vector<std::pair<std::string, bool> >& accounts);

  // Used during periodic reconciliation.
  void StartReconcile();
  void FinishReconcile();
  void AbortReconcile();
  void CalculateIfReconcileIsDone();
  void ScheduleStartReconcileIfChromeAccountsChanged();
  void HandleSuccessfulAccountIdCheck(const std::string& account_id);
  void HandleFailedAccountIdCheck(const std::string& account_id);
  void HandleRefreshTokenFetched(const std::string& account_id,
                                 const std::string& refresh_token);

  void ContinueReconcileActionAfterGetGaiaAccounts(
      const GoogleServiceAuthError& error,
      const std::vector<std::pair<std::string, bool> >& accounts);
  void ValidateAccountsFromTokenService();
  // Note internally that this |account_id| is added to the cookie jar.
  bool MarkAccountAsAddedToCookie(const std::string& account_id);
  // Note internally that this |account_id| is added to the token service.
  void MarkAccountAsAddedToChrome(const std::string& account_id);

  void OnCookieChanged(const net::CanonicalCookie* cookie);

  // Overriden from GaiaAuthConsumer.
  virtual void OnListAccountsSuccess(const std::string& data) OVERRIDE;
  virtual void OnListAccountsFailure(const GoogleServiceAuthError& error)
      OVERRIDE;

  // Overriden from MergeSessionHelper::Observer.
  virtual void MergeSessionCompleted(const std::string& account_id,
                                     const GoogleServiceAuthError& error)
      OVERRIDE;

  // Overriden from OAuth2TokenService::Consumer.
  virtual void OnGetTokenSuccess(const OAuth2TokenService::Request* request,
                                 const std::string& access_token,
                                 const base::Time& expiration_time) OVERRIDE;
  virtual void OnGetTokenFailure(const OAuth2TokenService::Request* request,
                                 const GoogleServiceAuthError& error) OVERRIDE;

  // Overriden from OAuth2TokenService::Observer.
  virtual void OnRefreshTokenRevoked(const std::string& account_id) OVERRIDE;
  virtual void OnEndBatchChanges() OVERRIDE;

  // Overriden from SigninManagerBase::Observer.
  virtual void GoogleSigninSucceeded(const std::string& username,
                                     const std::string& password) OVERRIDE;
  virtual void GoogleSignedOut(const std::string& username) OVERRIDE;

  void MayBeDoNextListAccounts();

  // The ProfileOAuth2TokenService associated with this reconcilor.
  ProfileOAuth2TokenService* token_service_;

  // The SigninManager associated with this reconcilor.
  SigninManagerBase* signin_manager_;

  // The SigninClient associated with this reconcilor.
  SigninClient* client_;

  MergeSessionHelper merge_session_helper_;
  scoped_ptr<GaiaAuthFetcher> gaia_fetcher_;
  bool registered_with_token_service_;

  // True while the reconcilor is busy checking or managing the accounts in
  // this profile.
  bool is_reconcile_started_;

  // True iff this is the first time the reconcilor is executing.
  bool first_execution_;

  // Used during reconcile action.
  // These members are used used to validate the gaia cookie.  |gaia_accounts_|
  // holds the state of google accounts in the gaia cookie.  Each element is
  // a pair that holds the email address of the account and a boolean that
  // indicates whether the account is valid or not.  The accounts in the vector
  // are ordered the in same way as the gaia cookie.
  bool are_gaia_accounts_set_;
  std::vector<std::pair<std::string, bool> > gaia_accounts_;

  // Used during reconcile action.
  // These members are used to validate the tokens in OAuth2TokenService.
  std::string primary_account_;
  std::vector<std::string> chrome_accounts_;
  scoped_ptr<OAuth2TokenService::Request>* requests_;
  ScopedVector<UserIdFetcher> user_id_fetchers_;
  ScopedVector<SigninOAuthHelper> refresh_token_fetchers_;
  EmailSet valid_chrome_accounts_;
  EmailSet invalid_chrome_accounts_;
  std::vector<std::string> add_to_cookie_;
  std::vector<std::pair<std::string, int> > add_to_chrome_;

  std::deque<GetAccountsFromCookieCallback> get_gaia_accounts_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(AccountReconcilor);
};

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_RECONCILOR_H_
