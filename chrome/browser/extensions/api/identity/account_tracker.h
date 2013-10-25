// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_IDENTITY_ACCOUNT_TRACKER_H_
#define CHROME_BROWSER_EXTENSIONS_API_IDENTITY_ACCOUNT_TRACKER_H_

#include <map>
#include <string>

#include "base/observer_list.h"
#include "chrome/browser/signin/signin_global_error.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_source.h"
#include "google_apis/gaia/gaia_oauth_client.h"
#include "google_apis/gaia/oauth2_token_service.h"

class GoogleServiceAuthError;
class Profile;

namespace extensions {

struct AccountIds {
  std::string account_key;  // The account ID used by OAuth2TokenService.
  std::string gaia;
  std::string email;
};

class AccountIdFetcher;

// The AccountTracker keeps track of what accounts exist on the
// profile and the state of their credentials. The tracker fetches the
// gaia ID of each account it knows about.
//
// The AccountTracker maintains these invariants:
// 1. Events are only fired after the gaia ID has been fetched.
// 2. Add/Remove and SignIn/SignOut pairs are always generated in order.
// 3. SignIn follows Add, and there will be a SignOut between SignIn & Remove.
class AccountTracker : public OAuth2TokenService::Observer,
                       public content::NotificationObserver,
                       public SigninGlobalError::AuthStatusProvider {
 public:
  explicit AccountTracker(Profile* profile);
  virtual ~AccountTracker();

  class Observer {
   public:
    virtual void OnAccountAdded(const AccountIds& ids) = 0;
    virtual void OnAccountRemoved(const AccountIds& ids) = 0;
    virtual void OnAccountSignInChanged(const AccountIds& ids,
                                        bool is_signed_in) = 0;
  };

  void Shutdown();

  void ReportAuthError(const std::string& account_key,
                       const GoogleServiceAuthError& error);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // OAuth2TokenService::Observer implementation.
  virtual void OnRefreshTokenAvailable(const std::string& account_key) OVERRIDE;
  virtual void OnRefreshTokenRevoked(const std::string& account_key) OVERRIDE;

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  void OnUserInfoFetchSuccess(AccountIdFetcher* fetcher,
                              const std::string& gaia_id);
  void OnUserInfoFetchFailure(AccountIdFetcher* fetcher);

  // AuthStatusProvider implementation.
  virtual std::string GetAccountId() const OVERRIDE;
  virtual GoogleServiceAuthError GetAuthStatus() const OVERRIDE;

 private:
  struct AccountState {
    AccountIds ids;
    bool is_signed_in;
  };

  void NotifyAccountAdded(const AccountState& account);
  void NotifyAccountRemoved(const AccountState& account);
  void NotifySignInChanged(const AccountState& account);

  void ClearAuthError(const std::string& account_key);
  void UpdateSignInState(const std::string& account_key, bool is_signed_in);

  void StartTrackingAccount(const std::string& account_key);
  void StopTrackingAccount(const std::string& account_key);
  void StartFetchingUserInfo(const std::string& account_key);
  void DeleteFetcher(AccountIdFetcher* fetcher);

  Profile* profile_;
  std::map<std::string, AccountIdFetcher*> user_info_requests_;
  std::map<std::string, AccountState> accounts_;
  std::map<std::string, GoogleServiceAuthError> account_errors_;
  ObserverList<Observer> observer_list_;
  content::NotificationRegistrar registrar_;
};

class AccountIdFetcher : public OAuth2TokenService::Consumer,
                         public gaia::GaiaOAuthClient::Delegate {
 public:
  AccountIdFetcher(Profile* profile,
                   AccountTracker* tracker,
                   const std::string& account_key);
  virtual ~AccountIdFetcher();

  const std::string& account_key() { return account_key_; }

  void Start();

  // OAuth2TokenService::Consumer implementation.
  virtual void OnGetTokenSuccess(const OAuth2TokenService::Request* request,
                                 const std::string& access_token,
                                 const base::Time& expiration_time) OVERRIDE;
  virtual void OnGetTokenFailure(const OAuth2TokenService::Request* request,
                                 const GoogleServiceAuthError& error) OVERRIDE;

  // gaia::GaiaOAuthClient::Delegate implementation.
  virtual void OnGetUserIdResponse(const std::string& gaia_id) OVERRIDE;
  virtual void OnOAuthError() OVERRIDE;
  virtual void OnNetworkError(int response_code) OVERRIDE;

 private:
  Profile* profile_;
  AccountTracker* tracker_;
  const std::string account_key_;

  scoped_ptr<OAuth2TokenService::Request> login_token_request_;
  scoped_ptr<gaia::GaiaOAuthClient> gaia_oauth_client_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_IDENTITY_ACCOUNT_TRACKER_H_
