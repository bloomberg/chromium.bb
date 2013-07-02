// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CLOUD_USER_POLICY_SIGNIN_SERVICE_H_
#define CHROME_BROWSER_POLICY_CLOUD_USER_POLICY_SIGNIN_SERVICE_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service_base.h"

class Profile;

namespace policy {

class CloudPolicyClientRegistrationHelper;

// A specialization of the UserPolicySigninServiceBase for the desktop
// platforms (Windows, Mac and Linux).
class UserPolicySigninService : public UserPolicySigninServiceBase {
 public:
  // Creates a UserPolicySigninService associated with the passed |profile|.
  explicit UserPolicySigninService(Profile* profile);
  virtual ~UserPolicySigninService();

  // Registers a CloudPolicyClient for fetching policy for a user. The
  // |oauth2_login_token| and |username| are explicitly passed because
  // the user is not signed in yet (TokenService does not have any tokens yet
  // to prevent services from using it until after we've fetched policy).
  void RegisterPolicyClient(const std::string& username,
                            const std::string& oauth2_login_token,
                            const PolicyRegistrationCallback& callback);

  // content::NotificationObserver implementation:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // CloudPolicyService::Observer implementation:
  virtual void OnInitializationCompleted(CloudPolicyService* service) OVERRIDE;

  // BrowserContextKeyedService implementation:
  virtual void Shutdown() OVERRIDE;

  // UserPolicySigninServiceBase implementation:
  virtual void InitializeUserCloudPolicyManager(
      scoped_ptr<CloudPolicyClient> client) OVERRIDE;
  virtual void ShutdownUserCloudPolicyManager() OVERRIDE;

 private:
  // Fetches an OAuth token to allow the cloud policy service to register with
  // the cloud policy server. |oauth_login_token| should contain an OAuth login
  // refresh token that can be downscoped to get an access token for the
  // device_management service.
  void RegisterCloudPolicyService(const std::string& oauth_login_token);

  // Callback invoked when policy registration has finished.
  void OnRegistrationComplete();

  // Helper routine which prohibits user signout if the user is registered for
  // cloud policy.
  void ProhibitSignoutIfNeeded();

  // Invoked when a policy registration request is complete.
  void CallPolicyRegistrationCallback(scoped_ptr<CloudPolicyClient> client,
                                      PolicyRegistrationCallback callback);

  scoped_ptr<CloudPolicyClientRegistrationHelper> registration_helper_;

  DISALLOW_COPY_AND_ASSIGN(UserPolicySigninService);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CLOUD_USER_POLICY_SIGNIN_SERVICE_H_
