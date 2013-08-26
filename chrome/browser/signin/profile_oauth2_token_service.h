// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_PROFILE_OAUTH2_TOKEN_SERVICE_H_
#define CHROME_BROWSER_SIGNIN_PROFILE_OAUTH2_TOKEN_SERVICE_H_

#include <string>

#include "base/gtest_prod_util.h"
#include "chrome/browser/signin/signin_global_error.h"
#include "components/browser_context_keyed_service/browser_context_keyed_service.h"
#include "components/webdata/common/web_data_service_consumer.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "google_apis/gaia/oauth2_token_service.h"

namespace net {
class URLRequestContextGetter;
}

class GoogleServiceAuthError;
class Profile;
class TokenService;
class SigninGlobalError;

// ProfileOAuth2TokenService is a BrowserContextKeyedService that retrieves
// OAuth2 access tokens for a given set of scopes using the OAuth2 login
// refresh token maintained by TokenService.
//
// See |OAuth2TokenService| for usage details.
//
// Note: after StartRequest returns, in-flight requests will continue
// even if the TokenService refresh token that was used to initiate
// the request changes or is cleared.  When the request completes,
// Consumer::OnGetTokenSuccess will be invoked, but the access token
// won't be cached.
//
// Note: requests should be started from the UI thread. To start a
// request from other thread, please use ProfileOAuth2TokenServiceRequest.
class ProfileOAuth2TokenService : public OAuth2TokenService,
                                  public content::NotificationObserver,
                                  public SigninGlobalError::AuthStatusProvider,
                                  public BrowserContextKeyedService,
                                  public WebDataServiceConsumer {
 public:
  // content::NotificationObserver listening for TokenService updates.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Initializes this token service with the profile.
  virtual void Initialize(Profile* profile);

  // BrowserContextKeyedService implementation.
  virtual void Shutdown() OVERRIDE;

  // SigninGlobalError::AuthStatusProvider implementation.
  virtual GoogleServiceAuthError GetAuthStatus() const OVERRIDE;

  // Takes injected TokenService for testing.
  bool ShouldCacheForRefreshToken(TokenService *token_service,
                                  const std::string& refresh_token);

  // Updates a |refresh_token| for an |account_id|. Credentials are persisted,
  // and avialable through |LoadCredentials| after service is restarted.
  void UpdateCredentials(const std::string& account_id,
                         const std::string& refresh_token);

  // Revokes credentials related to |account_id|.
  void RevokeCredentials(const std::string& account_id);

  // Revokes all credentials handled by the object.
  void RevokeAllCredentials();

  SigninGlobalError* signin_global_error() {
    return signin_global_error_.get();
  }

  const SigninGlobalError* signin_global_error() const {
    return signin_global_error_.get();
  }

  Profile* profile() const { return profile_; }

 protected:
  friend class ProfileOAuth2TokenServiceFactory;
  ProfileOAuth2TokenService();
  virtual ~ProfileOAuth2TokenService();

  // OAuth2TokenService overrides.
  virtual std::string GetRefreshToken() OVERRIDE;

  // OAuth2TokenService implementation.
  virtual net::URLRequestContextGetter* GetRequestContext() OVERRIDE;

  // Updates the internal cache of the result from the most-recently-completed
  // auth request (used for reporting errors to the user).
  virtual void UpdateAuthError(const GoogleServiceAuthError& error) OVERRIDE;

  // Overridden to not cache tokens if the TokenService refresh token
  // changes while a token fetch is in-flight.  If the user logs out and
  // logs back in with a different account, then any in-flight token
  // fetches will be for the old account's refresh token.  Therefore
  // when they come back, they shouldn't be cached.
  virtual void RegisterCacheEntry(const std::string& refresh_token,
                                  const ScopeSet& scopes,
                                  const std::string& access_token,
                                  const base::Time& expiration_date) OVERRIDE;

 private:
  FRIEND_TEST_ALL_PREFIXES(ProfileOAuth2TokenServiceTest,
                           StaleRefreshTokensNotCached);
  FRIEND_TEST_ALL_PREFIXES(ProfileOAuth2TokenServiceTest,
                           TokenServiceUpdateClearsCache);
  FRIEND_TEST_ALL_PREFIXES(ProfileOAuth2TokenServiceTest,
                           PersistenceDBUpgrade);
  FRIEND_TEST_ALL_PREFIXES(ProfileOAuth2TokenServiceTest,
                           PersistenceLoadCredentials);

  // WebDataServiceConsumer implementation:
  virtual void OnWebDataServiceRequestDone(
      WebDataServiceBase::Handle handle,
      const WDTypedResult* result) OVERRIDE;

  // Loads credentials from a backing persistent store to make them available
  // after service is used between profile restarts.
  void LoadCredentials();

  // Loads credentials into in memory stucture.
  void LoadAllCredentialsIntoMemory(
      const std::map<std::string, std::string>& db_tokens);

  // Loads a single pair of |account_id|, |refresh_token| into memory.
  void LoadCredentialsIntoMemory(const std::string& account_id,
                                 const std::string& refresh_token);

  // The profile with which this instance was initialized, or NULL.
  Profile* profile_;

  // Handle to the request reading tokens from database.
  WebDataServiceBase::Handle web_data_service_request_;

  // In memory refresh token store mapping account_id to refresh_token.
  std::map<std::string, std::string> refresh_tokens_;

  // Used to show auth errors in the wrench menu. The SigninGlobalError is
  // different than most GlobalErrors in that its lifetime is controlled by
  // ProfileOAuth2TokenService (so we can expose a reference for use in the
  // wrench menu).
  scoped_ptr<SigninGlobalError> signin_global_error_;

  // The auth status from the most-recently-completed request.
  GoogleServiceAuthError last_auth_error_;

  // Registrar for notifications from the TokenService.
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(ProfileOAuth2TokenService);
};

#endif  // CHROME_BROWSER_SIGNIN_PROFILE_OAUTH2_TOKEN_SERVICE_H_
