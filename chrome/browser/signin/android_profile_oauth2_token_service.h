// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_ANDROID_PROFILE_OAUTH2_TOKEN_SERVICE_H_
#define CHROME_BROWSER_SIGNIN_ANDROID_PROFILE_OAUTH2_TOKEN_SERVICE_H_

#include <jni.h>
#include <string>

#include "base/android/jni_helper.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/signin/profile_oauth2_token_service.h"
#include "google_apis/gaia/google_service_auth_error.h"

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
  // StartRequest() fetches a token for the currently signed-in account; this
  // version uses the account corresponding to |username|. This allows fetching
  // tokens before a user is signed-in (e.g. during the sign-in flow).
  scoped_ptr<OAuth2TokenService::Request> StartRequestForUsername(
      const std::string& username,
      const OAuth2TokenService::ScopeSet& scopes,
      OAuth2TokenService::Consumer* consumer);

  virtual bool RefreshTokenIsAvailable() OVERRIDE;
  virtual void InvalidateToken(const ScopeSet& scopes,
                               const std::string& invalid_token) OVERRIDE;

  // Registers the AndroidProfileOAuth2TokenService's native methods through
  // JNI.
  static bool Register(JNIEnv* env);

 protected:
  friend class ProfileOAuth2TokenServiceFactory;
  AndroidProfileOAuth2TokenService();
  virtual ~AndroidProfileOAuth2TokenService();

  // Overridden from OAuth2TokenService to intercept token fetch requests and
  // redirect them to the Account Manager.
  virtual void FetchOAuth2Token(RequestImpl* request,
                                net::URLRequestContextGetter* getter,
                                const std::string& client_id,
                                const std::string& client_secret,
                                const ScopeSet& scopes) OVERRIDE;

  // Low-level helper function used by both FetchOAuth2Token and
  // StartRequestForUsername to fetch tokens. virtual to enable mocks.
  virtual void FetchOAuth2TokenWithUsername(
      RequestImpl* request,
      const std::string& username,
      const ScopeSet& scope);

 private:
  DISALLOW_COPY_AND_ASSIGN(AndroidProfileOAuth2TokenService);
};

#endif  // CHROME_BROWSER_SIGNIN_ANDROID_PROFILE_OAUTH2_TOKEN_SERVICE_H_
