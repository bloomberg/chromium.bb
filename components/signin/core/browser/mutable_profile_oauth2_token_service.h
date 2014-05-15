// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_MUTABLE_PROFILE_OAUTH2_TOKEN_SERVICE_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_MUTABLE_PROFILE_OAUTH2_TOKEN_SERVICE_H_

#include "base/memory/scoped_vector.h"
#include "base/threading/thread_checker.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/webdata/common/web_data_service_base.h"
#include "components/webdata/common/web_data_service_consumer.h"

// A specialization of ProfileOAuth2TokenService that can can mutate its OAuth2
// tokens.
//
// Note: This class is just a placeholder for now. Methods used to mutate
// the tokens are currently being migrated from ProfileOAuth2TokenService.
class MutableProfileOAuth2TokenService : public ProfileOAuth2TokenService,
                                         public WebDataServiceConsumer  {
 public:
  // ProfileOAuth2TokenService overrides.
  virtual void Shutdown() OVERRIDE;
  virtual std::vector<std::string> GetAccounts() OVERRIDE;

  // The below three methods should be called only on the thread on which this
  // object was created.
  virtual void LoadCredentials(const std::string& primary_account_id) OVERRIDE;
  virtual void UpdateCredentials(const std::string& account_id,
                                 const std::string& refresh_token) OVERRIDE;
  virtual void RevokeAllCredentials() OVERRIDE;
  virtual bool RefreshTokenIsAvailable(const std::string& account_id) const
      OVERRIDE;

  // Revokes credentials related to |account_id|.
  void RevokeCredentials(const std::string& account_id);

 protected:
  class AccountInfo : public SigninErrorController::AuthStatusProvider {
   public:
    AccountInfo(ProfileOAuth2TokenService* token_service,
                const std::string& account_id,
                const std::string& refresh_token);
    virtual ~AccountInfo();

    const std::string& refresh_token() const { return refresh_token_; }
    void set_refresh_token(const std::string& token) {
      refresh_token_ = token;
    }

    void SetLastAuthError(const GoogleServiceAuthError& error);

    // SigninErrorController::AuthStatusProvider implementation.
    virtual std::string GetAccountId() const OVERRIDE;
    virtual std::string GetUsername() const OVERRIDE;
    virtual GoogleServiceAuthError GetAuthStatus() const OVERRIDE;

   private:
    ProfileOAuth2TokenService* token_service_;
    std::string account_id_;
    std::string refresh_token_;
    GoogleServiceAuthError last_auth_error_;

    DISALLOW_COPY_AND_ASSIGN(AccountInfo);
  };

  // Maps the |account_id| of accounts known to ProfileOAuth2TokenService
  // to information about the account.
  typedef std::map<std::string, linked_ptr<AccountInfo> > AccountInfoMap;

  friend class ProfileOAuth2TokenServiceFactory;
  friend class MutableProfileOAuth2TokenServiceTest;

  MutableProfileOAuth2TokenService();
  virtual ~MutableProfileOAuth2TokenService();

  // OAuth2TokenService implementation.
  virtual OAuth2AccessTokenFetcher* CreateAccessTokenFetcher(
      const std::string& account_id,
      net::URLRequestContextGetter* getter,
      OAuth2AccessTokenConsumer* consumer) OVERRIDE;
  virtual net::URLRequestContextGetter* GetRequestContext() OVERRIDE;

  // Updates the internal cache of the result from the most-recently-completed
  // auth request (used for reporting errors to the user).
  virtual void UpdateAuthError(const std::string& account_id,
                               const GoogleServiceAuthError& error) OVERRIDE;

  virtual std::string GetRefreshToken(const std::string& account_id) const;

  AccountInfoMap& refresh_tokens() { return refresh_tokens_; }

 private:
  class RevokeServerRefreshToken;

  FRIEND_TEST_ALL_PREFIXES(MutableProfileOAuth2TokenServiceTest,
                           TokenServiceUpdateClearsCache);
  FRIEND_TEST_ALL_PREFIXES(MutableProfileOAuth2TokenServiceTest,
                           PersistenceDBUpgrade);
  FRIEND_TEST_ALL_PREFIXES(MutableProfileOAuth2TokenServiceTest,
                           PersistenceLoadCredentials);

  // WebDataServiceConsumer implementation:
  virtual void OnWebDataServiceRequestDone(
      WebDataServiceBase::Handle handle,
      const WDTypedResult* result) OVERRIDE;

  // Loads credentials into in memory stucture.
  void LoadAllCredentialsIntoMemory(
      const std::map<std::string, std::string>& db_tokens);

  // Persists credentials for |account_id|. Enables overriding for
  // testing purposes, or other cases, when accessing the DB is not desired.
  void PersistCredentials(const std::string& account_id,
                          const std::string& refresh_token);

  // Clears credentials persisted for |account_id|. Enables overriding for
  // testing purposes, or other cases, when accessing the DB is not desired.
  void ClearPersistedCredentials(const std::string& account_id);

  // Revokes the refresh token on the server.
  void RevokeCredentialsOnServer(const std::string& refresh_token);

  // Cancels any outstanding fetch for tokens from the web database.
  void CancelWebTokenFetch();

  // In memory refresh token store mapping account_id to refresh_token.
  AccountInfoMap refresh_tokens_;

  // Handle to the request reading tokens from database.
  WebDataServiceBase::Handle web_data_service_request_;

  // The primary account id of this service's profile during the loading of
  // credentials.  This member is empty otherwise.
  std::string loading_primary_account_id_;

  ScopedVector<RevokeServerRefreshToken> server_revokes_;

  // Used to verify that certain methods are called only on the thread on which
  // this instance was created.
  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(MutableProfileOAuth2TokenService);
};

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_MUTABLE_PROFILE_OAUTH2_TOKEN_SERVICE_H_
