// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_USER_CLOUD_POLICY_MANAGER_CHROMEOS_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_USER_CLOUD_POLICY_MANAGER_CHROMEOS_H_

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_manager.h"
#include "components/policy/core/common/cloud/cloud_policy_service.h"

class GoogleServiceAuthError;
class PrefService;

namespace base {
class SequencedTaskRunner;
}

namespace net {
class URLRequestContextGetter;
}

namespace policy {

class CloudExternalDataManager;
class DeviceManagementService;
class PolicyOAuth2TokenFetcher;
class WildcardLoginChecker;

// Implements logic for initializing user policy on Chrome OS.
class UserCloudPolicyManagerChromeOS : public CloudPolicyManager,
                                       public CloudPolicyClient::Observer,
                                       public CloudPolicyService::Observer,
                                       public KeyedService {
 public:
  // If |wait_for_policy_fetch| is true, IsInitializationComplete() will return
  // false as long as there hasn't been a successful policy fetch.
  // |task_runner| is the runner for policy refresh tasks.
  // |file_task_runner| is used for file operations. Currently this must be the
  // FILE BrowserThread.
  // |io_task_runner| is used for network IO. Currently this must be the IO
  // BrowserThread.
  UserCloudPolicyManagerChromeOS(
      std::unique_ptr<CloudPolicyStore> store,
      std::unique_ptr<CloudExternalDataManager> external_data_manager,
      const base::FilePath& component_policy_cache_path,
      bool wait_for_policy_fetch,
      base::TimeDelta initial_policy_fetch_timeout,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner,
      const scoped_refptr<base::SequencedTaskRunner>& file_task_runner,
      const scoped_refptr<base::SequencedTaskRunner>& io_task_runner);
  ~UserCloudPolicyManagerChromeOS() override;

  // Initializes the cloud connection. |local_state| and
  // |device_management_service| must stay valid until this object is deleted.
  void Connect(
      PrefService* local_state,
      DeviceManagementService* device_management_service,
      scoped_refptr<net::URLRequestContextGetter> system_request_context);

  // This class is one of the policy providers, and must be ready for the
  // creation of the Profile's PrefService; all the other KeyedServices depend
  // on the PrefService, so this class can't depend on other
  // BrowserContextKeyedServices to avoid a circular dependency. So instead of
  // using the ProfileOAuth2TokenService directly to get the access token, a 3rd
  // service (UserCloudPolicyTokenForwarder) will fetch it later and pass it to
  // this method once available.
  // The |access_token| can then be used to authenticate the registration
  // request to the DMServer.
  void OnAccessTokenAvailable(const std::string& access_token);

  // Returns true if the underlying CloudPolicyClient is already registered.
  bool IsClientRegistered() const;

  // Indicates a wildcard login check should be performed once an access token
  // is available.
  void EnableWildcardLoginCheck(const std::string& username);

  // ConfigurationPolicyProvider:
  void Shutdown() override;
  bool IsInitializationComplete(PolicyDomain domain) const override;

  // CloudPolicyService::Observer:
  void OnInitializationCompleted(CloudPolicyService* service) override;

  // CloudPolicyClient::Observer:
  void OnPolicyFetched(CloudPolicyClient* client) override;
  void OnRegistrationStateChanged(CloudPolicyClient* client) override;
  void OnClientError(CloudPolicyClient* client) override;

  // ComponentCloudPolicyService::Delegate:
  void OnComponentCloudPolicyUpdated() override;

  // CloudPolicyManager:
  void OnStoreLoaded(CloudPolicyStore* cloud_policy_store) override;

  // Helper function to force a policy fetch timeout.
  void ForceTimeoutForTest();

 protected:
  // CloudPolicyManager:
  void GetChromePolicy(PolicyMap* policy_map) override;

 private:
  // Fetches a policy token using the refresh token if available, or the
  // authentication context of the signin context, and calls back
  // OnOAuth2PolicyTokenFetched when done.
  void FetchPolicyOAuthToken();

  // Called once the policy access token is available, and starts the
  // registration with the policy server if the token was successfully fetched.
  void OnOAuth2PolicyTokenFetched(const std::string& policy_token,
                                  const GoogleServiceAuthError& error);

  // Completion handler for the explicit policy fetch triggered on startup in
  // case |wait_for_policy_fetch_| is true. |success| is true if the fetch was
  // successful.
  void OnInitialPolicyFetchComplete(bool success);

  // Called when |policy_fetch_timeout_| times out, to cancel the blocking
  // wait for the initial policy fetch.
  void OnBlockingFetchTimeout();

  // Cancels waiting for the policy fetch and flags the
  // ConfigurationPolicyProvider ready (assuming all other initialization tasks
  // have completed). Pass |true| if policy fetch was successful (either
  // because policy was successfully fetched, or if DMServer has notified us
  // that the user is not managed).
  void CancelWaitForPolicyFetch(bool success);

  void StartRefreshSchedulerIfReady();

  // Owns the store, note that CloudPolicyManager just keeps a plain pointer.
  std::unique_ptr<CloudPolicyStore> store_;

  // Manages external data referenced by policies.
  std::unique_ptr<CloudExternalDataManager> external_data_manager_;

  // Username for the wildcard login check if applicable, empty otherwise.
  std::string wildcard_username_;

  // Path where policy for components will be cached.
  base::FilePath component_policy_cache_path_;

  // Whether to wait for a policy fetch to complete before reporting
  // IsInitializationComplete().
  bool wait_for_policy_fetch_;

  // Whether we should allow policy fetches to fail, or wait forever until they
  // succeed (typically we won't allow them to fail until we have loaded policy
  // at least once).
  bool allow_failed_policy_fetches_;

  // A timer that puts a hard limit on the maximum time to wait for the initial
  // policy fetch.
  base::Timer policy_fetch_timeout_;

  // The pref service to pass to the refresh scheduler on initialization.
  PrefService* local_state_;

  // Used to fetch the policy OAuth token, when necessary. This object holds
  // a callback with an unretained reference to the manager, when it exists.
  std::unique_ptr<PolicyOAuth2TokenFetcher> token_fetcher_;

  // Keeps alive the wildcard checker while its running.
  std::unique_ptr<WildcardLoginChecker> wildcard_login_checker_;

  // The access token passed to OnAccessTokenAvailable. It is stored here so
  // that it can be used if OnInitializationCompleted is called later.
  std::string access_token_;

  // Timestamps for collecting timing UMA stats.
  base::Time time_init_started_;
  base::Time time_init_completed_;
  base::Time time_token_available_;
  base::Time time_client_registered_;

  DISALLOW_COPY_AND_ASSIGN(UserCloudPolicyManagerChromeOS);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_USER_CLOUD_POLICY_MANAGER_CHROMEOS_H_
