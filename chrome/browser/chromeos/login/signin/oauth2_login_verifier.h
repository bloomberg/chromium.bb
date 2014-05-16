// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SIGNIN_OAUTH2_LOGIN_VERIFIER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SIGNIN_OAUTH2_LOGIN_VERIFIER_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "google_apis/gaia/gaia_auth_consumer.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "google_apis/gaia/oauth2_token_service.h"
#include "net/url_request/url_request_context_getter.h"

namespace chromeos {

// Given the OAuth2 refresh token, this class will try to exchange it for GAIA
// credentials (SID+LSID) and populate current session's cookie jar.
class OAuth2LoginVerifier : public base::SupportsWeakPtr<OAuth2LoginVerifier>,
                            public GaiaAuthConsumer,
                            public OAuth2TokenService::Consumer {
 public:
  typedef base::Callback<void(bool connection_error)> ErrorHandler;

  class Delegate {
   public:
    virtual ~Delegate() {}
    // Invoked when cookie session is successfully merged.
    virtual void OnSessionMergeSuccess() = 0;

    // Invoked when cookie session can not be merged.
    virtual void OnSessionMergeFailure(bool connection_error) = 0;

    // Invoked when account list is retrieved during post-merge session
    // verification.
    virtual void OnListAccountsSuccess(const std::string& data) = 0;

    // Invoked when post-merge session verification fails.
    virtual void OnListAccountsFailure(bool connection_error) = 0;
  };

  OAuth2LoginVerifier(OAuth2LoginVerifier::Delegate* delegate,
                      net::URLRequestContextGetter* system_request_context,
                      net::URLRequestContextGetter* user_request_context,
                      const std::string& oauthlogin_access_token);
  virtual ~OAuth2LoginVerifier();

  // Initiates verification of GAIA cookies in |profile|'s cookie jar.
  void VerifyUserCookies(Profile* profile);

  // Attempts to restore session from OAuth2 refresh token minting all necesarry
  // tokens along the way (OAuth2 access token, SID/LSID, GAIA service token).
  void VerifyProfileTokens(Profile* profile);

 private:
  enum SessionRestoreType {
    RESTORE_UNDEFINED = 0,
    RESTORE_FROM_GAIA_TOKEN = 1,
    RESTORE_FROM_OAUTH2_REFRESH_TOKEN = 2,
  };
  // GaiaAuthConsumer overrides.
  virtual void OnUberAuthTokenSuccess(const std::string& token) OVERRIDE;
  virtual void OnUberAuthTokenFailure(
      const GoogleServiceAuthError& error) OVERRIDE;
  virtual void OnMergeSessionSuccess(const std::string& data) OVERRIDE;
  virtual void OnMergeSessionFailure(
      const GoogleServiceAuthError& error) OVERRIDE;
  virtual void OnListAccountsSuccess(const std::string& data) OVERRIDE;
  virtual void OnListAccountsFailure(
      const GoogleServiceAuthError& error) OVERRIDE;

  // OAuth2TokenService::Consumer overrides.
  virtual void OnGetTokenSuccess(const OAuth2TokenService::Request* request,
                                 const std::string& access_token,
                                 const base::Time& expiration_time) OVERRIDE;
  virtual void OnGetTokenFailure(const OAuth2TokenService::Request* request,
                                 const GoogleServiceAuthError& error) OVERRIDE;

  // Starts fetching OAuth1 access token for OAuthLogin call.
  void StartFetchingOAuthLoginAccessToken(Profile* profile);

  // Starts OAuthLogin request for GAIA uber-token.
  void StartOAuthLoginForUberToken();

  // Attempts to merge session from present |gaia_token_|.
  void StartMergeSession();

  // Schedules post merge verification to ensure that browser session restore
  // hasn't stumped over SID/LSID.
  void SchedulePostMergeVerification();

  // Starts GAIA auth cookies (SID/LSID) verification.
  void StartAuthCookiesVerification();

  // Decides how to proceed on GAIA |error|. If the error looks temporary,
  // retries |task| after certain delay until max retry count is reached.
  void RetryOnError(const char* operation_id,
                    const GoogleServiceAuthError& error,
                    const base::Closure& task_to_retry,
                    const ErrorHandler& error_handler);

  // Called when network is connected.
  void VerifyProfileTokensImpl(Profile* profile);

  OAuth2LoginVerifier::Delegate* delegate_;
  scoped_refptr<net::URLRequestContextGetter> system_request_context_;
  scoped_refptr<net::URLRequestContextGetter> user_request_context_;
  scoped_ptr<GaiaAuthFetcher> gaia_fetcher_;
  std::string access_token_;
  std::string gaia_token_;
  scoped_ptr<OAuth2TokenService::Request> login_token_request_;
  // The retry counter. Increment this only when failure happened.
  int retry_count_;

  DISALLOW_COPY_AND_ASSIGN(OAuth2LoginVerifier);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SIGNIN_OAUTH2_LOGIN_VERIFIER_H_
