// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CLOUD_USER_POLICY_SIGNIN_SERVICE_ANDROID_H_
#define CHROME_BROWSER_POLICY_CLOUD_USER_POLICY_SIGNIN_SERVICE_ANDROID_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service_base.h"

class AndroidProfileOAuth2TokenService;
class Profile;

namespace policy {

class CloudPolicyClientRegistrationHelper;

// A specialization of the UserPolicySigninServiceBase for Android.
class UserPolicySigninService : public UserPolicySigninServiceBase {
 public:
  // Creates a UserPolicySigninService associated with the passed |profile|.
  UserPolicySigninService(Profile* profile,
                          PrefService* local_state,
                          DeviceManagementService* device_management_service,
                          AndroidProfileOAuth2TokenService* token_service);
  virtual ~UserPolicySigninService();

  // Registers a CloudPolicyClient for fetching policy for |username|.
  // This requests an OAuth2 token for the services involved, and contacts
  // the policy service if the account has management enabled.
  // |callback| is invoked once the CloudPolicyClient is ready to fetch policy,
  // or once it is determined that |username| is not a managed account.
  void RegisterPolicyClient(const std::string& username,
                            const PolicyRegistrationCallback& callback);

 private:
  void CallPolicyRegistrationCallback(scoped_ptr<CloudPolicyClient> client,
                                      PolicyRegistrationCallback callback);

  // BrowserContextKeyedService implementation:
  virtual void Shutdown() OVERRIDE;

  // CloudPolicyService::Observer implementation:
  virtual void OnInitializationCompleted(CloudPolicyService* service) OVERRIDE;

  // Registers for cloud policy for an already signed-in user.
  void RegisterCloudPolicyService();

  // Cancels a pending cloud policy registration attempt.
  void CancelPendingRegistration();

  void OnRegistrationDone();

  scoped_ptr<CloudPolicyClientRegistrationHelper> registration_helper_;
  base::WeakPtrFactory<UserPolicySigninService> weak_factory_;

  // Weak pointer to the token service used to authenticate the
  // CloudPolicyClient during registration.
  AndroidProfileOAuth2TokenService* oauth2_token_service_;

  DISALLOW_COPY_AND_ASSIGN(UserPolicySigninService);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CLOUD_USER_POLICY_SIGNIN_SERVICE_ANDROID_H_
