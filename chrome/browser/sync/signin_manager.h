// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The signin manager encapsulates some functionality tracking
// which user is signed in. When a user is signed in, a ClientLogin
// request is run on their behalf. Auth tokens are fetched from Google
// and the results are stored in the TokenService.

#ifndef CHROME_BROWSER_SYNC_SIGNIN_MANAGER_H_
#define CHROME_BROWSER_SYNC_SIGNIN_MANAGER_H_
#pragma once

#include <string>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/net/gaia/gaia_oauth_fetcher.h"
#include "chrome/common/net/gaia/gaia_auth_consumer.h"
#include "chrome/common/net/gaia/google_service_auth_error.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class GaiaAuthFetcher;
class Profile;
class PrefService;

// Details for the Notification type GOOGLE_SIGNIN_SUCCESSFUL.
// A listener might use this to make note of a username / password
// pair for encryption keys.
struct GoogleServiceSigninSuccessDetails {
  GoogleServiceSigninSuccessDetails(const std::string& in_username,
                                    const std::string& in_password)
      : username(in_username),
        password(in_password) {}
  std::string username;
  std::string password;
};

class SigninManager : public GaiaAuthConsumer,
                      public GaiaOAuthConsumer,
                      public content::NotificationObserver {
 public:
  SigninManager();
  virtual ~SigninManager();

  // Call to register our prefs.
  static void RegisterUserPrefs(PrefService* user_prefs);

  // If user was signed in, load tokens from DB if available.
  void Initialize(Profile* profile);
  bool IsInitialized() const;

  // If a user is signed in, this will return their name.
  // Otherwise, it will return an empty string.
  const std::string& GetUsername();

  // Sets the user name.  Used for migrating credentials from previous system.
  void SetUsername(const std::string& username);

  // Attempt to sign in this user with OAuth. If successful, set a preference
  // indicating the signed in user and send out a notification, then start
  // fetching tokens for the user.
  virtual void StartOAuthSignIn(const std::string& oauth1_request_token);

  // Attempt to sign in this user with ClientLogin. If successful, set a
  // preference indicating the signed in user and send out a notification,
  // then start fetching tokens for the user.
  // This is overridden for test subclasses that don't want to issue auth
  // requests.
  virtual void StartSignIn(const std::string& username,
                           const std::string& password,
                           const std::string& login_token,
                           const std::string& login_captcha);

  // Used when a second factor access code was required to complete a signin
  // attempt.
  void ProvideSecondFactorAccessCode(const std::string& access_code);

  // Sign a user out, removing the preference, erasing all keys
  // associated with the user, and canceling all auth in progress.
  void SignOut();

  // Called when a new request to re-authenticate a user is in progress.
  // Will clear in memory data but leaves the db as such so when the browser
  // restarts we can use the old token(which might throw a password error).
  void ClearInMemoryData();

  // GaiaAuthConsumer
  virtual void OnClientLoginSuccess(const ClientLoginResult& result) OVERRIDE;
  virtual void OnClientLoginFailure(
      const GoogleServiceAuthError& error) OVERRIDE;
  virtual void OnGetUserInfoSuccess(const std::string& key,
                                    const std::string& value) OVERRIDE;
  virtual void OnGetUserInfoKeyNotFound(const std::string& key) OVERRIDE;
  virtual void OnGetUserInfoFailure(
      const GoogleServiceAuthError& error) OVERRIDE;
  virtual void OnTokenAuthFailure(const GoogleServiceAuthError& error) OVERRIDE;

  // GaiaOAuthConsumer
  virtual void OnOAuthGetAccessTokenSuccess(const std::string& token,
                                            const std::string& secret) OVERRIDE;
  virtual void OnOAuthGetAccessTokenFailure(
      const GoogleServiceAuthError& error) OVERRIDE;
  virtual void OnOAuthWrapBridgeSuccess(const std::string& service_name,
                                        const std::string& token,
                                        const std::string& expires_in) OVERRIDE;
  virtual void OnOAuthWrapBridgeFailure(
      const std::string& service_name,
      const GoogleServiceAuthError& error) OVERRIDE;
  virtual void OnUserInfoSuccess(const std::string& email) OVERRIDE;
  virtual void OnUserInfoFailure(const GoogleServiceAuthError& error) OVERRIDE;

  // content::NotificationObserver
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  void PrepareForSignin();
  void PrepareForOAuthSignin();

  Profile* profile_;

  // ClientLogin identity.
  std::string username_;
  std::string password_;  // This is kept empty whenever possible.
  bool had_two_factor_error_;

  // OAuth identity.
  std::string oauth_username_;
  std::string oauth1_request_token_;

  void CleanupNotificationRegistration();

  // Result of the last client login, kept pending the lookup of the
  // canonical email.
  ClientLoginResult last_result_;

  // Actual client login handler.
  scoped_ptr<GaiaAuthFetcher> client_login_;

  // Actual OAuth login handler.
  scoped_ptr<GaiaOAuthFetcher> oauth_login_;

  // Register for notifications from the TokenService.
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(SigninManager);
};

#endif  // CHROME_BROWSER_SYNC_SIGNIN_MANAGER_H_
