// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CLOUD_USER_POLICY_SIGNIN_SERVICE_H_
#define CHROME_BROWSER_POLICY_CLOUD_USER_POLICY_SIGNIN_SERVICE_H_

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/policy/cloud/cloud_policy_service.h"
#include "chrome/browser/policy/cloud/user_info_fetcher.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class Profile;

namespace base {
class Time;
}

namespace policy {

class CloudPolicyClientRegistrationHelper;
class CloudPolicyClient;
class UserCloudPolicyManager;

// The UserPolicySigninService is responsible for interacting with the policy
// infrastructure (mainly UserCloudPolicyManager) to load policy for the signed
// in user.
//
// At signin time, this class initializes the UCPM and loads policy before any
// other signed in services are initialized. After each restart, this class
// ensures that the CloudPolicyClient is registered (in case the policy server
// was offline during the initial policy fetch) and if not it initiates a fresh
// registration process.
//
// Finally, if the user signs out, this class is responsible for shutting down
// the policy infrastructure to ensure that any cached policy is cleared.
class UserPolicySigninService
    : public ProfileKeyedService,
      public CloudPolicyClient::Observer,
      public CloudPolicyService::Observer,
      public content::NotificationObserver {
 public:
  // The callback invoked once policy registration is complete. Passed
  // CloudPolicyClient parameter is null if DMToken fetch failed.
  typedef base::Callback<void(scoped_ptr<CloudPolicyClient>)>
      PolicyRegistrationCallback;

  // The callback invoked once policy fetch is complete. Passed boolean
  // parameter is set to true if the policy fetch succeeded.
  typedef base::Callback<void(bool)> PolicyFetchCallback;

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

  // Initiates a policy fetch as part of user signin, using a CloudPolicyClient
  // previously initialized via RegisterPolicyClient. |callback| is invoked
  // once the policy fetch is complete, passing true if the policy fetch
  // succeeded.
  void FetchPolicyForSignedInUser(scoped_ptr<CloudPolicyClient> client,
                                  const PolicyFetchCallback& callback);

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // CloudPolicyService::Observer implementation.
  virtual void OnInitializationCompleted(CloudPolicyService* service) OVERRIDE;

  // CloudPolicyClient::Observer implementation.
  virtual void OnPolicyFetched(CloudPolicyClient* client) OVERRIDE;
  virtual void OnRegistrationStateChanged(CloudPolicyClient* client) OVERRIDE;
  virtual void OnClientError(CloudPolicyClient* client) OVERRIDE;

  // ProfileKeyedService implementation:
  virtual void Shutdown() OVERRIDE;

 private:
  // Returns false if cloud policy is disabled or if the passed |email_address|
  // is definitely not from a hosted domain (according to the blacklist in
  // BrowserPolicyConnector::IsNonEnterpriseUser()).
  bool ShouldLoadPolicyForUser(const std::string& email_address);

  // Initializes the UserCloudPolicyManager using the passed CloudPolicyClient.
  void InitializeUserCloudPolicyManager(scoped_ptr<CloudPolicyClient> client);

  // Initializes the UserCloudPolicyManager with policy for the currently
  // signed-in user.
  void InitializeForSignedInUser();

  // Fetches an OAuth token to allow the cloud policy service to register with
  // the cloud policy server. |oauth_login_token| should contain an OAuth login
  // refresh token that can be downscoped to get an access token for the
  // device_management service.
  void RegisterCloudPolicyService(std::string oauth_login_token);

  // Callback invoked when policy registration has finished.
  void OnRegistrationComplete();

  // Helper routine which prohibits user signout if the user is registered for
  // cloud policy.
  void ProhibitSignoutIfNeeded();

  // Helper routines to (un)register for CloudPolicyService and
  // CloudPolicyClient notifications.
  void StartObserving();
  void StopObserving();

  // Shuts down the UserCloudPolicyManager (for example, after the user signs
  // out) and deletes any cached policy.
  void ShutdownUserCloudPolicyManager();

  // Invoked when a policy registration request is complete.
  void CallPolicyRegistrationCallback(scoped_ptr<CloudPolicyClient> client,
                                      PolicyRegistrationCallback callback);

  // Convenience helper to get the UserCloudPolicyManager for |profile_|.
  UserCloudPolicyManager* GetManager();

  // Weak pointer to the profile this service is associated with.
  Profile* profile_;

  content::NotificationRegistrar registrar_;

  scoped_ptr<CloudPolicyClientRegistrationHelper> registration_helper_;

  base::WeakPtrFactory<UserPolicySigninService> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(UserPolicySigninService);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CLOUD_USER_POLICY_SIGNIN_SERVICE_H_
