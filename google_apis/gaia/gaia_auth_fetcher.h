// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GAIA_GAIA_AUTH_FETCHER_H_
#define GOOGLE_APIS_GAIA_GAIA_AUTH_FETCHER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "google_apis/gaia/gaia_auth_consumer.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "url/gurl.h"

// Authenticate a user against the Google Accounts ClientLogin API
// with various capabilities and return results to a GaiaAuthConsumer.
//
// In the future, we will also issue auth tokens from this class.
// This class should be used on a single thread, but it can be whichever thread
// that you like.
//
// This class can handle one request at a time on any thread. To parallelize
// requests, create multiple GaiaAuthFetcher's.

class GaiaAuthFetcherTest;

namespace net {
class URLFetcher;
class URLRequestContextGetter;
class URLRequestStatus;
}

class GaiaAuthFetcher : public net::URLFetcherDelegate {
 public:
  // Magic string indicating that, while a second factor is still
  // needed to complete authentication, the user provided the right password.
  static const char kSecondFactor[];

  // Magic string indicating that though the user does not have Less Secure
  // Apps enabled, the user provided the right password.
  static const char kWebLoginRequired[];

  // This will later be hidden behind an auth service which caches
  // tokens.
  GaiaAuthFetcher(GaiaAuthConsumer* consumer,
                  const std::string& source,
                  net::URLRequestContextGetter* getter);
  ~GaiaAuthFetcher() override;

  // Start a request to obtain service token for the the account identified by
  // |sid| and |lsid| and the |service|.
  //
  // Either OnIssueAuthTokenSuccess or OnIssueAuthTokenFailure will be
  // called on the consumer on the original thread.
  void StartIssueAuthToken(const std::string& sid,
                           const std::string& lsid,
                           const char* const service);

  // Start a request to obtain |service| token for the the account identified by
  // |uber_token|.
  //
  // Either OnIssueAuthTokenSuccess or OnIssueAuthTokenFailure will be
  // called on the consumer on the original thread.
  void StartTokenAuth(const std::string& uber_token,
                      const char* const service);

  // Start a request to obtain service token for the the account identified by
  // |oauth2_access_token| and the |service|.
  //
  // Either OnIssueAuthTokenSuccess or OnIssueAuthTokenFailure will be
  // called on the consumer on the original thread.
  void StartIssueAuthTokenForOAuth2(const std::string& oauth2_access_token,
                                    const char* const service);

  // Start a request to revoke |auth_token|.
  //
  // OnOAuth2RevokeTokenCompleted will be called on the consumer on the original
  // thread.
  void StartRevokeOAuth2Token(const std::string& auth_token);

  // Start a request to exchange the cookies of a signed-in user session
  // for an OAuthLogin-scoped oauth2 token.  In the case of a session with
  // multiple accounts signed in, |session_index| indicate the which of accounts
  // within the session.
  //
  // Either OnClientOAuthSuccess or OnClientOAuthFailure will be
  // called on the consumer on the original thread.
  void StartCookieForOAuthLoginTokenExchange(const std::string& session_index);

  // Start a request to exchange the cookies of a signed-in user session
  // for an OAuthLogin-scoped oauth2 token. In the case of a session with
  // multiple accounts signed in, |session_index| indicate the which of accounts
  // within the session.
  // Resulting refresh token is annotated on the server with |device_id|. Format
  // of device_id on the server is at most 64 unicode characters.
  //
  // Either OnClientOAuthSuccess or OnClientOAuthFailure will be
  // called on the consumer on the original thread.
  void StartCookieForOAuthLoginTokenExchangeWithDeviceId(
      const std::string& session_index,
      const std::string& device_id);

  // Start a request to exchange the cookies of a signed-in user session
  // and for specified client for an OAuthLogin-scoped oauth2 token. Client is
  // determined by its |client_id|. In the case of a session with multiple
  // accounts signed in, |session_index| indicate the which of accounts
  // within the session. If |fetch_token_from_auth_code| is not set fetching
  // process stops after receiving an auth code and ClientOAuthSuccess won't be
  // called.
  // Resulting refresh token is annotated on the server with |device_id|. Format
  // of device_id on the server is at most 64 unicode characters.
  //
  // Either OnClientOAuthCode or ClientOAuthSuccess or OnClientOAuthFailure
  // will be called on the consumer on the original thread.
  void StartCookieForOAuthLoginTokenExchange(
      bool fetch_token_from_auth_code,
      const std::string& session_index,
      const std::string& client_id,
      const std::string& device_id);

  // Start a request to exchange the authorization code for an OAuthLogin-scoped
  // oauth2 token.
  //
  // Either OnClientOAuthSuccess or OnClientOAuthFailure will be
  // called on the consumer on the original thread.
  void StartAuthCodeForOAuth2TokenExchange(const std::string& auth_code);

  // Start a request to exchange the authorization code for an OAuthLogin-scoped
  // oauth2 token.
  // Resulting refresh token is annotated on the server with |device_id|. Format
  // of device_id on the server is at most 64 unicode characters.
  //
  // Either OnClientOAuthSuccess or OnClientOAuthFailure will be
  // called on the consumer on the original thread.
  void StartAuthCodeForOAuth2TokenExchangeWithDeviceId(
      const std::string& auth_code,
      const std::string& device_id);

  // Start a request to get user info for the account identified by |lsid|.
  //
  // Either OnGetUserInfoSuccess or OnGetUserInfoFailure will be
  // called on the consumer on the original thread.
  void StartGetUserInfo(const std::string& lsid);

  // Start a MergeSession request to pre-login the user with the given
  // credentials.
  //
  // Start a MergeSession request to fill the browsing cookie jar with
  // credentials represented by the account whose uber-auth token is
  // |uber_token|.  This method will modify the cookies of the current profile.
  //
  // The |external_cc_result| string can specify the result of connetion checks
  // for various google properties, and MergeSession will set cookies on those
  // properties too if appropriate.  See StartGetCheckConnectionInfo() for
  // details.  The string is a comma separated list of token/result pairs, where
  // token and result are separated by a colon.  This string may be empty, in
  // which case no specific handling is performed.
  //
  // Either OnMergeSessionSuccess or OnMergeSessionFailure will be
  // called on the consumer on the original thread.
  void StartMergeSession(const std::string& uber_token,
                         const std::string& external_cc_result);

  // Start a request to exchange an OAuthLogin-scoped oauth2 access token for an
  // uber-auth token.  The returned token can be used with the method
  // StartMergeSession().
  // If |is_bound_to_channel_id| is true, then the generated UberToken will
  // be bound to the channel ID of the network context of |getter_|.
  //
  // Either OnUberAuthTokenSuccess or OnUberAuthTokenFailure will be
  // called on the consumer on the original thread.
  void StartTokenFetchForUberAuthExchange(const std::string& access_token,
                                          bool is_bound_to_channel_id);

  // Start a request to exchange an OAuthLogin-scoped oauth2 access token for a
  // ClientLogin-style service tokens.  The response to this request is the
  // same as the response to a ClientLogin request, except that captcha
  // challenges are never issued.
  //
  // Either OnClientLoginSuccess or OnClientLoginFailure will be
  // called on the consumer on the original thread. If |service| is empty,
  // the call will attempt to fetch uber auth token.
  void StartOAuthLogin(const std::string& access_token,
                       const std::string& service);

  // Starts a request to list the accounts in the GAIA cookie.
  void StartListAccounts();

  // Starts a request to log out the accounts in the GAIA cookie.
  void StartLogOut();

  // Starts a request to get the list of URLs to check for connection info.
  // Returns token/URL pairs to check, and the resulting status can be given to
  // /MergeSession requests.
  void StartGetCheckConnectionInfo();

  // Starts listing any sessions that exist for the IDP. If all requested scopes
  // have been approved by the session user, then a login hint is included in
  // the response.
  void StartListIDPSessions(const std::string& scopes,
                            const std::string& domain);

  // Generates an access token for the session, specifying the scopes and
  // |login_hint|.
  void StartGetTokenResponse(const std::string& scopes,
                             const std::string& domain,
                             const std::string& login_hint);

  // Implementation of net::URLFetcherDelegate
  void OnURLFetchComplete(const net::URLFetcher* source) override;

  // StartClientLogin been called && results not back yet?
  bool HasPendingFetch();

  // Stop any URL fetches in progress.
  virtual void CancelRequest();

  // From a URLFetcher result, generate an appropriate error.
  // From the API documentation, both IssueAuthToken and ClientLogin have
  // the same error returns.
  static GoogleServiceAuthError GenerateOAuthLoginError(
      const std::string& data,
      const net::URLRequestStatus& status);

 protected:
  // Create and start |fetcher_|, used to make all Gaia request.  |body| is
  // used as the body of the POST request sent to GAIA.  Any strings listed in
  // |headers| are added as extra HTTP headers in the request.
  //
  // |load_flags| are passed to directly to net::URLFetcher::Create() when
  // creating the URL fetcher.
  //
  // HasPendingFetch() should return false before calling this method, and will
  // return true afterwards.
  virtual void CreateAndStartGaiaFetcher(const std::string& body,
                                         const std::string& headers,
                                         const GURL& gaia_gurl,
                                         int load_flags);

  // Dispatch the results of a request.
  void DispatchFetchedRequest(const GURL& url,
                              const std::string& data,
                              const net::ResponseCookies& cookies,
                              const net::URLRequestStatus& status,
                              int response_code);

  void SetPendingFetch(bool pending_fetch);

  // Set the headers to use during the Logout call.
  void SetLogoutHeaders(const std::string& headers);

 private:
  // The format of the POST body for IssueAuthToken.
  static const char kIssueAuthTokenFormat[];
  // The format of the query string to get OAuth2 auth code from auth token.
  static const char kClientLoginToOAuth2URLFormat[];
  // The format of the POST body to get OAuth2 token pair from auth code.
  static const char kOAuth2CodeToTokenPairBodyFormat[];
  // Additional param for the POST body to get OAuth2 token pair from auth code.
  static const char kOAuth2CodeToTokenPairDeviceIdParam[];
  // The format of the POST body to revoke an OAuth2 token.
  static const char kOAuth2RevokeTokenBodyFormat[];
  // The format of the POST body for GetUserInfo.
  static const char kGetUserInfoFormat[];
  // The format of the POST body for MergeSession.
  static const char kMergeSessionFormat[];
  // The format of the URL for UberAuthToken.
  static const char kUberAuthTokenURLFormat[];
  // The format of the body for OAuthLogin.
  static const char kOAuthLoginFormat[];

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

  // Constants for parsing ClientOAuth errors.
  static const char kNeedsAdditional[];
  static const char kCaptcha[];
  static const char kTwoFactor[];

  // Constants for request/response for OAuth2 requests.
  static const char kAuthHeaderFormat[];
  static const char kOAuthHeaderFormat[];
  static const char kOAuth2BearerHeaderFormat[];
  static const char kDeviceIdHeaderFormat[];
  static const char kClientLoginToOAuth2CookiePartSecure[];
  static const char kClientLoginToOAuth2CookiePartHttpOnly[];
  static const char kClientLoginToOAuth2CookiePartCodePrefix[];
  static const int kClientLoginToOAuth2CookiePartCodePrefixLength;

  // Process the results of a ClientLogin fetch.
  void OnClientLoginFetched(const std::string& data,
                            const net::URLRequestStatus& status,
                            int response_code);

  void OnIssueAuthTokenFetched(const std::string& data,
                               const net::URLRequestStatus& status,
                               int response_code);

  void OnClientLoginToOAuth2Fetched(const std::string& data,
                                    const net::ResponseCookies& cookies,
                                    const net::URLRequestStatus& status,
                                    int response_code);

  void OnOAuth2TokenPairFetched(const std::string& data,
                                const net::URLRequestStatus& status,
                                int response_code);

  void OnOAuth2RevokeTokenFetched(const std::string& data,
                                  const net::URLRequestStatus& status,
                                  int response_code);

  void OnListAccountsFetched(const std::string& data,
                             const net::URLRequestStatus& status,
                             int response_code);

  void OnLogOutFetched(const std::string& data,
                       const net::URLRequestStatus& status,
                       int response_code);

  void OnGetUserInfoFetched(const std::string& data,
                            const net::URLRequestStatus& status,
                            int response_code);

  void OnMergeSessionFetched(const std::string& data,
                             const net::URLRequestStatus& status,
                             int response_code);

  void OnUberAuthTokenFetch(const std::string& data,
                            const net::URLRequestStatus& status,
                            int response_code);

  void OnOAuthLoginFetched(const std::string& data,
                           const net::URLRequestStatus& status,
                           int response_code);

  void OnGetCheckConnectionInfoFetched(const std::string& data,
                                       const net::URLRequestStatus& status,
                                       int response_code);

  void OnListIdpSessionsFetched(const std::string& data,
                                const net::URLRequestStatus& status,
                                int response_code);

  void OnGetTokenResponseFetched(const std::string& data,
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

  // Parse ClientLogin to OAuth2 response.
  static bool ParseClientLoginToOAuth2Response(
      const net::ResponseCookies& cookies,
      std::string* auth_code);

  static bool ParseClientLoginToOAuth2Cookie(const std::string& cookie,
                                             std::string* auth_code);

  static bool ParseListIdpSessionsResponse(const std::string& data,
                                           std::string* login_hint);

  // Is this a special case Gaia error for TwoFactor auth?
  static bool IsSecondFactorSuccess(const std::string& alleged_error);

  // Is this a special case Gaia error for Less Secure Apps?
  static bool IsWebLoginRequiredSuccess(const std::string& alleged_error);

  // Supply the sid / lsid returned from ClientLogin in order to
  // request a long lived auth token for a service.
  static std::string MakeIssueAuthTokenBody(const std::string& sid,
                                            const std::string& lsid,
                                            const char* const service);
  // Given auth code and device ID (optional), create body to get OAuth2 token
  // pair.
  static std::string MakeGetTokenPairBody(const std::string& auth_code,
                                          const std::string& device_id);
  // Given an OAuth2 token, create body to revoke the token.
  std::string MakeRevokeTokenBody(const std::string& auth_token);
  // Supply the lsid returned from ClientLogin in order to fetch
  // user information.
  static std::string MakeGetUserInfoBody(const std::string& lsid);

  // Supply the authentication token returned from StartIssueAuthToken.
  static std::string MakeMergeSessionQuery(
      const std::string& auth_token,
      const std::string& external_cc_result,
      const std::string& continue_url,
      const std::string& source);

  static std::string MakeGetAuthCodeHeader(const std::string& auth_token);

  static std::string MakeOAuthLoginBody(const std::string& service,
                                        const std::string& source);

  static std::string MakeListIDPSessionsBody(const std::string& scopes,
                                             const std::string& domain);

  static std::string MakeGetTokenResponseBody(const std::string& scopes,
                                              const std::string& domain,
                                              const std::string& login_hint);

  // From a URLFetcher result, generate an appropriate error.
  // From the API documentation, both IssueAuthToken and ClientLogin have
  // the same error returns.
  static GoogleServiceAuthError GenerateAuthError(
      const std::string& data,
      const net::URLRequestStatus& status);

  // These fields are common to GaiaAuthFetcher, same every request.
  GaiaAuthConsumer* const consumer_;
  net::URLRequestContextGetter* const getter_;
  std::string source_;
  const GURL issue_auth_token_gurl_;
  const GURL oauth2_token_gurl_;
  const GURL oauth2_revoke_gurl_;
  const GURL get_user_info_gurl_;
  const GURL merge_session_gurl_;
  const GURL uberauth_token_gurl_;
  const GURL oauth_login_gurl_;
  const GURL list_accounts_gurl_;
  const GURL logout_gurl_;
  const GURL get_check_connection_info_url_;
  const GURL oauth2_iframe_url_;

  // While a fetch is going on:
  std::unique_ptr<net::URLFetcher> fetcher_;
  GURL client_login_to_oauth2_gurl_;
  std::string request_body_;
  std::string requested_service_;
  bool fetch_pending_ = false;
  bool fetch_token_from_auth_code_ = false;

  // Headers used during the Logout call.
  std::string logout_headers_;

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
  FRIEND_TEST_ALL_PREFIXES(GaiaAuthFetcherTest,
      ParseClientLoginToOAuth2Response);
  FRIEND_TEST_ALL_PREFIXES(GaiaAuthFetcherTest, ParseOAuth2TokenPairResponse);
  FRIEND_TEST_ALL_PREFIXES(GaiaAuthFetcherTest, ClientOAuthSuccess);
  FRIEND_TEST_ALL_PREFIXES(GaiaAuthFetcherTest, ClientOAuthWithQuote);
  FRIEND_TEST_ALL_PREFIXES(GaiaAuthFetcherTest, ClientOAuthChallengeSuccess);
  FRIEND_TEST_ALL_PREFIXES(GaiaAuthFetcherTest, ClientOAuthChallengeQuote);

  DISALLOW_COPY_AND_ASSIGN(GaiaAuthFetcher);
};

#endif  // GOOGLE_APIS_GAIA_GAIA_AUTH_FETCHER_H_
