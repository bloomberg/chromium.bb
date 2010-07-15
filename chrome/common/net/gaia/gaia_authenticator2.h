// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_NET_GAIA_GAIA_AUTHENTICATOR2_H_
#define CHROME_COMMON_NET_GAIA_GAIA_AUTHENTICATOR2_H_

#include <string>

#include "base/gtest_prod_util.h"
#include "base/scoped_ptr.h"
#include "chrome/common/net/gaia/gaia_auth_consumer.h"
#include "chrome/common/net/url_fetcher.h"
#include "googleurl/src/gurl.h"

// Authenticate a user against the Google Accounts ClientLogin API
// with various capabilities and return results to a GaiaAuthConsumer.
//
// In the future, we will also issue auth tokens from this class.
// This class should be used on a single thread, but it can be whichever thread
// that you like.
//
// This class can handle one request at a time. To parallelize requests,
// create multiple GaiaAuthenticator2's.

class GaiaAuthenticator2Test;

class GaiaAuthenticator2 : public URLFetcher::Delegate {
 public:
  // Constants to use in the ClientLogin request POST body.
  static const char kChromeOSSource[];
  static const char kContactsService[];

  // The URLs for different calls in the Google Accounts programmatic login API.
  static const char kClientLoginUrl[];
  static const char kIssueAuthTokenUrl[];

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
  // You can't make more than request at a time.
  void StartClientLogin(const std::string& username,
                        const std::string& password,
                        const char* const service,
                        const std::string& login_token,
                        const std::string& login_captcha);

  // GaiaAuthConsumer will be called on the original thread
  // after results come back. This class is thread agnostic.
  // You can't make more than one request at a time.
  void StartIssueAuthToken(const std::string& sid,
                           const std::string& lsid,
                           const char* const service);

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
  // The format of the POST body for IssueAuthToken.
  static const char kIssueAuthTokenFormat[];

  // Process the results of a ClientLogin fetch.
  void OnClientLoginFetched(const std::string& data,
                            const URLRequestStatus& status,
                            int response_code);

  void OnIssueAuthTokenFetched(const std::string& data,
                               const URLRequestStatus& status,
                               int response_code);

  // Tokenize the results of a ClientLogin fetch.
  static void ParseClientLoginResponse(const std::string& data,
                                       std::string* sid,
                                       std::string* lsid,
                                       std::string* token);

  // From a URLFetcher result, generate an appropriate GaiaAuthError.
  // From the API documentation, both IssueAuthToken and ClientLogin have
  // the same error returns.
  static GaiaAuthConsumer::GaiaAuthError GenerateAuthError(
      const std::string& data,
      const URLRequestStatus& status);

  // Is this a special case Gaia error for TwoFactor auth?
  static bool IsSecondFactorSuccess(const std::string& alleged_error);

  // Given parameters, create a ClientLogin request body.
  static std::string MakeClientLoginBody(const std::string& username,
                                         const std::string& password,
                                         const std::string& source,
                                         const char* const service,
                                         const std::string& login_token,
                                         const std::string& login_captcha);
  // Supply the sid / lsid returned from ClientLogin in order to
  // request a long lived auth token for a service.
  static std::string MakeIssueAuthTokenBody(const std::string& sid,
                                            const std::string& lsid,
                                            const char* const service);

  // Create a fetcher useable for making any Gaia request.
  static URLFetcher* CreateGaiaFetcher(URLRequestContextGetter* getter,
                                       const std::string& body,
                                       const GURL& gaia_gurl_,
                                       URLFetcher::Delegate* delegate);


  // These fields are common to GaiaAuthenticator2, same every request
  GaiaAuthConsumer* const consumer_;
  URLRequestContextGetter* const getter_;
  std::string source_;
  const GURL client_login_gurl_;
  const GURL issue_auth_token_gurl_;

  // While a fetch is going on:
  scoped_ptr<URLFetcher> fetcher_;
  std::string request_body_;
  std::string requested_service_;  // Currently tracked for IssueAuthToken only
  bool fetch_pending_;

  friend class GaiaAuthenticator2Test;
  FRIEND_TEST_ALL_PREFIXES(GaiaAuthenticator2Test, LoginNetFailure);
  FRIEND_TEST_ALL_PREFIXES(GaiaAuthenticator2Test, CheckNormalErrorCode);
  FRIEND_TEST_ALL_PREFIXES(GaiaAuthenticator2Test, CheckTwoFactorResponse);

  DISALLOW_COPY_AND_ASSIGN(GaiaAuthenticator2);
};

#endif  // CHROME_COMMON_NET_GAIA_GAIA_AUTHENTICATOR2_H_
