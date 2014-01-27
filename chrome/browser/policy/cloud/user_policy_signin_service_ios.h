// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CLOUD_USER_POLICY_SIGNIN_SERVICE_IOS_H_
#define CHROME_BROWSER_POLICY_CLOUD_USER_POLICY_SIGNIN_SERVICE_IOS_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service_base.h"

class ProfileOAuth2TokenService;
class Profile;

namespace net {
class URLRequestContextGetter;
}

namespace policy {

class CloudPolicyClientRegistrationHelper;

typedef void (^PolicyRegistrationBlockCallback)(const std::string& dm_token,
                                                const std::string& client_id);

typedef void (^PolicyFetchBlockCallback)(bool succeeded);

// A specialization of the UserPolicySigninServiceBase for iOS.
// TODO(joaodasilva): share more code with UserPolicySigninServiceBase and
// the Android implementation. http://crbug.com/273055
class UserPolicySigninService : public UserPolicySigninServiceBase {
 public:
  // Creates a UserPolicySigninService associated with the passed |profile|.
  UserPolicySigninService(
      Profile* profile,
      PrefService* local_state,
      DeviceManagementService* device_management_service,
      UserCloudPolicyManager* policy_manager,
      SigninManager* signin_manager,
      scoped_refptr<net::URLRequestContextGetter> system_request_context,
      ProfileOAuth2TokenService* token_service);
  virtual ~UserPolicySigninService();

  // Registers a CloudPolicyClient for fetching policy for |username|.
  // This requests an OAuth2 token for the services involved, and contacts
  // the policy service if the account has management enabled.
  // |callback| is invoked once we have registered this device to fetch policy,
  // or once it is determined that |username| is not a managed account.
  void RegisterForPolicy(const std::string& username,
                         PolicyRegistrationBlockCallback callback);

  // Wrapper for FetchPolicyForSignedInUser that uses a block instead of
  // a base::Callback.
  void FetchPolicy(
      const std::string& username,
      const std::string& dm_token,
      const std::string& client_id,
      scoped_refptr<net::URLRequestContextGetter> profile_request_context,
      PolicyFetchBlockCallback callback);

 private:
  void CallPolicyRegistrationCallback(scoped_ptr<CloudPolicyClient> client,
                                      PolicyRegistrationBlockCallback callback);

  void CallPolicyFetchCallback(PolicyFetchBlockCallback callback,
                               bool succeeded);

  // BrowserContextKeyedService implementation:
  virtual void Shutdown() OVERRIDE;

  // CloudPolicyService::Observer implementation:
  virtual void OnInitializationCompleted(CloudPolicyService* service) OVERRIDE;

  // Cancels a pending cloud policy registration attempt.
  void CancelPendingRegistration();

  scoped_ptr<CloudPolicyClientRegistrationHelper> registration_helper_;
  base::WeakPtrFactory<UserPolicySigninService> weak_factory_;

  // Weak pointer to the token service used to authenticate the
  // CloudPolicyClient during registration.
  ProfileOAuth2TokenService* oauth2_token_service_;

  // The PrefService associated with the profile.
  PrefService* profile_prefs_;

  DISALLOW_COPY_AND_ASSIGN(UserPolicySigninService);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CLOUD_USER_POLICY_SIGNIN_SERVICE_IOS_H_
