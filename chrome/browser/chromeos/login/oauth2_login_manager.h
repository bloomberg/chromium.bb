// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_OAUTH2_LOGIN_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_OAUTH2_LOGIN_MANAGER_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/login/oauth2_login_verifier.h"
#include "chrome/browser/chromeos/login/oauth2_policy_fetcher.h"
#include "chrome/browser/chromeos/login/oauth2_token_fetcher.h"
#include "chrome/browser/chromeos/login/oauth_login_manager.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "net/url_request/url_request_context_getter.h"

class GoogleServiceAuthError;
class Profile;
class TokenService;

namespace chromeos {

// OAuth2 specialization of OAuthLoginManager.
class OAuth2LoginManager : public OAuthLoginManager,
                           public content::NotificationObserver,
                           public OAuth2LoginVerifier::Delegate,
                           public OAuth2TokenFetcher::Delegate {
 public:
  explicit OAuth2LoginManager(OAuthLoginManager::Delegate* delegate);
  virtual ~OAuth2LoginManager();

  // OAuthLoginManager overrides.
  virtual void RestorePolicyTokens(
      net::URLRequestContextGetter* auth_request_context) OVERRIDE;
  virtual void RestoreSession(
      Profile* user_profile,
      net::URLRequestContextGetter* auth_request_context,
      bool restore_from_auth_cookies) OVERRIDE;
  virtual void ContinueSessionRestore() OVERRIDE;
  virtual void Stop() OVERRIDE;

 private:
  // Session restore outcomes (for UMA).
  enum {
    SESSION_RESTORE_UNDEFINED = 0,
    SESSION_RESTORE_SUCCESS = 1,
    SESSION_RESTORE_TOKEN_FETCH_FAILED = 2,
    SESSION_RESTORE_NO_REFRESH_TOKEN_FAILED = 3,
    SESSION_RESTORE_OAUTHLOGIN_FAILED = 4,
    SESSION_RESTORE_MERGE_SESSION_FAILED = 5,
    SESSION_RESTORE_COUNT = SESSION_RESTORE_MERGE_SESSION_FAILED,
  };
  // content::NotificationObserver overrides.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // OAuth2LoginVerifier::Delegate overrides.
  virtual void OnOAuthLoginSuccess(
      const GaiaAuthConsumer::ClientLoginResult& gaia_credentials) OVERRIDE;
  virtual void OnOAuthLoginFailure() OVERRIDE;
  virtual void OnSessionMergeSuccess() OVERRIDE;
  virtual void OnSessionMergeFailure() OVERRIDE;

  // OAuth2TokenFetcher::Delegate overrides.
  virtual void OnOAuth2TokensAvailable(
      const GaiaAuthConsumer::ClientOAuthResult& oauth2_tokens) OVERRIDE;
  virtual void OnOAuth2TokensFetchFailed() OVERRIDE;

  // Retrieves TokenService for |user_profile_| and sets up notification
  // observer events.
  TokenService* SetupTokenService();

  // Removes legacy tokens form OAuth1 flow.
  void RemoveLegacyTokens();

  // Loads previously stored OAuth2 tokens and kicks off its validation.
  void LoadAndVerifyOAuth2Tokens();

  // Attempts to fetch OAuth2 tokens by using pre-authenticated cookie jar from
  // provided |auth_profile|.
  void FetchOAuth2Tokens();

  // Reports when all tokens are loaded.
  void ReportOAuth2TokensLoaded();

  // Issue GAIA cookie recovery (MergeSession) from |refresh_token_|.
  void RestoreSessionCookies();

  // Fetches device policy OAuth2 access tokens if have not attempted or
  // failed that step previously.
  void FetchPolicyTokens();

  // Checks GAIA error and figures out whether the request should be
  // re-attempted.
  bool RetryOnError(const GoogleServiceAuthError& error);

  // On successfuly OAuthLogin, starts token service token fetching process.
  void StartTokenService(
      const GaiaAuthConsumer::ClientLoginResult& gaia_credentials);

  // Keeps the track if we have already reported OAuth2 token being loaded
  // by TokenService.
  bool loading_reported_;
  content::NotificationRegistrar registrar_;
  scoped_ptr<OAuth2TokenFetcher> oauth2_token_fetcher_;
  scoped_ptr<OAuth2LoginVerifier> login_verifier_;
  scoped_ptr<OAuth2PolicyFetcher> oauth2_policy_fetcher_;
  // OAuth2 refresh token.
  std::string refresh_token_;

  DISALLOW_COPY_AND_ASSIGN(OAuth2LoginManager);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_OAUTH2_LOGIN_MANAGER_H_
