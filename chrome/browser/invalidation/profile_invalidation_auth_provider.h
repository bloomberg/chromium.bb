// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INVALIDATION_PROFILE_INVALIDATION_AUTH_PROVIDER_H_
#define CHROME_BROWSER_INVALIDATION_PROFILE_INVALIDATION_AUTH_PROVIDER_H_

#include "base/macros.h"
#include "chrome/browser/invalidation/invalidation_auth_provider.h"
#include "chrome/browser/signin/signin_manager_base.h"

class LoginUIService;
class ProfileOAuth2TokenService;

namespace invalidation {

// An authentication provider implementation that's backed by
// ProfileOAuth2TokenService and SigninManager.
class ProfileInvalidationAuthProvider : public InvalidationAuthProvider,
                                        public SigninManagerBase::Observer {
 public:
  ProfileInvalidationAuthProvider(SigninManagerBase* signin_manager,
                                  ProfileOAuth2TokenService* token_service,
                                  LoginUIService* login_ui_service);
  virtual ~ProfileInvalidationAuthProvider();

  // InvalidationAuthProvider:
  virtual std::string GetAccountId() OVERRIDE;
  virtual OAuth2TokenService* GetTokenService() OVERRIDE;
  virtual bool ShowLoginUI() OVERRIDE;

  // SigninManagerBase::Observer:
  virtual void GoogleSignedOut(const std::string& username) OVERRIDE;

 private:
  SigninManagerBase* const signin_manager_;
  ProfileOAuth2TokenService* const token_service_;
  LoginUIService* const login_ui_service_;

  DISALLOW_COPY_AND_ASSIGN(ProfileInvalidationAuthProvider);
};

}  // namespace invalidation

#endif  // CHROME_BROWSER_INVALIDATION_PROFILE_INVALIDATION_AUTH_PROVIDER_H_
