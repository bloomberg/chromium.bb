// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_SIGNIN_CLIENT_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_SIGNIN_CLIENT_H_

#include "base/callback.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/core/browser/webdata/token_web_data.h"

class PrefService;
class SigninManagerBase;
class TokenWebData;

namespace net {
class CanonicalCookie;
class URLRequestContextGetter;
}

#if defined(OS_IOS)
namespace ios {
// TODO(msarda): http://crbug.com/358544 Remove this iOS specific code from the
// core SigninClient.
class ProfileOAuth2TokenServiceIOSProvider;
}
#endif

// An interface that needs to be supplied to the Signin component by its
// embedder.
class SigninClient : public KeyedService {
 public:
  typedef base::Callback<void(const net::CanonicalCookie* cookie)>
      CookieChangedCallback;

  virtual ~SigninClient() {}

  // Gets the preferences associated with the client.
  virtual PrefService* GetPrefs() = 0;

  // Gets the TokenWebData instance associated with the client.
  virtual scoped_refptr<TokenWebData> GetDatabase() = 0;

  // Returns whether it is possible to revoke credentials.
  virtual bool CanRevokeCredentials() = 0;

  // Returns device id that is scoped to single signin. This device id will be
  // regenerated if user signs out and signs back in.
  // When refresh token is requested for this user it will be annotated with
  // this device id.
  virtual std::string GetSigninScopedDeviceId() = 0;

  // Clears signin scoped device id. This happens when user signs out or about
  // to sign in.
  virtual void ClearSigninScopedDeviceId() = 0;

  // Returns the URL request context information associated with the client.
  virtual net::URLRequestContextGetter* GetURLRequestContext() = 0;

  // Returns whether the user's credentials should be merged into the cookie
  // jar on signin completion.
  virtual bool ShouldMergeSigninCredentialsIntoCookieJar() = 0;

  // Returns a string containing the version info of the product in which the
  // Signin component is being used.
  virtual std::string GetProductVersion() = 0;

  // Sets the callback that should be called when a cookie changes. The
  // callback will be called only if it is not empty.
  // TODO(blundell): Eliminate this interface in favor of having core signin
  // code observe cookie changes once //chrome/browser/net has been
  // componentized.
  virtual void SetCookieChangedCallback(
      const CookieChangedCallback& callback) = 0;

  // Called when Google signin has succeeded.
  virtual void GoogleSigninSucceeded(const std::string& username,
                                     const std::string& password) {}

  virtual void SetSigninProcess(int host_id) = 0;
  virtual void ClearSigninProcess() = 0;
  virtual bool IsSigninProcess(int host_id) const = 0;
  virtual bool HasSigninProcess() const = 0;


#if defined(OS_IOS)
  // TODO(msarda): http://crbug.com/358544 Remove this iOS specific code from
  // the core SigninClient.
  virtual ios::ProfileOAuth2TokenServiceIOSProvider* GetIOSProvider() = 0;
#endif
};

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_SIGNIN_CLIENT_H_
