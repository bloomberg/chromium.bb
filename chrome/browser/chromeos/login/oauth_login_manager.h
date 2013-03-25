// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_OAUTH_LOGIN_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_OAUTH_LOGIN_MANAGER_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "net/url_request/url_request_context_getter.h"

class Profile;

namespace chromeos {

// This class is responsible for restoring authenticated web sessions out of
// OAuth tokens or vice versa.
class OAuthLoginManager {
 public:
  enum SessionRestoreState {
    // Session restore is not started.
    SESSION_RESTORE_NOT_STARTED,
    // Session restore is in progress. We are currently issuing calls to verify
    // stored OAuth tokens and populate cookie jar with GAIA credentials.
    SESSION_RESTORE_IN_PROGRESS,
    // Session restore is completed.
    SESSION_RESTORE_DONE,
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

  class Delegate {
   public:
    virtual ~Delegate() {}

    // Raised when merge session is completed.
    virtual void OnCompletedMergeSession() = 0;

    // Raised when cookie jar authentication is successfully completed.
    virtual void OnCompletedAuthentication(Profile* user_profile) = 0;

    // Raised when stored OAuth(1|2) tokens are found and authentication
    // profile is no longer needed.
    virtual void OnFoundStoredTokens() = 0;

    // Raised when policy tokens are retrieved.
    virtual void OnRestoredPolicyTokens() {}
  };

  // Factory method.
  static OAuthLoginManager* Create(OAuthLoginManager::Delegate* delegate);

  explicit OAuthLoginManager(OAuthLoginManager::Delegate* delegate);
  virtual ~OAuthLoginManager();

  // Starts the process of retrieving policy tokens.
  virtual void RestorePolicyTokens(
      net::URLRequestContextGetter* auth_request_context) = 0;

  // Restores and verifies OAuth tokens either following specified
  // |restore_strategy|. For |restore_strategy| with values
  // RESTORE_FROM_PASSED_OAUTH2_REFRESH_TOKEN or
  // RESTORE_FROM_AUTH_CODE, respectively
  // parameters |oauth2_refresh_token| or |auth_code| need to have non-empty
  // value.
  virtual void RestoreSession(
      Profile* user_profile,
      net::URLRequestContextGetter* auth_request_context,
      SessionRestoreStrategy restore_strategy,
      const std::string& oauth2_refresh_token,
      const std::string& auth_code) = 0;

  // Continues session restore after transient network errors.
  virtual void ContinueSessionRestore() = 0;

  // Stops all background authentication requests.
  virtual void Stop() = 0;

  // Returns session restore state.
  SessionRestoreState state() { return state_; }

 protected:
  // Signals delegate that authentication is completed, kicks off token fetching
  // process in TokenService.
  void CompleteAuthentication();

  OAuthLoginManager::Delegate* delegate_;
  Profile* user_profile_;
  scoped_refptr<net::URLRequestContextGetter> auth_request_context_;
  SessionRestoreStrategy restore_strategy_;
  SessionRestoreState state_;

  DISALLOW_COPY_AND_ASSIGN(OAuthLoginManager);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_OAUTH_LOGIN_MANAGER_H_
