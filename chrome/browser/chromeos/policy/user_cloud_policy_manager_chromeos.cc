// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/user_cloud_policy_manager_chromeos.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/policy/policy_oauth2_token_fetcher.h"
#include "chrome/browser/chromeos/policy/user_cloud_policy_manager_factory_chromeos.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/policy/cloud/cloud_policy_refresh_scheduler.h"
#include "chrome/browser/policy/cloud/resource_cache.h"
#include "chrome/browser/policy/policy_bundle.h"
#include "chrome/common/pref_names.h"
#include "net/url_request/url_request_context_getter.h"

namespace em = enterprise_management;

namespace policy {

UserCloudPolicyManagerChromeOS::UserCloudPolicyManagerChromeOS(
    scoped_ptr<CloudPolicyStore> store,
    scoped_ptr<ResourceCache> resource_cache,
    bool wait_for_policy_fetch)
    : CloudPolicyManager(
          PolicyNamespaceKey(dm_protocol::kChromeUserPolicyType, std::string()),
          store.get()),
      store_(store.Pass()),
      wait_for_policy_fetch_(wait_for_policy_fetch) {
  if (resource_cache) {
    component_policy_service_.reset(new ComponentCloudPolicyService(
        this, store_.get(), resource_cache.Pass()));
  }
}

UserCloudPolicyManagerChromeOS::~UserCloudPolicyManagerChromeOS() {}

void UserCloudPolicyManagerChromeOS::Connect(
    PrefService* local_state,
    DeviceManagementService* device_management_service,
    scoped_refptr<net::URLRequestContextGetter> request_context,
    UserAffiliation user_affiliation) {
  DCHECK(device_management_service);
  DCHECK(local_state);
  local_state_ = local_state;
  scoped_ptr<CloudPolicyClient> cloud_policy_client(
      new CloudPolicyClient(std::string(), std::string(), user_affiliation,
                            NULL, device_management_service));
  core()->Connect(cloud_policy_client.Pass());
  client()->AddObserver(this);

  if (component_policy_service_)
    component_policy_service_->Connect(client(), request_context);

  // Determine the next step after the CloudPolicyService initializes.
  if (service()->IsInitializationComplete()) {
    OnInitializationCompleted(service());
  } else {
    service()->AddObserver(this);
  }
}

void UserCloudPolicyManagerChromeOS::OnRefreshTokenAvailable(
    const std::string& refresh_token) {
  oauth2_login_tokens_.refresh_token = refresh_token;
  if (!oauth2_login_tokens_.refresh_token.empty() &&
      service() && service()->IsInitializationComplete() &&
      client() && !client()->is_registered()) {
    FetchPolicyOAuthTokenUsingRefreshToken();
  }
}

bool UserCloudPolicyManagerChromeOS::IsClientRegistered() const {
  return client() && client()->is_registered();
}

void UserCloudPolicyManagerChromeOS::Shutdown() {
  if (client())
    client()->RemoveObserver(this);
  if (service())
    service()->RemoveObserver(this);
  token_fetcher_.reset();
  component_policy_service_.reset();
  CloudPolicyManager::Shutdown();
}

bool UserCloudPolicyManagerChromeOS::IsInitializationComplete(
    PolicyDomain domain) const {
  if (!CloudPolicyManager::IsInitializationComplete(domain))
    return false;
  if (domain == POLICY_DOMAIN_CHROME)
    return !wait_for_policy_fetch_;
  if (ComponentCloudPolicyService::SupportsDomain(domain) &&
      component_policy_service_) {
    return component_policy_service_->is_initialized();
  }
  return true;
}

void UserCloudPolicyManagerChromeOS::RegisterPolicyDomain(
    PolicyDomain domain,
    const std::set<std::string>& component_ids) {
  if (ComponentCloudPolicyService::SupportsDomain(domain) &&
      component_policy_service_) {
    component_policy_service_->RegisterPolicyDomain(domain, component_ids);
  }
}

scoped_ptr<PolicyBundle> UserCloudPolicyManagerChromeOS::CreatePolicyBundle() {
  scoped_ptr<PolicyBundle> bundle = CloudPolicyManager::CreatePolicyBundle();
  if (component_policy_service_)
    bundle->MergeFrom(component_policy_service_->policy());
  return bundle.Pass();
}

void UserCloudPolicyManagerChromeOS::OnInitializationCompleted(
    CloudPolicyService* cloud_policy_service) {
  DCHECK_EQ(service(), cloud_policy_service);
  cloud_policy_service->RemoveObserver(this);
  // If the CloudPolicyClient isn't registered at this stage then it needs an
  // OAuth token for the initial registration.
  //
  // If |wait_for_policy_fetch_| is true then Profile initialization is blocking
  // on the initial policy fetch, so the token must be fetched immediately.
  // In that case, the signin Profile is used to authenticate a Gaia request to
  // fetch a refresh token, and then the policy token is fetched.
  //
  // If |wait_for_policy_fetch_| is false then the UserCloudPolicyTokenForwarder
  // service will eventually call OnRefreshTokenAvailable() once the
  // TokenService has one. That call may have already happened while waiting for
  // initialization of the CloudPolicyService, so in that case check if a
  // refresh token is already available.
  if (!client()->is_registered()) {
    if (wait_for_policy_fetch_) {
      FetchPolicyOAuthTokenUsingSigninProfile();
    } else if (!oauth2_login_tokens_.refresh_token.empty()) {
      FetchPolicyOAuthTokenUsingRefreshToken();
    }
  }

  if (!wait_for_policy_fetch_) {
    // If this isn't blocking on a policy fetch then
    // CloudPolicyManager::OnStoreLoaded() already published the cached policy.
    // Start the refresh scheduler now, which will eventually refresh the
    // cached policy or make the first fetch once the OAuth2 token is
    // available.
    StartRefreshScheduler();
  }
}

void UserCloudPolicyManagerChromeOS::OnPolicyFetched(
    CloudPolicyClient* client) {
  // No action required. If we're blocked on a policy fetch, we'll learn about
  // completion of it through OnInitialPolicyFetchComplete().
}

void UserCloudPolicyManagerChromeOS::OnRegistrationStateChanged(
    CloudPolicyClient* cloud_policy_client) {
  DCHECK_EQ(client(), cloud_policy_client);
  if (wait_for_policy_fetch_) {
    // If we're blocked on the policy fetch, now is a good time to issue it.
    if (client()->is_registered()) {
      service()->RefreshPolicy(
          base::Bind(
              &UserCloudPolicyManagerChromeOS::OnInitialPolicyFetchComplete,
              base::Unretained(this)));
    } else {
      // If the client has switched to not registered, we bail out as this
      // indicates the cloud policy setup flow has been aborted.
      CancelWaitForPolicyFetch();
    }
  }
}

void UserCloudPolicyManagerChromeOS::OnClientError(
    CloudPolicyClient* cloud_policy_client) {
  DCHECK_EQ(client(), cloud_policy_client);
  CancelWaitForPolicyFetch();
}

void UserCloudPolicyManagerChromeOS::OnComponentCloudPolicyRefreshNeeded() {
  core()->RefreshSoon();
}

void UserCloudPolicyManagerChromeOS::OnComponentCloudPolicyUpdated() {
  CheckAndPublishPolicy();
  StartRefreshScheduler();
}

void UserCloudPolicyManagerChromeOS::FetchPolicyOAuthTokenUsingSigninProfile() {
  scoped_refptr<net::URLRequestContextGetter> signin_context;
  Profile* signin_profile = chromeos::ProfileHelper::GetSigninProfile();
  if (signin_profile)
    signin_context = signin_profile->GetRequestContext();
  if (!signin_context) {
    LOG(ERROR) << "No signin Profile for policy oauth token fetch!";
    OnOAuth2PolicyTokenFetched(std::string());
    return;
  }

  token_fetcher_.reset(new PolicyOAuth2TokenFetcher(
      signin_context,
      g_browser_process->system_request_context(),
      base::Bind(&UserCloudPolicyManagerChromeOS::OnOAuth2PolicyTokenFetched,
                 base::Unretained(this))));
  token_fetcher_->Start();
}

void UserCloudPolicyManagerChromeOS::FetchPolicyOAuthTokenUsingRefreshToken() {
  token_fetcher_.reset(new PolicyOAuth2TokenFetcher(
      g_browser_process->system_request_context(),
      oauth2_login_tokens_.refresh_token,
      base::Bind(&UserCloudPolicyManagerChromeOS::OnOAuth2PolicyTokenFetched,
                 base::Unretained(this))));
  token_fetcher_->Start();
}

void UserCloudPolicyManagerChromeOS::OnOAuth2PolicyTokenFetched(
    const std::string& policy_token) {
  DCHECK(!client()->is_registered());
  // The TokenService will reuse the refresh token fetched by the
  // |token_fetcher_|, if it fetched one.
  if (token_fetcher_->has_oauth2_tokens())
    oauth2_login_tokens_ = token_fetcher_->oauth2_tokens();

  if (policy_token.empty()) {
    // Failed to get a token, stop waiting and use an empty policy.
    CancelWaitForPolicyFetch();
  } else {
    // Start client registration. Either OnRegistrationStateChanged() or
    // OnClientError() will be called back.
    client()->Register(em::DeviceRegisterRequest::USER,
                       policy_token, std::string(), false);
  }

  token_fetcher_.reset();
}

void UserCloudPolicyManagerChromeOS::OnInitialPolicyFetchComplete(
    bool success) {
  CancelWaitForPolicyFetch();
}

void UserCloudPolicyManagerChromeOS::CancelWaitForPolicyFetch() {
  wait_for_policy_fetch_ = false;
  CheckAndPublishPolicy();
  // Now that |wait_for_policy_fetch_| is guaranteed to be false, the scheduler
  // can be started.
  StartRefreshScheduler();
}

void UserCloudPolicyManagerChromeOS::StartRefreshScheduler() {
  if (core()->refresh_scheduler())
    return;  // Already started.

  if (wait_for_policy_fetch_)
    return;  // Still waiting for the initial, blocking fetch.

  if (!service() || !local_state_)
    return;  // Not connected.

  if (component_policy_service_ &&
      !component_policy_service_->is_initialized()) {
    // If the client doesn't have the list of components to fetch yet then don't
    // start the scheduler. The |component_policy_service_| will call back into
    // OnComponentCloudPolicyUpdated() once it's ready.
    return;
  }

  core()->StartRefreshScheduler();
  core()->TrackRefreshDelayPref(local_state_, prefs::kUserPolicyRefreshRate);
}

}  // namespace policy
