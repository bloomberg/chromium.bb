// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_ANDROID_PROFILE_OAUTH2_TOKEN_SERVICE_H_
#define CHROME_BROWSER_SIGNIN_ANDROID_PROFILE_OAUTH2_TOKEN_SERVICE_H_

#include <string>

#include "chrome/browser/signin/profile_oauth2_token_service.h"

namespace net {
class URLRequestContextGetter;
}

class TokenService;

// A specialization of ProfileOAuth2TokenService that will be returned by
// ProfileOAuth2TokenServiceFactory for OS_ANDROID.  This instance uses
// native Android features to lookup OAuth2 tokens.
//
// See |ProfileOAuth2TokenService| for usage details.
//
// Note: requests should be started from the UI thread. To start a
// request from other thread, please use ProfileOAuth2TokenServiceRequest.
class AndroidProfileOAuth2TokenService : public ProfileOAuth2TokenService {
 public:
  // Start the OAuth2 access token for the given scopes using
  // ProfileSyncServiceAndroid.
  virtual scoped_ptr<OAuth2TokenService::Request> StartRequest(
      const OAuth2TokenService::ScopeSet& scopes,
      OAuth2TokenService::Consumer* consumer) OVERRIDE;

  virtual void InvalidateToken(const ScopeSet& scopes,
                               const std::string& invalid_token) OVERRIDE;

 protected:
  friend class ProfileOAuth2TokenServiceFactory;
  explicit AndroidProfileOAuth2TokenService(
      net::URLRequestContextGetter* getter);
  virtual ~AndroidProfileOAuth2TokenService();

  // Takes injected TokenService for testing.
  bool ShouldCacheForRefreshToken(TokenService *token_service,
                                  const std::string& refresh_token);

 private:
  DISALLOW_COPY_AND_ASSIGN(AndroidProfileOAuth2TokenService);
};

#endif  // CHROME_BROWSER_SIGNIN_ANDROID_PROFILE_OAUTH2_TOKEN_SERVICE_H_
