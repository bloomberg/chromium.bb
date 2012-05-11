// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_GAIA_GAIA_OAUTH_FETCHER_H_
#define CHROME_BROWSER_NET_GAIA_GAIA_OAUTH_FETCHER_H_
#pragma once

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/net/chrome_cookie_notification_details.h"
#include "chrome/browser/net/gaia/gaia_oauth_consumer.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/common/url_fetcher_delegate.h"
#include "googleurl/src/gurl.h"

struct ChromeCookieDetails;

class Browser;
class Profile;

namespace net {
class URLRequestContextGetter;
class URLRequestStatus;
typedef std::vector<std::string> ResponseCookies;
}

// Authenticate a user using Gaia's OAuth1 and OAuth2 support.
//
// Users of this class typically desire an OAuth2 Access token scoped for a
// specific service.  This will typically start with either an interactive
// login, using StartGetOAuthToken, or with a long-lived OAuth1 all-scope
// token obtained through a previous login or other means, using
// StartOAuthGetAccessToken.  In fact, one can start with any of these
// routines:
//   StartGetOAuthToken()
//   StartOAuthGetAccessToken()
//   StartOAuthWrapBridge()
//   StartUserInfo()
// with the expectation that each of these calls the next Start* routine in
// the sequence, except for StartUserInfo as it's the last one.
//
// This class can handle one request at a time, and all calls through an
// instance should be serialized.
class GaiaOAuthFetcher : public content::URLFetcherDelegate,
                         public content::NotificationObserver {
 public:
  // Defines steps of OAuth process performed by this class.
  typedef enum {
    OAUTH1_LOGIN,
    OAUTH1_REQUEST_TOKEN,
    OAUTH1_ALL_ACCESS_TOKEN,
    OAUTH2_SERVICE_ACCESS_TOKEN,
    USER_INFO,
    OAUTH2_REVOKE_TOKEN,
  } RequestType;

  GaiaOAuthFetcher(GaiaOAuthConsumer* consumer,
                   net::URLRequestContextGetter* getter,
                   Profile* profile,
                   const std::string& service_scope);

  virtual ~GaiaOAuthFetcher();

  // Sets the mask of which OAuth fetch steps should be automatically kicked
  // of upon successful completition of the previous steps. By default,
  // this class will chain all steps in OAuth proccess.
  void SetAutoFetchLimit(RequestType limit) { auto_fetch_limit_ = limit; }

  // Obtains an OAuth 1 request token
  //
  // Pops up a window aimed at the Gaia OAuth URL for GetOAuthToken and then
  // listens for COOKIE_CHANGED notifications
  virtual void StartGetOAuthToken();

  // Non-UI version of the method above. Initiates Gaia OAuth request token
  // retrieval.
  void StartGetOAuthTokenRequest();

  // Performs account login based on OAuth1 access token and its secret.
  void StartOAuthLogin(const char* source,
                       const char* service,
                       const std::string& oauth1_access_token,
                       const std::string& oauth1_access_token_secret);

  // Obtains an OAuth1 access token and secret
  //
  // oauth1_request_token is from GetOAuthToken's result.
  virtual void StartOAuthGetAccessToken(
      const std::string& oauth1_request_token);

  // Obtains an OAuth2 access token using Gaia's OAuth1-to-OAuth2 bridge.
  //
  // oauth1_access_token and oauth1_access_token_secret are from
  // OAuthGetAccessToken's result.
  //
  // wrap_token_duration is typically one hour,
  // which is also the max -- you can only decrease it.
  //
  // service_scope will be used as a service name.  For example, Chromium Sync
  // uses https://www.googleapis.com/auth/chromesync for its OAuth2 service
  // scope here as well as for its service name in TokenService.
  virtual void StartOAuthWrapBridge(
      const std::string& oauth1_access_token,
      const std::string& oauth1_access_token_secret,
      const std::string& wrap_token_duration,
      const std::string& service_scope);

  // Obtains user information related to an OAuth2 access token
  //
  // oauth2_access_token is from OAuthWrapBridge's result.
  virtual void StartUserInfo(const std::string& oauth2_access_token);

  // Starts a request for revoking the given OAuth access token (as requested by
  // StartOAuthGetAccessToken).
  virtual void StartOAuthRevokeAccessToken(const std::string& token,
                                           const std::string& secret);

  // Starts a request for revoking the given OAuth Bearer token (as requested by
  // StartOAuthWrapBridge).
  virtual void StartOAuthRevokeWrapToken(const std::string& token);

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Called when a cookie, e. g. oauth_token, changes
  virtual void OnCookieChanged(Profile* profile,
                               ChromeCookieDetails* cookie_details);

  // Called when a cookie, e. g. oauth_token, changes
  virtual void OnBrowserClosing(Browser* profile,
                                bool detail);

  // Implementation of content::URLFetcherDelegate
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

  // StartGetOAuthToken (or other Start* routine) been called, but results
  // are not back yet.
  virtual bool HasPendingFetch() const;

  // Stop any URL fetches in progress.
  virtual void CancelRequest();

 protected:
  // Stores the type of the current request in flight.
  RequestType request_type_;

 private:
  // Process the results of a GetOAuthToken fetch via UI.
  virtual void OnGetOAuthTokenFetched(const std::string& token);

  // Process the results of a GetOAuthToken fetch for non-UI driven path.
  virtual void OnGetOAuthTokenUrlFetched(const net::ResponseCookies& cookies,
                                         const net::URLRequestStatus& status,
                                         int response_code);

  // Process the results of a OAuthLogin fetch.
  virtual void OnOAuthLoginFetched(const std::string& data,
                                   const net::URLRequestStatus& status,
                                   int response_code);

  // Process the results of a OAuthGetAccessToken fetch.
  virtual void OnOAuthGetAccessTokenFetched(const std::string& data,
                                            const net::URLRequestStatus& status,
                                            int response_code);

  // Process the results of a OAuthWrapBridge fetch.
  virtual void OnOAuthWrapBridgeFetched(const std::string& data,
                                        const net::URLRequestStatus& status,
                                        int response_code);

  // Process the results of a token revocation fetch.
  virtual void OnOAuthRevokeTokenFetched(const std::string& data,
                                         const net::URLRequestStatus& status,
                                         int response_code);

  // Process the results of a userinfo fetch.
  virtual void OnUserInfoFetched(const std::string& data,
                                 const net::URLRequestStatus& status,
                                 int response_code);

  // Tokenize the results of a GetOAuthToken fetch.
  static void ParseGetOAuthTokenResponse(const std::string& data,
                                         std::string* token);

  // Tokenize the results of a OAuthLogin fetch.
  static void ParseOAuthLoginResponse(const std::string& data,
                                      std::string* sid,
                                      std::string* lsid,
                                      std::string* auth);

  // Tokenize the results of a OAuthGetAccessToken fetch.
  static void ParseOAuthGetAccessTokenResponse(const std::string& data,
                                               std::string* token,
                                               std::string* secret);

  // Tokenize the results of a OAuthWrapBridge fetch.
  static void ParseOAuthWrapBridgeResponse(const std::string& data,
                                           std::string* token,
                                           std::string* expires_in);

  // Tokenize the results of a userinfo fetch.
  static void ParseUserInfoResponse(const std::string& data,
                                    std::string* email);

  // From a URLFetcher result, generate an appropriate error.
  static GoogleServiceAuthError GenerateAuthError(
      const std::string& data,
      const net::URLRequestStatus& status,
      int response_code);

  // Given parameters, create a OAuth v1 request URL.
  static GURL MakeGetOAuthTokenUrl(const std::string& oauth1_login_scope,
                                   const std::string& product_name);

  // Given parameters, create a OAuthGetAccessToken request body.
  static std::string MakeOAuthGetAccessTokenBody(
      const std::string& oauth1_request_token);

  // Given parameters, create a OAuthLogin request body.
  static std::string MakeOAuthLoginBody(
      const char* source,
      const char* service,
      const std::string& oauth1_access_token,
      const std::string& oauth1_access_token_secret);

  // Given parameters, create a OAuthWrapBridge request body.
  static std::string MakeOAuthWrapBridgeBody(
      const std::string& oauth1_access_token,
      const std::string& oauth1_access_token_secret,
      const std::string& wrap_token_duration,
      const std::string& oauth2_service_scope);

  // Create a fetcher useable for making any Gaia OAuth request.
  static content::URLFetcher* CreateGaiaFetcher(
      net::URLRequestContextGetter* getter,
      const GURL& gaia_gurl_,
      const std::string& body,
      const std::string& headers,
      bool send_cookies,
      content::URLFetcherDelegate* delegate);

  bool ShouldAutoFetch(RequestType fetch_step);

  // These fields are common to GaiaOAuthFetcher, same every request
  GaiaOAuthConsumer* const consumer_;
  net::URLRequestContextGetter* const getter_;
  Profile* profile_;
  Browser* popup_;
  content::NotificationRegistrar registrar_;

  // While a fetch is going on:
  scoped_ptr<content::URLFetcher> fetcher_;
  std::string request_body_;
  std::string request_headers_;
  std::string service_scope_;
  bool fetch_pending_;
  RequestType auto_fetch_limit_;

  DISALLOW_COPY_AND_ASSIGN(GaiaOAuthFetcher);
};

#endif  // CHROME_BROWSER_NET_GAIA_GAIA_OAUTH_FETCHER_H_
