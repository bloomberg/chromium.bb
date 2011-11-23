// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_NET_GAIA_GAIA_AUTH_FETCHER_H_
#define CHROME_COMMON_NET_GAIA_GAIA_AUTH_FETCHER_H_
#pragma once

#include <string>

#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/common/net/gaia/gaia_auth_consumer.h"
#include "content/public/common/url_fetcher_delegate.h"
#include "googleurl/src/gurl.h"

// Authenticate a user against the Google Accounts ClientLogin API
// with various capabilities and return results to a GaiaAuthConsumer.
//
// In the future, we will also issue auth tokens from this class.
// This class should be used on a single thread, but it can be whichever thread
// that you like.
//
// This class can handle one request at a time. To parallelize requests,
// create multiple GaiaAuthFetcher's.

class GaiaAuthFetcherTest;

namespace net {
class URLRequestContextGetter;
class URLRequestStatus;
}

class GaiaAuthFetcher : public content::URLFetcherDelegate {
 public:
  enum HostedAccountsSetting {
    HostedAccountsAllowed,
    HostedAccountsNotAllowed
  };

  // Magic string indicating that, while a second factor is still
  // needed to complete authentication, the user provided the right password.
  static const char kSecondFactor[];

  // This will later be hidden behind an auth service which caches
  // tokens.
  GaiaAuthFetcher(GaiaAuthConsumer* consumer,
                  const std::string& source,
                  net::URLRequestContextGetter* getter);
  virtual ~GaiaAuthFetcher();

  // GaiaAuthConsumer will be called on the original thread
  // after results come back. This class is thread agnostic.
  // You can't make more than request at a time.
  void StartClientLogin(const std::string& username,
                        const std::string& password,
                        const char* const service,
                        const std::string& login_token,
                        const std::string& login_captcha,
                        HostedAccountsSetting allow_hosted_accounts);

  // GaiaAuthConsumer will be called on the original thread
  // after results come back. This class is thread agnostic.
  // You can't make more than one request at a time.
  void StartIssueAuthToken(const std::string& sid,
                           const std::string& lsid,
                           const char* const service);

  // Start a request to get a particular key from user info.
  // GaiaAuthConsumer will be called back on the same thread when
  // results come back.
  // You can't make more than one request at a time.
  void StartGetUserInfo(const std::string& lsid,
                        const std::string& info_key);

  // Start a TokenAuth request to pre-login the user with the given credentials.
  void StartTokenAuth(const std::string& auth_token);

  // Start a MergeSession request to pre-login the user with the given
  // credentials.  Unlike TokenAuth above, MergeSession will not sign out any
  // existing accounts.
  void StartMergeSession(const std::string& auth_token);

  // Implementation of content::URLFetcherDelegate
  virtual void OnURLFetchComplete(const content::URLFetcher* source) OVERRIDE;

  // StartClientLogin been called && results not back yet?
  bool HasPendingFetch();

  // Stop any URL fetches in progress.
  void CancelRequest();

  // From a URLFetcher result, generate an appropriate error.
  // From the API documentation, both IssueAuthToken and ClientLogin have
  // the same error returns.
  static GoogleServiceAuthError GenerateOAuthLoginError(
      const std::string& data,
      const net::URLRequestStatus& status);

 private:
  // ClientLogin body constants that don't change
  static const char kCookiePersistence[];
  static const char kAccountTypeHostedOrGoogle[];
  static const char kAccountTypeGoogle[];

  // The format of the POST body for ClientLogin.
  static const char kClientLoginFormat[];
  // The format of said POST body when CAPTCHA token & answer are specified.
  static const char kClientLoginCaptchaFormat[];
  // The format of the POST body for IssueAuthToken.
  static const char kIssueAuthTokenFormat[];
  // The format of the POST body for GetUserInfo.
  static const char kGetUserInfoFormat[];
  // The format of the POST body for TokenAuth.
  static const char kTokenAuthFormat[];
  // The format of the POST body for MergeSession.
  static const char kMergeSessionFormat[];

  // Constants for parsing ClientLogin errors.
  static const char kAccountDeletedError[];
  static const char kAccountDeletedErrorCode[];
  static const char kAccountDisabledError[];
  static const char kAccountDisabledErrorCode[];
  static const char kBadAuthenticationError[];
  static const char kBadAuthenticationErrorCode[];
  static const char kCaptchaError[];
  static const char kCaptchaErrorCode[];
  static const char kServiceUnavailableError[];
  static const char kServiceUnavailableErrorCode[];
  static const char kErrorParam[];
  static const char kErrorUrlParam[];
  static const char kCaptchaUrlParam[];
  static const char kCaptchaTokenParam[];

  // Process the results of a ClientLogin fetch.
  void OnClientLoginFetched(const std::string& data,
                            const net::URLRequestStatus& status,
                            int response_code);

  void OnIssueAuthTokenFetched(const std::string& data,
                               const net::URLRequestStatus& status,
                               int response_code);

  void OnGetUserInfoFetched(const std::string& data,
                            const net::URLRequestStatus& status,
                            int response_code);

  void OnTokenAuthFetched(const std::string& data,
                          const net::URLRequestStatus& status,
                          int response_code);

  void OnMergeSessionFetched(const std::string& data,
                             const net::URLRequestStatus& status,
                             int response_code);

  // Tokenize the results of a ClientLogin fetch.
  static void ParseClientLoginResponse(const std::string& data,
                                       std::string* sid,
                                       std::string* lsid,
                                       std::string* token);

  static void ParseClientLoginFailure(const std::string& data,
                                      std::string* error,
                                      std::string* error_url,
                                      std::string* captcha_url,
                                      std::string* captcha_token);

  // Is this a special case Gaia error for TwoFactor auth?
  static bool IsSecondFactorSuccess(const std::string& alleged_error);

  // Given parameters, create a ClientLogin request body.
  static std::string MakeClientLoginBody(
      const std::string& username,
      const std::string& password,
      const std::string& source,
      const char* const service,
      const std::string& login_token,
      const std::string& login_captcha,
      HostedAccountsSetting allow_hosted_accounts);
  // Supply the sid / lsid returned from ClientLogin in order to
  // request a long lived auth token for a service.
  static std::string MakeIssueAuthTokenBody(const std::string& sid,
                                            const std::string& lsid,
                                            const char* const service);
  // Supply the lsid returned from ClientLogin in order to fetch
  // user information.
  static std::string MakeGetUserInfoBody(const std::string& lsid);

  // Supply the authentication token returned from StartIssueAuthToken.
  static std::string MakeTokenAuthBody(const std::string& auth_token,
                                       const std::string& continue_url,
                                       const std::string& source);

  // Supply the authentication token returned from StartIssueAuthToken.
  static std::string MakeMergeSessionBody(const std::string& auth_token,
                                       const std::string& continue_url,
                                       const std::string& source);

  // Create a fetcher useable for making any Gaia request.
  static content::URLFetcher* CreateGaiaFetcher(
      net::URLRequestContextGetter* getter,
      const std::string& body,
      const GURL& gaia_gurl,
      bool send_cookies,
      content::URLFetcherDelegate* delegate);

  // From a URLFetcher result, generate an appropriate error.
  // From the API documentation, both IssueAuthToken and ClientLogin have
  // the same error returns.
  static GoogleServiceAuthError GenerateAuthError(
      const std::string& data,
      const net::URLRequestStatus& status);

  // These fields are common to GaiaAuthFetcher, same every request
  GaiaAuthConsumer* const consumer_;
  net::URLRequestContextGetter* const getter_;
  std::string source_;
  const GURL client_login_gurl_;
  const GURL issue_auth_token_gurl_;
  const GURL get_user_info_gurl_;
  const GURL token_auth_gurl_;
  const GURL merge_session_gurl_;

  // While a fetch is going on:
  scoped_ptr<content::URLFetcher> fetcher_;
  std::string request_body_;
  std::string requested_service_;   // Currently tracked for IssueAuthToken only
  std::string requested_info_key_;  // Currently tracked for GetUserInfo only
  bool fetch_pending_;

  friend class GaiaAuthFetcherTest;
  FRIEND_TEST_ALL_PREFIXES(GaiaAuthFetcherTest, CaptchaParse);
  FRIEND_TEST_ALL_PREFIXES(GaiaAuthFetcherTest, AccountDeletedError);
  FRIEND_TEST_ALL_PREFIXES(GaiaAuthFetcherTest, AccountDisabledError);
  FRIEND_TEST_ALL_PREFIXES(GaiaAuthFetcherTest, BadAuthenticationError);
  FRIEND_TEST_ALL_PREFIXES(GaiaAuthFetcherTest, IncomprehensibleError);
  FRIEND_TEST_ALL_PREFIXES(GaiaAuthFetcherTest, ServiceUnavailableError);
  FRIEND_TEST_ALL_PREFIXES(GaiaAuthFetcherTest, CheckNormalErrorCode);
  FRIEND_TEST_ALL_PREFIXES(GaiaAuthFetcherTest, CheckTwoFactorResponse);
  FRIEND_TEST_ALL_PREFIXES(GaiaAuthFetcherTest, LoginNetFailure);

  DISALLOW_COPY_AND_ASSIGN(GaiaAuthFetcher);
};

#endif  // CHROME_COMMON_NET_GAIA_GAIA_AUTH_FETCHER_H_
