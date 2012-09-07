// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_ONLINE_ATTEMPT_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_ONLINE_ATTEMPT_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/login/login_status_consumer.h"
#include "chrome/browser/net/gaia/gaia_oauth_consumer.h"
#include "chrome/browser/net/gaia/gaia_oauth_fetcher.h"
#include "google_apis/gaia/gaia_auth_consumer.h"
#include "google_apis/gaia/google_service_auth_error.h"

class GaiaAuthFetcher;
class Profile;

namespace chromeos {
class AuthAttemptState;
class AuthAttemptStateResolver;

class OnlineAttempt
    : public GaiaAuthConsumer,
      public GaiaOAuthConsumer {
 public:
  OnlineAttempt(bool using_oauth,
                AuthAttemptState* current_attempt,
                AuthAttemptStateResolver* callback);
  virtual ~OnlineAttempt();

  // Initiate the online login attempt either through client or auth login.
  // Status will be recorded in |current_attempt|, and resolver_->Resolve() will
  // be called on the IO thread when useful state is available.
  // Must be called on the UI thread.
  void Initiate(Profile* auth_profile);

  // GaiaAuthConsumer overrides. Callbacks from GaiaAuthFetcher
  virtual void OnClientLoginFailure(
      const GoogleServiceAuthError& error) OVERRIDE;
  virtual void OnClientLoginSuccess(
      const GaiaAuthConsumer::ClientLoginResult& credentials) OVERRIDE;

  // GaiaOAuthConsumer overrides. Callbacks from GaiaOAuthFetcher.
  virtual void OnOAuthLoginSuccess(const std::string& sid,
                                   const std::string& lsid,
                                   const std::string& auth) OVERRIDE;
  virtual void OnOAuthLoginFailure(
      const GoogleServiceAuthError& error) OVERRIDE;

 private:
  FRIEND_TEST_ALL_PREFIXES(OnlineAttemptTest, LoginSuccess);
  FRIEND_TEST_ALL_PREFIXES(OnlineAttemptTest, TwoFactorSuccess);

  // Milliseconds until we timeout our attempt to hit ClientLogin.
  static const int kClientLoginTimeoutMs;

  void TryClientLogin();
  void CancelClientLogin();

  void TriggerResolve(const LoginFailure& outcome);

  bool HasPendingFetch();
  void CancelRequest();

  // True if we use GAIA extension to perform authentication.
  bool using_oauth_;

  AuthAttemptState* const attempt_;
  AuthAttemptStateResolver* const resolver_;

  // Handles ClientLogin communications with Gaia.
  scoped_ptr<GaiaAuthFetcher> client_fetcher_;
  // Handles OAuthLogin communications with Gaia.
  scoped_ptr<GaiaOAuthFetcher> oauth_fetcher_;

  // Used to cancel the CancelClientLogin closure.
  base::WeakPtrFactory<OnlineAttempt> weak_factory_;

  // Whether we're willing to re-try the ClientLogin attempt.
  bool try_again_;

  friend class OnlineAttemptTest;
  DISALLOW_COPY_AND_ASSIGN(OnlineAttempt);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_ONLINE_ATTEMPT_H_
