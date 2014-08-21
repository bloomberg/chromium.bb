// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_IOS_BROWSER_PROFILE_OAUTH2_TOKEN_SERVICE_IOS_H_
#define COMPONENTS_SIGNIN_IOS_BROWSER_PROFILE_OAUTH2_TOKEN_SERVICE_IOS_H_

#include <string>

#include "base/threading/thread_checker.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"

class OAuth2AccessTokenFetcher;

namespace ios{
class ProfileOAuth2TokenServiceIOSProvider;
}

// A specialization of ProfileOAuth2TokenService that will be returned by
// ProfileOAuth2TokenServiceFactory for OS_IOS when iOS authentication service
// is used to lookup OAuth2 tokens.
//
// See |ProfileOAuth2TokenService| for usage details.
//
// Note: Requests should be started from the UI thread. To start a
// request from aother thread, please use OAuth2TokenServiceRequest.
class ProfileOAuth2TokenServiceIOS : public ProfileOAuth2TokenService {
 public:
  // KeyedService
  virtual void Shutdown() OVERRIDE;

  // OAuth2TokenService
  virtual bool RefreshTokenIsAvailable(
      const std::string& account_id) const OVERRIDE;

  virtual void InvalidateOAuth2Token(const std::string& account_id,
                                     const std::string& client_id,
                                     const ScopeSet& scopes,
                                     const std::string& access_token) OVERRIDE;

  // ProfileOAuth2TokenService
  virtual void Initialize(SigninClient* client) OVERRIDE;
  virtual void LoadCredentials(const std::string& primary_account_id) OVERRIDE;
  virtual std::vector<std::string> GetAccounts() OVERRIDE;
  virtual void UpdateAuthError(const std::string& account_id,
                               const GoogleServiceAuthError& error) OVERRIDE;

  // This method should not be called when using shared authentication.
  virtual void UpdateCredentials(const std::string& account_id,
                                 const std::string& refresh_token) OVERRIDE;

  // Removes all credentials from this instance of |ProfileOAuth2TokenService|,
  // however, it does not revoke the identities from the device.
  // Subsequent calls to |RefreshTokenIsAvailable| will return |false|.
  virtual void RevokeAllCredentials() OVERRIDE;

  // Reloads accounts from the provider. Fires |OnRefreshTokenAvailable| for
  // each new account. Fires |OnRefreshTokenRevoked| for each account that was
  // removed.
  void ReloadCredentials();

 protected:
  friend class ProfileOAuth2TokenServiceFactory;
  friend class ProfileOAuth2TokenServiceIOSTest;

  ProfileOAuth2TokenServiceIOS();
  virtual ~ProfileOAuth2TokenServiceIOS();

  virtual OAuth2AccessTokenFetcher* CreateAccessTokenFetcher(
      const std::string& account_id,
      net::URLRequestContextGetter* getter,
      OAuth2AccessTokenConsumer* consumer) OVERRIDE;

  // Protected and virtual to be overriden by fake for testing.

  // Adds |account_id| to |accounts_| if it does not exist or udpates
  // the auth error state of |account_id| if it exists. Fires
  // |OnRefreshTokenAvailable| if the account info is updated.
  virtual void AddOrUpdateAccount(const std::string& account_id);

  // Removes |account_id| from |accounts_|. Fires |OnRefreshTokenRevoked|
  // if the account info is removed.
  virtual void RemoveAccount(const std::string& account_id);

 private:
  class AccountInfo : public SigninErrorController::AuthStatusProvider {
   public:
    AccountInfo(ProfileOAuth2TokenService* token_service,
                const std::string& account_id);
    virtual ~AccountInfo();

    void SetLastAuthError(const GoogleServiceAuthError& error);

    // SigninErrorController::AuthStatusProvider implementation.
    virtual std::string GetAccountId() const OVERRIDE;
    virtual std::string GetUsername() const OVERRIDE;
    virtual GoogleServiceAuthError GetAuthStatus() const OVERRIDE;

   private:
    ProfileOAuth2TokenService* token_service_;
    std::string account_id_;
    GoogleServiceAuthError last_auth_error_;

    DISALLOW_COPY_AND_ASSIGN(AccountInfo);
  };

  // Maps the |account_id| of accounts known to ProfileOAuth2TokenService
  // to information about the account.
  typedef std::map<std::string, linked_ptr<AccountInfo> > AccountInfoMap;

  // Returns the iOS provider;
  ios::ProfileOAuth2TokenServiceIOSProvider* GetProvider();

  // Info about the existing accounts.
  AccountInfoMap accounts_;

  // Calls to this class are expected to be made from the browser UI thread.
  // The purpose of this checker is to detect access to
  // ProfileOAuth2TokenService from multiple threads in upstream code.
  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(ProfileOAuth2TokenServiceIOS);
};

#endif  // COMPONENTS_SIGNIN_IOS_BROWSER_PROFILE_OAUTH2_TOKEN_SERVICE_IOS_H_
