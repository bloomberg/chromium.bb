// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_PROFILE_IDENTITY_PROVIDER_H_
#define CHROME_BROWSER_SIGNIN_PROFILE_IDENTITY_PROVIDER_H_

#include "base/macros.h"
#include "components/signin/core/browser/signin_manager_base.h"
#include "google_apis/gaia/identity_provider.h"

class LoginUIService;
class ProfileOAuth2TokenService;

// An identity provider implementation that's backed by
// ProfileOAuth2TokenService and SigninManager.
class ProfileIdentityProvider : public IdentityProvider,
                                public SigninManagerBase::Observer {
 public:
  ProfileIdentityProvider(SigninManagerBase* signin_manager,
                          ProfileOAuth2TokenService* token_service,
                          LoginUIService* login_ui_service);
  virtual ~ProfileIdentityProvider();

  // IdentityProvider:
  virtual std::string GetActiveUsername() override;
  virtual std::string GetActiveAccountId() override;
  virtual OAuth2TokenService* GetTokenService() override;
  virtual bool RequestLogin() override;

  // SigninManagerBase::Observer:
  virtual void GoogleSigninSucceeded(const std::string& account_id,
                                     const std::string& username,
                                     const std::string& password) override;
  virtual void GoogleSignedOut(const std::string& account_id,
                               const std::string& username) override;

 private:
  SigninManagerBase* const signin_manager_;
  ProfileOAuth2TokenService* const token_service_;
  LoginUIService* const login_ui_service_;

  DISALLOW_COPY_AND_ASSIGN(ProfileIdentityProvider);
};

#endif  // CHROME_BROWSER_SIGNIN_PROFILE_IDENTITY_PROVIDER_H_
