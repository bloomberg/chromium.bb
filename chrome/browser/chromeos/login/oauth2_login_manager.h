// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_OAUTH2_LOGIN_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_OAUTH2_LOGIN_MANAGER_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/login/oauth2_login_verifier.h"
#include "chrome/browser/chromeos/login/oauth2_token_fetcher.h"
#include "components/browser_context_keyed_service/browser_context_keyed_service.h"
#include "google_apis/gaia/oauth2_token_service.h"
#include "net/url_request/url_request_context_getter.h"

class GoogleServiceAuthError;
class Profile;
class TokenService;

namespace chromeos {

// This class is responsible for restoring authenticated web sessions out of
// OAuth2 refresh tokens or pre-authenticated cookie jar.
class OAuth2LoginManager : public BrowserContextKeyedService,
                           public OAuth2LoginVerifier::Delegate,
                           public OAuth2TokenFetcher::Delegate,
                           public OAuth2TokenService::Observer {
 public:
  // Session restore states.
  enum SessionRestoreState {
    // Session restore is not started.
    SESSION_RESTORE_NOT_STARTED,
    // Session restore is being prepared.
    SESSION_RESTORE_PREPARING,
    // Session restore is in progress. We are currently issuing calls to verify
    // stored OAuth tokens and populate cookie jar with GAIA credentials.
    SESSION_RESTORE_IN_PROGRESS,
    // Session restore is completed.
    SESSION_RESTORE_DONE,
    // Session restore failed.
    SESSION_RESTORE_FAILED,
  };

  // Session restore strategy.
  enum SessionRestoreStrategy {
    // Generate OAuth2 refresh token from authentication profile's cookie jar.
    // Restore session from generated OAuth2 refresh token.
    RESTORE_FROM_COOKIE_JAR,
    // Restore session from saved OAuth2 refresh token from TokenServices.
    RESTORE_FROM_SAVED_OAUTH2_REFRESH_TOKEN,
    // Restore session from OAuth2 refresh token passed via command line.
    RESTORE_FROM_PASSED_OAUTH2_REFRESH_TOKEN,
    // Restore session from authentication code passed via command line.
    RESTORE_FROM_AUTH_CODE,
  };

  class Observer {
   public:
    virtual ~Observer() {}

    // Raised when merge session state changes.
    virtual void OnSessionRestoreStateChanged(Profile* user_profile,
                                              SessionRestoreState state) {}

    // Raised when a new OAuth2 refresh token is avaialble.
    virtual void OnNewRefreshTokenAvaiable(Profile* user_profile) {}

    // Raised when session's GAIA credentials (SID+LSID) are available to
    // other signed in services.
    virtual void OnSessionAuthenticated(Profile* user_profile) {}
  };

  explicit OAuth2LoginManager(Profile* user_profile);
  virtual ~OAuth2LoginManager();

  void AddObserver(OAuth2LoginManager::Observer* observer);
  void RemoveObserver(OAuth2LoginManager::Observer* observer);

  // Restores and verifies OAuth tokens either following specified
  // |restore_strategy|. For |restore_strategy| with values
  // RESTORE_FROM_PASSED_OAUTH2_REFRESH_TOKEN or
  // RESTORE_FROM_AUTH_CODE, respectively
  // parameters |oauth2_refresh_token| or |auth_code| need to have non-empty
  // value.
  void RestoreSession(
      net::URLRequestContextGetter* auth_request_context,
      SessionRestoreStrategy restore_strategy,
      const std::string& oauth2_refresh_token,
      const std::string& auth_code);

  // Continues session restore after transient network errors.
  void ContinueSessionRestore();

  // Stops all background authentication requests.
  void Stop();

  // Returns session restore state.
  SessionRestoreState state() { return state_; }

  const base::Time& session_restore_start() { return session_restore_start_; }

  // Returns true if the tab loading should block until session restore
  // finishes.
  bool ShouldBlockTabLoading();

 private:
  friend class MergeSessionLoadPageTest;

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

  // BrowserContextKeyedService implementation.
  virtual void Shutdown() OVERRIDE;

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

  // OAuth2TokenService::Observer implementation:
  virtual void OnRefreshTokenAvailable(const std::string& account_id) OVERRIDE;

  // Signals delegate that authentication is completed, kicks off token fetching
  // process in TokenService.
  void CompleteAuthentication();

  // Retrieves TokenService for |user_profile_| and sets up notification
  // observer events.
  TokenService* SetupTokenService();

  // Records OAuth2 tokens fetched through cookies-to-token exchange into
  // TokenService.
  void StoreOAuth2Tokens(
      const GaiaAuthConsumer::ClientOAuthResult& oauth2_tokens);

  // Loads previously stored OAuth2 tokens and kicks off its validation.
  void LoadAndVerifyOAuth2Tokens();

  // Attempts to fetch OAuth2 tokens by using pre-authenticated cookie jar from
  // provided |auth_profile|.
  void FetchOAuth2Tokens();

  // Reports when all tokens are loaded.
  void ReportOAuth2TokensLoaded();

  // Issue GAIA cookie recovery (MergeSession) from |refresh_token_|.
  void RestoreSessionCookies();

  // Checks GAIA error and figures out whether the request should be
  // re-attempted.
  bool RetryOnError(const GoogleServiceAuthError& error);

  // On successfuly OAuthLogin, starts token service token fetching process.
  void StartTokenService(
      const GaiaAuthConsumer::ClientLoginResult& gaia_credentials);

  // Changes |state_|, if needed fires observers (OnSessionRestoreStateChanged).
  void SetSessionRestoreState(SessionRestoreState state);

  // Testing helper.
  void SetSessionRestoreStartForTesting(const base::Time& time);

  // Keeps the track if we have already reported OAuth2 token being loaded
  // by TokenService.
  Profile* user_profile_;
  scoped_refptr<net::URLRequestContextGetter> auth_request_context_;
  SessionRestoreStrategy restore_strategy_;
  SessionRestoreState state_;

  bool loading_reported_;

  scoped_ptr<OAuth2TokenFetcher> oauth2_token_fetcher_;
  scoped_ptr<OAuth2LoginVerifier> login_verifier_;

  // OAuth2 refresh token.
  std::string refresh_token_;

  // Authorization code for fetching OAuth2 tokens.
  std::string auth_code_;

  // Session restore start time.
  base::Time session_restore_start_;

  // List of observers to notify when token availability changes.
  // Makes sure list is empty on destruction.
  // TODO(zelidrag|gspencer): Figure out how to get rid of ProfileHelper so we
  // can change the line below to ObserverList<Observer, true>.
  ObserverList<Observer, false> observer_list_;

  DISALLOW_COPY_AND_ASSIGN(OAuth2LoginManager);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_OAUTH2_LOGIN_MANAGER_H_
