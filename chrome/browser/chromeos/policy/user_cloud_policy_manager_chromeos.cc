// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/user_cloud_policy_manager_chromeos.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/metrics/sparse_histogram.h"
#include "base/sequenced_task_runner.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/policy/policy_oauth2_token_fetcher.h"
#include "chrome/browser/chromeos/policy/user_cloud_policy_manager_factory_chromeos.h"
#include "chrome/browser/chromeos/policy/wildcard_login_checker.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/common/chrome_content_client.h"
#include "components/policy/core/common/cloud/cloud_external_data_manager.h"
#include "components/policy/core/common/cloud/cloud_policy_refresh_scheduler.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/cloud/system_policy_request_context.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/core/common/policy_types.h"
#include "components/user_manager/user_manager.h"
#include "net/url_request/url_request_context_getter.h"
#include "policy/policy_constants.h"
#include "url/gurl.h"

namespace em = enterprise_management;

namespace policy {

namespace {

// UMA histogram names.
const char kUMADelayInitialization[] =
    "Enterprise.UserPolicyChromeOS.DelayInitialization";
const char kUMAInitialFetchClientError[] =
    "Enterprise.UserPolicyChromeOS.InitialFetch.ClientError";
const char kUMAInitialFetchDelayClientRegister[] =
    "Enterprise.UserPolicyChromeOS.InitialFetch.DelayClientRegister";
const char kUMAInitialFetchDelayOAuth2Token[] =
    "Enterprise.UserPolicyChromeOS.InitialFetch.DelayOAuth2Token";
const char kUMAInitialFetchDelayPolicyFetch[] =
    "Enterprise.UserPolicyChromeOS.InitialFetch.DelayPolicyFetch";
const char kUMAInitialFetchDelayTotal[] =
    "Enterprise.UserPolicyChromeOS.InitialFetch.DelayTotal";
const char kUMAInitialFetchOAuth2Error[] =
    "Enterprise.UserPolicyChromeOS.InitialFetch.OAuth2Error";
const char kUMAInitialFetchOAuth2NetworkError[] =
    "Enterprise.UserPolicyChromeOS.InitialFetch.OAuth2NetworkError";

void OnWildcardCheckCompleted(const std::string& username,
                              WildcardLoginChecker::Result result) {
  if (result == WildcardLoginChecker::RESULT_BLOCKED) {
    LOG(ERROR) << "Online wildcard login check failed, terminating session.";

    // TODO(mnissler): This only removes the user pod from the login screen, but
    // the cryptohome remains. This is because deleting the cryptohome for a
    // logged-in session is not possible. Fix this either by delaying the
    // cryptohome deletion operation or by getting rid of the in-session
    // wildcard check.
    user_manager::UserManager::Get()->RemoveUserFromList(username);
    chrome::AttemptUserExit();
  }
}

}  // namespace

UserCloudPolicyManagerChromeOS::UserCloudPolicyManagerChromeOS(
    scoped_ptr<CloudPolicyStore> store,
    scoped_ptr<CloudExternalDataManager> external_data_manager,
    const base::FilePath& component_policy_cache_path,
    bool wait_for_policy_fetch,
    base::TimeDelta initial_policy_fetch_timeout,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    const scoped_refptr<base::SequencedTaskRunner>& file_task_runner,
    const scoped_refptr<base::SequencedTaskRunner>& io_task_runner)
    : CloudPolicyManager(
          PolicyNamespaceKey(dm_protocol::kChromeUserPolicyType, std::string()),
          store.get(),
          task_runner,
          file_task_runner,
          io_task_runner),
      store_(store.Pass()),
      external_data_manager_(external_data_manager.Pass()),
      component_policy_cache_path_(component_policy_cache_path),
      wait_for_policy_fetch_(wait_for_policy_fetch),
      policy_fetch_timeout_(false, false) {
  time_init_started_ = base::Time::Now();
  if (wait_for_policy_fetch_ && !initial_policy_fetch_timeout.is_max()) {
    policy_fetch_timeout_.Start(
        FROM_HERE,
        initial_policy_fetch_timeout,
        base::Bind(&UserCloudPolicyManagerChromeOS::OnBlockingFetchTimeout,
                   base::Unretained(this)));
  }
}

UserCloudPolicyManagerChromeOS::~UserCloudPolicyManagerChromeOS() {}

void UserCloudPolicyManagerChromeOS::Connect(
    PrefService* local_state,
    DeviceManagementService* device_management_service,
    scoped_refptr<net::URLRequestContextGetter> system_request_context,
    UserAffiliation user_affiliation) {
  DCHECK(device_management_service);
  DCHECK(local_state);
  local_state_ = local_state;
  scoped_refptr<net::URLRequestContextGetter> request_context;
  if (system_request_context.get()) {
    // |system_request_context| can be null for tests.
    // Use the system request context here instead of a context derived
    // from the Profile because Connect() is called before the profile is
    // fully initialized (required so we can perform the initial policy load).
    // TODO(atwilson): Change this to use a UserPolicyRequestContext once
    // Connect() is called after profile initialization. http://crbug.com/323591
    request_context = new SystemPolicyRequestContext(
        system_request_context, GetUserAgent());
  }
  scoped_ptr<CloudPolicyClient> cloud_policy_client(
      new CloudPolicyClient(std::string(), std::string(),
                            kPolicyVerificationKeyHash, user_affiliation,
                            NULL, device_management_service,
                            request_context));
  core()->Connect(cloud_policy_client.Pass());
  client()->AddObserver(this);

  external_data_manager_->Connect(request_context);

  CreateComponentCloudPolicyService(component_policy_cache_path_,
                                    request_context);

  // Determine the next step after the CloudPolicyService initializes.
  if (service()->IsInitializationComplete()) {
    OnInitializationCompleted(service());
  } else {
    service()->AddObserver(this);
  }
}

void UserCloudPolicyManagerChromeOS::OnAccessTokenAvailable(
    const std::string& access_token) {
  access_token_ = access_token;

  if (!wildcard_username_.empty()) {
    wildcard_login_checker_.reset(new WildcardLoginChecker());
    wildcard_login_checker_->StartWithAccessToken(
        access_token,
        base::Bind(&OnWildcardCheckCompleted, wildcard_username_));
  }

  if (service() && service()->IsInitializationComplete() &&
      client() && !client()->is_registered()) {
    OnOAuth2PolicyTokenFetched(
        access_token, GoogleServiceAuthError(GoogleServiceAuthError::NONE));
  }
}

bool UserCloudPolicyManagerChromeOS::IsClientRegistered() const {
  return client() && client()->is_registered();
}

void UserCloudPolicyManagerChromeOS::EnableWildcardLoginCheck(
    const std::string& username) {
  DCHECK(access_token_.empty());
  wildcard_username_ = username;
}

void UserCloudPolicyManagerChromeOS::Shutdown() {
  if (client())
    client()->RemoveObserver(this);
  if (service())
    service()->RemoveObserver(this);
  token_fetcher_.reset();
  external_data_manager_->Disconnect();
  CloudPolicyManager::Shutdown();
}

bool UserCloudPolicyManagerChromeOS::IsInitializationComplete(
    PolicyDomain domain) const {
  if (!CloudPolicyManager::IsInitializationComplete(domain))
    return false;
  if (domain == POLICY_DOMAIN_CHROME)
    return !wait_for_policy_fetch_;
  return true;
}

void UserCloudPolicyManagerChromeOS::OnInitializationCompleted(
    CloudPolicyService* cloud_policy_service) {
  DCHECK_EQ(service(), cloud_policy_service);
  cloud_policy_service->RemoveObserver(this);

  time_init_completed_ = base::Time::Now();
  UMA_HISTOGRAM_MEDIUM_TIMES(kUMADelayInitialization,
                             time_init_completed_ - time_init_started_);

  // If the CloudPolicyClient isn't registered at this stage then it needs an
  // OAuth token for the initial registration.
  //
  // If |wait_for_policy_fetch_| is true then Profile initialization is blocking
  // on the initial policy fetch, so the token must be fetched immediately.
  // In that case, the signin Profile is used to authenticate a Gaia request to
  // fetch a refresh token, and then the policy token is fetched.
  //
  // If |wait_for_policy_fetch_| is false then the UserCloudPolicyTokenForwarder
  // service will eventually call OnAccessTokenAvailable() once an access token
  // is available. That call may have already happened while waiting for
  // initialization of the CloudPolicyService, so in that case check if an
  // access token is already available.
  if (!client()->is_registered()) {
    if (wait_for_policy_fetch_) {
      FetchPolicyOAuthTokenUsingSigninProfile();
    } else if (!access_token_.empty()) {
      OnAccessTokenAvailable(access_token_);
    }
  }

  if (!wait_for_policy_fetch_) {
    // If this isn't blocking on a policy fetch then
    // CloudPolicyManager::OnStoreLoaded() already published the cached policy.
    // Start the refresh scheduler now, which will eventually refresh the
    // cached policy or make the first fetch once the OAuth2 token is
    // available.
    StartRefreshSchedulerIfReady();
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
    time_client_registered_ = base::Time::Now();
    if (!time_token_available_.is_null()) {
      UMA_HISTOGRAM_MEDIUM_TIMES(
          kUMAInitialFetchDelayClientRegister,
          time_client_registered_ - time_token_available_);
    }

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
  if (wait_for_policy_fetch_) {
    UMA_HISTOGRAM_SPARSE_SLOWLY(kUMAInitialFetchClientError,
                                cloud_policy_client->status());
  }
  CancelWaitForPolicyFetch();
}

void UserCloudPolicyManagerChromeOS::OnComponentCloudPolicyUpdated() {
  CloudPolicyManager::OnComponentCloudPolicyUpdated();
  StartRefreshSchedulerIfReady();
}

void UserCloudPolicyManagerChromeOS::GetChromePolicy(PolicyMap* policy_map) {
  CloudPolicyManager::GetChromePolicy(policy_map);

  // If the store has a verified policy blob received from the server then apply
  // the defaults for policies that haven't been configured by the administrator
  // given that this is an enterprise user.
  if (!store()->has_policy())
    return;
  SetEnterpriseUsersDefaults(policy_map);
}

void UserCloudPolicyManagerChromeOS::FetchPolicyOAuthTokenUsingSigninProfile() {
  scoped_refptr<net::URLRequestContextGetter> signin_context;
  Profile* signin_profile = chromeos::ProfileHelper::GetSigninProfile();
  if (signin_profile)
    signin_context = signin_profile->GetRequestContext();
  if (!signin_context.get()) {
    LOG(ERROR) << "No signin Profile for policy oauth token fetch!";
    OnOAuth2PolicyTokenFetched(
        std::string(), GoogleServiceAuthError(GoogleServiceAuthError::NONE));
    return;
  }

  token_fetcher_.reset(new PolicyOAuth2TokenFetcher(
      signin_context.get(),
      g_browser_process->system_request_context(),
      base::Bind(&UserCloudPolicyManagerChromeOS::OnOAuth2PolicyTokenFetched,
                 base::Unretained(this))));
  token_fetcher_->Start();
}

void UserCloudPolicyManagerChromeOS::OnOAuth2PolicyTokenFetched(
    const std::string& policy_token,
    const GoogleServiceAuthError& error) {
  DCHECK(!client()->is_registered());
  time_token_available_ = base::Time::Now();
  if (wait_for_policy_fetch_) {
    UMA_HISTOGRAM_MEDIUM_TIMES(kUMAInitialFetchDelayOAuth2Token,
                               time_token_available_ - time_init_completed_);
  }

  if (error.state() == GoogleServiceAuthError::NONE) {
    // Start client registration. Either OnRegistrationStateChanged() or
    // OnClientError() will be called back.
    client()->Register(em::DeviceRegisterRequest::USER, policy_token,
                       std::string(), false, std::string(), std::string());
  } else {
    // Failed to get a token, stop waiting and use an empty policy.
    CancelWaitForPolicyFetch();

    UMA_HISTOGRAM_ENUMERATION(kUMAInitialFetchOAuth2Error,
                              error.state(),
                              GoogleServiceAuthError::NUM_STATES);
    if (error.state() == GoogleServiceAuthError::CONNECTION_FAILED) {
      UMA_HISTOGRAM_SPARSE_SLOWLY(kUMAInitialFetchOAuth2NetworkError,
                                  error.network_error());
    }
  }

  token_fetcher_.reset();
}

void UserCloudPolicyManagerChromeOS::OnInitialPolicyFetchComplete(
    bool success) {
  const base::Time now = base::Time::Now();
  UMA_HISTOGRAM_MEDIUM_TIMES(kUMAInitialFetchDelayPolicyFetch,
                             now - time_client_registered_);
  UMA_HISTOGRAM_MEDIUM_TIMES(kUMAInitialFetchDelayTotal,
                             now - time_init_started_);
  CancelWaitForPolicyFetch();
}

void UserCloudPolicyManagerChromeOS::OnBlockingFetchTimeout() {
  if (!wait_for_policy_fetch_)
    return;
  LOG(WARNING) << "Timed out while waiting for the initial policy fetch. "
               << "The first session will start without policy.";
  CancelWaitForPolicyFetch();
}

void UserCloudPolicyManagerChromeOS::CancelWaitForPolicyFetch() {
  if (!wait_for_policy_fetch_)
    return;

  wait_for_policy_fetch_ = false;
  policy_fetch_timeout_.Stop();
  CheckAndPublishPolicy();
  // Now that |wait_for_policy_fetch_| is guaranteed to be false, the scheduler
  // can be started.
  StartRefreshSchedulerIfReady();
}

void UserCloudPolicyManagerChromeOS::StartRefreshSchedulerIfReady() {
  if (core()->refresh_scheduler())
    return;  // Already started.

  if (wait_for_policy_fetch_)
    return;  // Still waiting for the initial, blocking fetch.

  if (!service() || !local_state_)
    return;  // Not connected.

  if (component_policy_service() &&
      !component_policy_service()->is_initialized()) {
    // If the client doesn't have the list of components to fetch yet then don't
    // start the scheduler. The |component_policy_service_| will call back into
    // OnComponentCloudPolicyUpdated() once it's ready.
    return;
  }

  core()->StartRefreshScheduler();
  core()->TrackRefreshDelayPref(local_state_,
                                policy_prefs::kUserPolicyRefreshRate);
}

}  // namespace policy
