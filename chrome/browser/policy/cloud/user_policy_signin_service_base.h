// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CLOUD_USER_POLICY_SIGNIN_SERVICE_BASE_H_
#define CHROME_BROWSER_POLICY_CLOUD_USER_POLICY_SIGNIN_SERVICE_BASE_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class PrefService;
class Profile;

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
// At signin time, this class initializes the UserCloudPolicyManager and loads
// policy before any other signed in services are initialized. After each
// restart, this class ensures that the CloudPolicyClient is registered (in case
// the policy server was offline during the initial policy fetch) and if not it
// initiates a fresh registration process.
//
// Finally, if the user signs out, this class is responsible for shutting down
// the policy infrastructure to ensure that any cached policy is cleared.
class UserPolicySigninServiceBase : public KeyedService,
                                    public CloudPolicyClient::Observer,
                                    public CloudPolicyService::Observer,
                                    public content::NotificationObserver,
                                    public SigninManagerBase::Observer {
 public:
  // The callback invoked once policy registration is complete. Passed
  // |dm_token| and |client_id| parameters are empty if policy registration
  // failed.
  typedef base::Callback<void(const std::string& dm_token,
                              const std::string& client_id)>
      PolicyRegistrationCallback;

  // The callback invoked once policy fetch is complete. Passed boolean
  // parameter is set to true if the policy fetch succeeded.
  typedef base::Callback<void(bool)> PolicyFetchCallback;

  // Creates a UserPolicySigninServiceBase associated with the passed |profile|.
  UserPolicySigninServiceBase(
      Profile* profile,
      PrefService* local_state,
      DeviceManagementService* device_management_service,
      UserCloudPolicyManager* policy_manager,
      SigninManager* signin_manager,
      scoped_refptr<net::URLRequestContextGetter> system_request_context);
  ~UserPolicySigninServiceBase() override;

  // Initiates a policy fetch as part of user signin, using a |dm_token| and
  // |client_id| fetched via RegisterForPolicyXXX(). |callback| is invoked
  // once the policy fetch is complete, passing true if the policy fetch
  // succeeded.
  // Virtual for testing.
  virtual void FetchPolicyForSignedInUser(
      const std::string& username,
      const std::string& dm_token,
      const std::string& client_id,
      scoped_refptr<net::URLRequestContextGetter> profile_request_context,
      const PolicyFetchCallback& callback);

  // SigninManagerBase::Observer implementation:
  void GoogleSignedOut(const std::string& account_id,
                       const std::string& username) override;

  // content::NotificationObserver implementation:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // CloudPolicyService::Observer implementation:
  void OnInitializationCompleted(CloudPolicyService* service) override;

  // CloudPolicyClient::Observer implementation:
  void OnPolicyFetched(CloudPolicyClient* client) override;
  void OnRegistrationStateChanged(CloudPolicyClient* client) override;
  void OnClientError(CloudPolicyClient* client) override;

  // KeyedService implementation:
  void Shutdown() override;

  void SetSystemRequestContext(
      scoped_refptr<net::URLRequestContextGetter> request_context);

 protected:
  net::URLRequestContextGetter* system_request_context() {
    return system_request_context_.get();
  }

  // Returns a CloudPolicyClient to perform a registration with the DM server,
  // or NULL if |username| shouldn't register for policy management.
  std::unique_ptr<CloudPolicyClient> CreateClientForRegistrationOnly(
      const std::string& username);

  // Returns false if cloud policy is disabled or if the passed |email_address|
  // is definitely not from a hosted domain (according to the blacklist in
  // BrowserPolicyConnector::IsNonEnterpriseUser()).
  bool ShouldLoadPolicyForUser(const std::string& email_address);

  // Invoked to initialize the UserPolicySigninService once its owning Profile
  // becomes ready. If the Profile has a signed-in account associated with it
  // at startup then this initializes the cloud policy manager by calling
  // InitializeForSignedInUser(); otherwise it clears any stored policies.
  void InitializeOnProfileReady(Profile* profile);

  // Invoked to initialize the cloud policy service for |username|, which is the
  // account associated with the Profile that owns this service. This is invoked
  // from InitializeOnProfileReady() if the Profile already has a signed-in
  // account at startup, or (on the desktop platforms) as soon as the user
  // signs-in and an OAuth2 login refresh token becomes available.
  void InitializeForSignedInUser(
      const std::string& username,
      scoped_refptr<net::URLRequestContextGetter> profile_request_context);

  // Initializes the cloud policy manager with the passed |client|. This is
  // called from InitializeForSignedInUser() when the Profile already has a
  // signed in account at startup, and from FetchPolicyForSignedInUser() during
  // the initial policy fetch after signing in.
  virtual void InitializeUserCloudPolicyManager(
      const std::string& username,
      std::unique_ptr<CloudPolicyClient> client);

  // Prepares for the UserCloudPolicyManager to be shutdown due to
  // user signout or profile destruction.
  virtual void PrepareForUserCloudPolicyManagerShutdown();

  // Shuts down the UserCloudPolicyManager (for example, after the user signs
  // out) and deletes any cached policy.
  virtual void ShutdownUserCloudPolicyManager();

  // Convenience helpers to get the associated UserCloudPolicyManager and
  // SigninManager.
  UserCloudPolicyManager* policy_manager() { return policy_manager_; }
  SigninManager* signin_manager() { return signin_manager_; }

  content::NotificationRegistrar* registrar() { return &registrar_; }

 private:
  // Weak pointer to the UserCloudPolicyManager and SigninManager this service
  // is associated with.
  UserCloudPolicyManager* policy_manager_;
  SigninManager* signin_manager_;

  content::NotificationRegistrar registrar_;

  PrefService* local_state_;
  DeviceManagementService* device_management_service_;
  scoped_refptr<net::URLRequestContextGetter> system_request_context_;

  base::WeakPtrFactory<UserPolicySigninServiceBase> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(UserPolicySigninServiceBase);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CLOUD_USER_POLICY_SIGNIN_SERVICE_BASE_H_
