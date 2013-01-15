// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_OAUTH1_TOKEN_FETCHER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_OAUTH1_TOKEN_FETCHER_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/net/gaia/gaia_oauth_consumer.h"
#include "chrome/browser/net/gaia/gaia_oauth_fetcher.h"

namespace net {
class URLRequestContextGetter;
}

namespace chromeos {

// Given the authenticated credentials from the cookie jar, try to exchange
// fetch OAuth1 token and secret. Automatically retries until max retry count is
// reached.
class OAuth1TokenFetcher : public base::SupportsWeakPtr<OAuth1TokenFetcher>,
                           public GaiaOAuthConsumer {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}
    virtual void OnOAuth1AccessTokenAvailable(const std::string& token,
                                              const std::string& secret) = 0;
    virtual void OnOAuth1AccessTokenFetchFailed() = 0;
  };

  OAuth1TokenFetcher(OAuth1TokenFetcher::Delegate* delegate,
                     net::URLRequestContextGetter* auth_context_getter);
  virtual ~OAuth1TokenFetcher();

  void Start();

 private:
  // Decides how to proceed on GAIA response and other errors. If the error
  // looks temporary, retries token fetching until max retry count is reached.
  // If retry count runs out, or error condition is unrecoverable, returns
  // false.
  bool RetryOnError(const GoogleServiceAuthError& error);

  // GaiaOAuthConsumer implementation:
  virtual void OnGetOAuthTokenSuccess(const std::string& oauth_token) OVERRIDE;
  virtual void OnGetOAuthTokenFailure(
      const GoogleServiceAuthError& error) OVERRIDE;
  virtual void OnOAuthGetAccessTokenSuccess(const std::string& token,
                                            const std::string& secret) OVERRIDE;
  virtual void OnOAuthGetAccessTokenFailure(
      const GoogleServiceAuthError& error) OVERRIDE;

  OAuth1TokenFetcher::Delegate* delegate_;
  GaiaOAuthFetcher oauth_fetcher_;

  // The retry counter. Increment this only when failure happened.
  int retry_count_;

  DISALLOW_COPY_AND_ASSIGN(OAuth1TokenFetcher);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_OAUTH1_TOKEN_FETCHER_H_
