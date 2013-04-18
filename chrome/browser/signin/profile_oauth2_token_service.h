// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_PROFILE_OAUTH2_TOKEN_SERVICE_H_
#define CHROME_BROWSER_SIGNIN_PROFILE_OAUTH2_TOKEN_SERVICE_H_

#include <string>

#include "base/gtest_prod_util.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "chrome/browser/signin/oauth2_token_service.h"
#include "chrome/browser/signin/signin_global_error.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

namespace net {
class URLRequestContextGetter;
}

class GoogleServiceAuthError;
class Profile;
class TokenService;

// ProfileOAuth2TokenService is a ProfileKeyedService that retrieves
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
                                  public ProfileKeyedService {
 public:
  // content::NotificationObserver listening for TokenService updates.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Initializes this token service with the profile.
  virtual void Initialize(Profile* profile);

  // ProfileKeyedService implementation.
  virtual void Shutdown() OVERRIDE;

  // SigninGlobalError::AuthStatusProvider implementation.
  virtual GoogleServiceAuthError GetAuthStatus() const OVERRIDE;

  // Takes injected TokenService for testing.
  bool ShouldCacheForRefreshToken(TokenService *token_service,
                                  const std::string& refresh_token);

 protected:
  friend class ProfileOAuth2TokenServiceFactory;
  explicit ProfileOAuth2TokenService(net::URLRequestContextGetter* getter);
  virtual ~ProfileOAuth2TokenService();

  virtual std::string GetRefreshToken() OVERRIDE;

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

  // The profile with which this instance was initialized, or NULL.
  Profile* profile_;

  // The auth status from the most-recently-completed request.
  GoogleServiceAuthError last_auth_error_;

  // Registrar for notifications from the TokenService.
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(ProfileOAuth2TokenService);
};

#endif  // CHROME_BROWSER_SIGNIN_PROFILE_OAUTH2_TOKEN_SERVICE_H_
