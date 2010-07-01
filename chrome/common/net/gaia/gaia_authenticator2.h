// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_NET_GAIA_GAIA_AUTHENTICATOR2_H_
#define CHROME_COMMON_NET_GAIA_GAIA_AUTHENTICATOR2_H_

#include <string>

#include "base/gtest_prod_util.h"
#include "chrome/common/net/url_fetcher.h"
#include "googleurl/src/gurl.h"

// Authenticate a user against the Google Accounts ClientLogin API
// with various capabilities and return results to a GaiaAuthConsumer.
//
// In the future, we will also issue auth tokens from this class.
// This class should be used on a single thread, but it can be whichever thread
// that you like.

class GaiaAuthConsumer;
class GaiaAuthenticator2Test;

class GaiaAuthenticator2 : public URLFetcher::Delegate {
 public:
  // Constants to use in the ClientLogin request POST body.
  static const char kChromeOSSource[];
  static const char kContactsService[];

  // The URLs for different calls in the Google Accounts programmatic login API.
  static const char kClientLoginUrl[];
  static const char kIssueAuthTokenUrl[];
  static const char kTokenAuthUrl[];


  // Magic string indicating that, while a second factor is still
  // needed to complete authentication, the user provided the right password.
  static const char kSecondFactor[];

  // This will later be hidden behind an auth service which caches
  // tokens.
  GaiaAuthenticator2(GaiaAuthConsumer* consumer,
                     const std::string& source,
                     URLRequestContextGetter* getter);
  virtual ~GaiaAuthenticator2();

  // GaiaAuthConsumer will be called on the original thread
  // after results come back. This class is thread agnostic.
  void StartClientLogin(const std::string& username,
                        const std::string& password,
                        const char* const service,
                        const std::string& login_token,
                        const std::string& login_captcha);

  // Implementation of URLFetcher::Delegate
  void OnURLFetchComplete(const URLFetcher* source,
                          const GURL& url,
                          const URLRequestStatus& status,
                          int response_code,
                          const ResponseCookies& cookies,
                          const std::string& data);

  // StartClientLogin been called && results not back yet?
  bool HasPendingFetch();

  // Stop any URL fetches in progress.
  void CancelRequest();

 private:
  // ClientLogin body constants that don't change
  static const char kCookiePersistence[];
  static const char kAccountType[];

  // The format of the POST body for ClientLogin.
  static const char kClientLoginFormat[];
  // The format of said POST body when CAPTCHA token & answer are specified.
  static const char kClientLoginCaptchaFormat[];

  // Process the results of a ClientLogin fetch.
  void OnClientLoginFetched(const std::string& data,
                            const URLRequestStatus& status,
                            int response_code);

  // Tokenize the results of a ClientLogin fetch.
  static void ParseClientLoginResponse(const std::string& data,
                                       std::string* sid,
                                       std::string* lsid,
                                       std::string* token);

  // Is this a special case Gaia error for TwoFactor auth?
  static bool IsSecondFactorSuccess(const std::string& alleged_error);

  // Given parameters, create a ClientLogin request body.
  static std::string GenerateRequestBody(const std::string& username,
                                         const std::string& password,
                                         const std::string& source,
                                         const char* service,
                                         const std::string& login_token,
                                         const std::string& login_captcha);

  // Create a fetcher useable for making a ClientLogin request.
  static URLFetcher* CreateClientLoginFetcher(URLRequestContextGetter* getter,
                                              const std::string& body,
                                              const GURL& client_login_gurl_,
                                              URLFetcher::Delegate* delegate);

  GaiaAuthConsumer* const consumer_;
  scoped_ptr<URLFetcher> fetcher_;
  URLRequestContextGetter* const getter_;
  std::string source_;
  const GURL client_login_gurl_;
  std::string request_body_;
  bool fetch_pending_;

  friend class GaiaAuthenticator2Test;
  FRIEND_TEST_ALL_PREFIXES(GaiaAuthenticator2Test, LoginNetFailure);
  FRIEND_TEST_ALL_PREFIXES(GaiaAuthenticator2Test, CheckNormalErrorCode);
  FRIEND_TEST_ALL_PREFIXES(GaiaAuthenticator2Test, CheckTwoFactorResponse);

  DISALLOW_COPY_AND_ASSIGN(GaiaAuthenticator2);
};

#endif  // CHROME_COMMON_NET_GAIA_GAIA_AUTHENTICATOR2_H_
