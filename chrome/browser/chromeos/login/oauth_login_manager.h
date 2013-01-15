// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_OAUTH_LOGIN_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_OAUTH_LOGIN_MANAGER_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/cros/cert_library.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/login/oauth1_login_verifier.h"
#include "chrome/browser/chromeos/login/oauth1_token_fetcher.h"
#include "chrome/browser/chromeos/login/oauth2_login_verifier.h"
#include "chrome/browser/chromeos/login/oauth2_policy_fetcher.h"
#include "chrome/browser/chromeos/login/oauth2_token_fetcher.h"
#include "chrome/browser/chromeos/login/policy_oauth_fetcher.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "net/url_request/url_request_context_getter.h"

class GoogleServiceAuthError;
class Profile;
class TokenService;

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

  class Delegate {
   public:
    virtual ~Delegate() {}

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

  // Restores and verifies OAuth tokens either from TokenService or previously
  // authenticated cookie jar.
  virtual void RestoreSession(
      Profile* user_profile,
      net::URLRequestContextGetter* auth_request_context,
      bool restore_from_auth_cookies) = 0;

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
  bool restore_from_auth_cookies_;
  SessionRestoreState state_;

  DISALLOW_COPY_AND_ASSIGN(OAuthLoginManager);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_OAUTH_LOGIN_MANAGER_H_
