// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_USER_CLOUD_POLICY_MANAGER_CHROMEOS_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_USER_CLOUD_POLICY_MANAGER_CHROMEOS_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/policy/cloud/cloud_policy_client.h"
#include "chrome/browser/policy/cloud/cloud_policy_constants.h"
#include "chrome/browser/policy/cloud/cloud_policy_manager.h"
#include "chrome/browser/policy/cloud/cloud_policy_service.h"
#include "chrome/browser/policy/cloud/component_cloud_policy_service.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "google_apis/gaia/gaia_auth_consumer.h"

class PrefService;

namespace net {
class URLRequestContextGetter;
}

namespace policy {

class DeviceManagementService;
class ResourceCache;
class PolicyOAuth2TokenFetcher;

// UserCloudPolicyManagerChromeOS implements logic for initializing user policy
// on Chrome OS.
class UserCloudPolicyManagerChromeOS
    : public CloudPolicyManager,
      public CloudPolicyClient::Observer,
      public CloudPolicyService::Observer,
      public ComponentCloudPolicyService::Delegate,
      public ProfileKeyedService {
 public:
  // If |wait_for_policy_fetch| is true, IsInitializationComplete() will return
  // false as long as there hasn't been a successful policy fetch.
  UserCloudPolicyManagerChromeOS(
      scoped_ptr<CloudPolicyStore> store,
      scoped_ptr<ResourceCache> resource_cache,
      bool wait_for_policy_fetch);
  virtual ~UserCloudPolicyManagerChromeOS();

  // Initializes the cloud connection. |local_state| and
  // |device_management_service| must stay valid until this object is deleted.
  void Connect(PrefService* local_state,
               DeviceManagementService* device_management_service,
               scoped_refptr<net::URLRequestContextGetter> request_context,
               UserAffiliation user_affiliation);

  // The OAuth2 login |refresh_token| can be used to obtain a policy OAuth2
  // token, if the CloudPolicyClient isn't registered yet.
  void OnRefreshTokenAvailable(const std::string& refresh_token);

  // Returns true if the underlying CloudPolicyClient is already registered.
  bool IsClientRegistered() const;

  // Returns the OAuth2 tokens obtained by the manager for the initial
  // registration, if it had to perform a blocking policy fetch.
  const GaiaAuthConsumer::ClientOAuthResult& oauth2_tokens() const {
    return oauth2_login_tokens_;
  }

  // ConfigurationPolicyProvider:
  virtual void Shutdown() OVERRIDE;
  virtual bool IsInitializationComplete(PolicyDomain domain) const OVERRIDE;
  virtual void RegisterPolicyDomain(
      PolicyDomain domain,
      const std::set<std::string>& component_ids) OVERRIDE;

  // CloudPolicyManager:
  virtual scoped_ptr<PolicyBundle> CreatePolicyBundle() OVERRIDE;

  // CloudPolicyService::Observer:
  virtual void OnInitializationCompleted(CloudPolicyService* service) OVERRIDE;

  // CloudPolicyClient::Observer:
  virtual void OnPolicyFetched(CloudPolicyClient* client) OVERRIDE;
  virtual void OnRegistrationStateChanged(CloudPolicyClient* client) OVERRIDE;
  virtual void OnClientError(CloudPolicyClient* client) OVERRIDE;

  // ComponentCloudPolicyService::Delegate:
  virtual void OnComponentCloudPolicyRefreshNeeded() OVERRIDE;
  virtual void OnComponentCloudPolicyUpdated() OVERRIDE;

 private:
  // These methods fetch a policy token using either the authentication context
  // of the signin Profile or a refresh token passed in OnRefreshTokenAvailable.
  // OnOAuth2PolicyTokenFetched is called back when the policy token is fetched.
  void FetchPolicyOAuthTokenUsingSigninProfile();
  void FetchPolicyOAuthTokenUsingRefreshToken();
  void OnOAuth2PolicyTokenFetched(const std::string& policy_token);

  // Completion handler for the explicit policy fetch triggered on startup in
  // case |wait_for_policy_fetch_| is true. |success| is true if the fetch was
  // successful.
  void OnInitialPolicyFetchComplete(bool success);

  // Cancels waiting for the policy fetch and flags the
  // ConfigurationPolicyProvider ready (assuming all other initialization tasks
  // have completed).
  void CancelWaitForPolicyFetch();

  void StartRefreshScheduler();

  // Owns the store, note that CloudPolicyManager just keeps a plain pointer.
  scoped_ptr<CloudPolicyStore> store_;

  // Handles fetching and storing cloud policy for components. It uses the
  // |store_|, so destroy it first.
  scoped_ptr<ComponentCloudPolicyService> component_policy_service_;

  // Whether to wait for a policy fetch to complete before reporting
  // IsInitializationComplete().
  bool wait_for_policy_fetch_;

  // The pref service to pass to the refresh scheduler on initialization.
  PrefService* local_state_;

  // Used to fetch the policy OAuth token, when necessary. This object holds
  // a callback with an unretained reference to the manager, when it exists.
  scoped_ptr<PolicyOAuth2TokenFetcher> token_fetcher_;

  // The OAuth2 login tokens fetched by the |token_fetcher_|, which can be
  // retrieved using oauth2_tokens().
  GaiaAuthConsumer::ClientOAuthResult oauth2_login_tokens_;

  DISALLOW_COPY_AND_ASSIGN(UserCloudPolicyManagerChromeOS);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_USER_CLOUD_POLICY_MANAGER_CHROMEOS_H_
