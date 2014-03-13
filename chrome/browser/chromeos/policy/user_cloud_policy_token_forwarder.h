// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_USER_CLOUD_POLICY_TOKEN_FORWARDER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_USER_CLOUD_POLICY_TOKEN_FORWARDER_H_

#include "base/basictypes.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/policy/core/common/cloud/cloud_policy_service.h"
#include "google_apis/gaia/oauth2_token_service.h"

class ProfileOAuth2TokenService;
class SigninManagerBase;

namespace policy {

class UserCloudPolicyManagerChromeOS;

// A PKS that observes a ProfileOAuth2TokenService and mints the policy access
// token for the UserCloudPolicyManagerChromeOS, when the token service becomes
// ready. This service decouples the UserCloudPolicyManagerChromeOS from
// depending directly on the ProfileOAuth2TokenService, since it is initialized
// much earlier.
class UserCloudPolicyTokenForwarder : public KeyedService,
                                      public OAuth2TokenService::Observer,
                                      public OAuth2TokenService::Consumer,
                                      public CloudPolicyService::Observer {
 public:
  // The factory of this PKS depends on the factories of these two arguments,
  // so this object will be Shutdown() first and these pointers can be used
  // until that point.
  UserCloudPolicyTokenForwarder(UserCloudPolicyManagerChromeOS* manager,
                                ProfileOAuth2TokenService* token_service,
                                SigninManagerBase* signin_manager);
  virtual ~UserCloudPolicyTokenForwarder();

  // KeyedService:
  virtual void Shutdown() OVERRIDE;

  // OAuth2TokenService::Observer:
  virtual void OnRefreshTokenAvailable(const std::string& account_id) OVERRIDE;

  // OAuth2TokenService::Consumer:
  virtual void OnGetTokenSuccess(const OAuth2TokenService::Request* request,
                                 const std::string& access_token,
                                 const base::Time& expiration_time) OVERRIDE;
  virtual void OnGetTokenFailure(const OAuth2TokenService::Request* request,
                                 const GoogleServiceAuthError& error) OVERRIDE;

  // CloudPolicyService::Observer:
  virtual void OnInitializationCompleted(CloudPolicyService* service) OVERRIDE;

 private:
  void Initialize();

  void RequestAccessToken();

  UserCloudPolicyManagerChromeOS* manager_;
  ProfileOAuth2TokenService* token_service_;
  SigninManagerBase* signin_manager_;
  scoped_ptr<OAuth2TokenService::Request> request_;

  DISALLOW_COPY_AND_ASSIGN(UserCloudPolicyTokenForwarder);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_USER_CLOUD_POLICY_TOKEN_FORWARDER_H_
