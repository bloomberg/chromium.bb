// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_ANDROID_PROFILE_OAUTH2_TOKEN_SERVICE_H_
#define CHROME_BROWSER_SIGNIN_ANDROID_PROFILE_OAUTH2_TOKEN_SERVICE_H_

#include <jni.h>
#include <string>

#include "base/android/jni_helper.h"
#include "base/callback.h"
#include "base/time/time.h"
#include "chrome/browser/signin/profile_oauth2_token_service.h"
#include "google_apis/gaia/google_service_auth_error.h"

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

  // Callback from FetchOAuth2Token.
  // Arguments:
  // - the error, or NONE if the token fetch was successful.
  // - the OAuth2 access token.
  // - the expiry time of the token (may be null, indicating that the expiry
  //   time is unknown.
  typedef base::Callback<void(
      const GoogleServiceAuthError&, const std::string&, const base::Time&)>
          FetchOAuth2TokenCallback;

  // Start the OAuth2 access token for the given scopes using
  // ProfileSyncServiceAndroid.
  virtual scoped_ptr<OAuth2TokenService::Request> StartRequest(
      const OAuth2TokenService::ScopeSet& scopes,
      OAuth2TokenService::Consumer* consumer) OVERRIDE;

  virtual bool RefreshTokenIsAvailable() OVERRIDE;
  virtual void InvalidateToken(const ScopeSet& scopes,
                               const std::string& invalid_token) OVERRIDE;

  // Registers the AndroidProfileOAuth2TokenService's native methods through
  // JNI.
  static bool Register(JNIEnv* env);

 protected:
  friend class ProfileOAuth2TokenServiceFactory;
  explicit AndroidProfileOAuth2TokenService(
      net::URLRequestContextGetter* getter);
  virtual ~AndroidProfileOAuth2TokenService();

  void FetchOAuth2Token(const std::string& scope,
                        const FetchOAuth2TokenCallback& callback);

 private:
  DISALLOW_COPY_AND_ASSIGN(AndroidProfileOAuth2TokenService);
};

#endif  // CHROME_BROWSER_SIGNIN_ANDROID_PROFILE_OAUTH2_TOKEN_SERVICE_H_
