// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_FORCE_SIGNIN_VERIFIER_H_
#define CHROME_BROWSER_SIGNIN_FORCE_SIGNIN_VERIFIER_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "google_apis/gaia/oauth2_token_service.h"
#include "net/base/backoff_entry.h"
#include "net/base/network_change_notifier.h"

class ForcedReauthenticationDialog;
class Profile;
class SigninManager;

// ForceSigninVerifier will verify profile's auth token when profile is loaded
// into memory by the first time via gaia server. It will retry on any transient
// error.
class ForceSigninVerifier
    : public OAuth2TokenService::Consumer,
      public net::NetworkChangeNotifier::NetworkChangeObserver {
 public:
  explicit ForceSigninVerifier(Profile* profile);
  ~ForceSigninVerifier() override;

  // override OAuth2TokenService::Consumer
  void OnGetTokenSuccess(const OAuth2TokenService::Request* request,
                         const std::string& access_token,
                         const base::Time& expiration_time) override;
  void OnGetTokenFailure(const OAuth2TokenService::Request* request,
                         const GoogleServiceAuthError& error) override;

  // override net::NetworkChangeNotifier::NetworkChangeObserver
  void OnNetworkChanged(
      net::NetworkChangeNotifier::ConnectionType type) override;

  // Cancel any pending or ongoing verification.
  void Cancel();

  // Return the value of |has_token_verified_|.
  bool HasTokenBeenVerified();

  // Abort signout countdown.
  void AbortSignoutCountdownIfExisted();

 protected:
  // Send the token verification request. The request will be sent only if
  //   - The token has never been verified before.
  //   - There is no on going verification.
  //   - There is network connection.
  //   - The profile has signed in.
  //
  void SendRequest();

  virtual bool ShouldSendRequest();

  // Show the warning dialog before signing out user and closing associated
  // browser window.
  virtual void ShowDialog();

  // Start the window closing countdown, return the duration.
  base::TimeDelta StartCountdown();

  OAuth2TokenService::Request* GetRequestForTesting();
  net::BackoffEntry* GetBackoffEntryForTesting();
  base::OneShotTimer* GetOneShotTimerForTesting();
  base::OneShotTimer* GetWindowCloseTimerForTesting();

 private:
  void CloseAllBrowserWindows();

  std::unique_ptr<OAuth2TokenService::Request> access_token_request_;

#if !defined(OS_MACOSX)
  Profile* profile_;

  std::unique_ptr<ForcedReauthenticationDialog> dialog_;
#endif

  // Indicates whether the verification is finished successfully or with a
  // persistent error.
  bool has_token_verified_;
  net::BackoffEntry backoff_entry_;
  base::OneShotTimer backoff_request_timer_;

  OAuth2TokenService* oauth2_token_service_;
  SigninManager* signin_manager_;

  base::Time token_request_time_;

  // The countdown of window closing.
  base::OneShotTimer window_close_timer_;

  DISALLOW_COPY_AND_ASSIGN(ForceSigninVerifier);
};

#endif  // CHROME_BROWSER_SIGNIN_FORCE_SIGNIN_VERIFIER_H_
