// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_SIGNIN_OAUTH_HELPER_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_SIGNIN_OAUTH_HELPER_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "google_apis/gaia/gaia_auth_consumer.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"

// Retrieves the OAuth2 information from an already signed in cookie jar.
// The information retrieved is: username, refresh token.
class SigninOAuthHelper : public GaiaAuthConsumer {
 public:
  // Implemented by users of SigninOAuthHelper to know then helper is finished.
  class Consumer {
   public:
    virtual ~Consumer() {}

    // Called when all the information is retrieved successfully.  |email|
    // and |display_email| correspond to the gaia properties called "email"
    // and "displayEmail" associated with the signed in account. |refresh_token|
    // is the account's login-scoped oauth2 refresh token.
    virtual void OnSigninOAuthInformationAvailable(
        const std::string& email,
        const std::string& display_email,
        const std::string& refresh_token) {}

    // Called when an error occurs while getting the information.
    virtual void OnSigninOAuthInformationFailure(
        const GoogleServiceAuthError& error) {}
  };

  explicit SigninOAuthHelper(net::URLRequestContextGetter* getter,
                             const std::string& session_index,
                             const std::string& signin_scoped_device_id,
                             Consumer* consumer);
  virtual ~SigninOAuthHelper();

 private:
  // Overridden from GaiaAuthConsumer.
  virtual void OnClientOAuthSuccess(const ClientOAuthResult& result) OVERRIDE;
  virtual void OnClientOAuthFailure(const GoogleServiceAuthError& error)
      OVERRIDE;
  virtual void OnClientLoginSuccess(const ClientLoginResult& result) OVERRIDE;
  virtual void OnClientLoginFailure(const GoogleServiceAuthError& error)
      OVERRIDE;
  virtual void OnGetUserInfoSuccess(const UserInfoMap& data) OVERRIDE;
  virtual void OnGetUserInfoFailure(const GoogleServiceAuthError& error)
      OVERRIDE;

  GaiaAuthFetcher gaia_auth_fetcher_;
  std::string refresh_token_;
  Consumer* consumer_;

  DISALLOW_COPY_AND_ASSIGN(SigninOAuthHelper);
};

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_SIGNIN_OAUTH_HELPER_H_
