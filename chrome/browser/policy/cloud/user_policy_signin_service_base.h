// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CLOUD_USER_POLICY_SIGNIN_SERVICE_BASE_H_
#define CHROME_BROWSER_POLICY_CLOUD_USER_POLICY_SIGNIN_SERVICE_BASE_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/policy/cloud/cloud_policy_client.h"
#include "chrome/browser/policy/cloud/cloud_policy_service.h"
#include "components/browser_context_keyed_service/browser_context_keyed_service.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class PrefService;
class Profile;
class SigninManager;

namespace net {
class URLRequestContextGetter;
}

namespace policy {

class DeviceManagementService;
class UserCloudPolicyManager;

// The UserPolicySigninService is responsible for interacting with the policy
// infrastructure (mainly UserCloudPolicyManager) to load policy for the signed
// in user. This is the base class that contains shared behavior.
//
// At signin time, this class initializes the UCPM and loads policy before any
// other signed in services are initialized. After each restart, this class
// ensures that the CloudPolicyClient is registered (in case the policy server
// was offline during the initial policy fetch) and if not it initiates a fresh
// registration process.
//
// Finally, if the user signs out, this class is responsible for shutting down
// the policy infrastructure to ensure that any cached policy is cleared.
class UserPolicySigninServiceBase : public BrowserContextKeyedService,
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

  // Creates a UserPolicySigninServiceBase associated with the passed |profile|.
  UserPolicySigninServiceBase(
      Profile* profile,
      PrefService* local_state,
      scoped_refptr<net::URLRequestContextGetter> request_context,
      DeviceManagementService* device_management_service);
  virtual ~UserPolicySigninServiceBase();

  // Initiates a policy fetch as part of user signin, using a CloudPolicyClient
  // previously initialized via RegisterPolicyClient. |callback| is invoked
  // once the policy fetch is complete, passing true if the policy fetch
  // succeeded.
  void FetchPolicyForSignedInUser(scoped_ptr<CloudPolicyClient> client,
                                  const PolicyFetchCallback& callback);

  // content::NotificationObserver implementation:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // CloudPolicyService::Observer implementation:
  virtual void OnInitializationCompleted(CloudPolicyService* service) OVERRIDE;

  // CloudPolicyClient::Observer implementation:
  virtual void OnPolicyFetched(CloudPolicyClient* client) OVERRIDE;
  virtual void OnRegistrationStateChanged(CloudPolicyClient* client) OVERRIDE;
  virtual void OnClientError(CloudPolicyClient* client) OVERRIDE;

  // BrowserContextKeyedService implementation:
  virtual void Shutdown() OVERRIDE;

 protected:
  // Returns true if policy should be loaded even when Gaia reports that the
  // account doesn't have management enabled.
  static bool ShouldForceLoadPolicy();

  // Returns a CloudPolicyClient to perform a registration with the DM server,
  // or NULL if |username| shouldn't register for policy management.
  scoped_ptr<CloudPolicyClient> PrepareToRegister(const std::string& username);

  // Returns false if cloud policy is disabled or if the passed |email_address|
  // is definitely not from a hosted domain (according to the blacklist in
  // BrowserPolicyConnector::IsNonEnterpriseUser()).
  bool ShouldLoadPolicyForUser(const std::string& email_address);

  // Invoked to initialize the UserPolicySigninService once its owning Profile
  // becomes ready. If the Profile has a signed-in account associated with it
  // at startup then this initializes the cloud policy manager by calling
  // InitializeForSignedInUser(); otherwise it clears any stored policies.
  void InitializeOnProfileReady();

  // Invoked to initialize the cloud policy service for |username|, which is the
  // account associated with the Profile that owns this service. This is invoked
  // from InitializeOnProfileReady() if the Profile already has a signed-in
  // account at startup, or (on the desktop platforms) as soon as the user
  // signs-in and an OAuth2 login refresh token becomes available.
  void InitializeForSignedInUser(const std::string& username);

  // Initializes the cloud policy manager with the passed |client|. This is
  // called from InitializeForSignedInUser() when the Profile already has a
  // signed in account at startup, and from FetchPolicyForSignedInUser() during
  // the initial policy fetch after signing in.
  virtual void InitializeUserCloudPolicyManager(
      scoped_ptr<CloudPolicyClient> client);

  // Prepares for the UserCloudPolicyManager to be shutdown due to
  // user signout or profile destruction.
  virtual void PrepareForUserCloudPolicyManagerShutdown();

  // Shuts down the UserCloudPolicyManager (for example, after the user signs
  // out) and deletes any cached policy.
  virtual void ShutdownUserCloudPolicyManager();

  // Convenience helper to get the UserCloudPolicyManager for |profile_|.
  UserCloudPolicyManager* GetManager();

  Profile* profile() { return profile_; }
  content::NotificationRegistrar* registrar() { return &registrar_; }
  SigninManager* GetSigninManager();

 private:
  // Weak pointer to the profile this service is associated with.
  Profile* profile_;

  content::NotificationRegistrar registrar_;

  PrefService* local_state_;
  scoped_refptr<net::URLRequestContextGetter> request_context_;
  DeviceManagementService* device_management_service_;

  base::WeakPtrFactory<UserPolicySigninServiceBase> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(UserPolicySigninServiceBase);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CLOUD_USER_POLICY_SIGNIN_SERVICE_BASE_H_
