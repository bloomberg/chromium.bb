// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/user_cloud_policy_manager_chromeos.h"

#include <set>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/debug/crash_logging.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/sparse_histogram.h"
#include "base/sequenced_task_runner.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/session/user_session_manager.h"
#include "chrome/browser/chromeos/login/users/affiliation.h"
#include "chrome/browser/chromeos/login/users/chrome_user_manager_impl.h"
#include "chrome/browser/chromeos/policy/policy_oauth2_token_fetcher.h"
#include "chrome/browser/chromeos/policy/user_policy_manager_factory_chromeos.h"
#include "chrome/browser/chromeos/policy/wildcard_login_checker.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/crash_keys.h"
#include "chromeos/chromeos_switches.h"
#include "components/policy/core/common/cloud/cloud_external_data_manager.h"
#include "components/policy/core/common/cloud/cloud_policy_refresh_scheduler.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "net/url_request/url_request_context_getter.h"
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
    user_manager::UserManager::Get()->RemoveUserFromList(
        AccountId::FromUserEmail(username));
    chrome::AttemptUserExit();
  }
}

}  // namespace

UserCloudPolicyManagerChromeOS::UserCloudPolicyManagerChromeOS(
    std::unique_ptr<CloudPolicyStore> store,
    std::unique_ptr<CloudExternalDataManager> external_data_manager,
    const base::FilePath& component_policy_cache_path,
    base::TimeDelta initial_policy_fetch_timeout,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    const scoped_refptr<base::SequencedTaskRunner>& file_task_runner,
    const scoped_refptr<base::SequencedTaskRunner>& io_task_runner)
    : CloudPolicyManager(dm_protocol::kChromeUserPolicyType,
                         std::string(),
                         store.get(),
                         task_runner,
                         file_task_runner,
                         io_task_runner),
      store_(std::move(store)),
      external_data_manager_(std::move(external_data_manager)),
      component_policy_cache_path_(component_policy_cache_path),
      waiting_for_initial_policy_fetch_(
          !initial_policy_fetch_timeout.is_zero()) {
  time_init_started_ = base::Time::Now();

  initial_policy_fetch_may_fail_ =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kAllowFailedPolicyFetchForTest) ||
      !initial_policy_fetch_timeout.is_max();
  // No need to set the timer when the timeout is infinite.
  if (waiting_for_initial_policy_fetch_ && initial_policy_fetch_may_fail_) {
    policy_fetch_timeout_.Start(
        FROM_HERE,
        initial_policy_fetch_timeout,
        base::Bind(&UserCloudPolicyManagerChromeOS::OnBlockingFetchTimeout,
                   base::Unretained(this)));
  }
}

void UserCloudPolicyManagerChromeOS::ForceTimeoutForTest() {
  DCHECK(policy_fetch_timeout_.IsRunning());
  // Stop the timer to mimic what happens when a real timer fires, then invoke
  // the timer callback directly.
  policy_fetch_timeout_.Stop();
  OnBlockingFetchTimeout();
}

UserCloudPolicyManagerChromeOS::~UserCloudPolicyManagerChromeOS() {}

void UserCloudPolicyManagerChromeOS::Connect(
    PrefService* local_state,
    DeviceManagementService* device_management_service,
    scoped_refptr<net::URLRequestContextGetter> system_request_context) {
  DCHECK(device_management_service);
  DCHECK(local_state);

  // TODO(emaxx): Remove the crash key after the crashes tracked at
  // https://crbug.com/685996 are fixed.
  if (core()->client()) {
    base::debug::SetCrashKeyToStackTrace(
        crash_keys::kUserCloudPolicyManagerConnectTrace, connect_callstack_);
  } else {
    connect_callstack_ = base::debug::StackTrace();
  }
  CHECK(!core()->client());

  local_state_ = local_state;
  // Note: |system_request_context| can be null for tests.
  // Use the system request context here instead of a context derived
  // from the Profile because Connect() is called before the profile is
  // fully initialized (required so we can perform the initial policy load).
  std::unique_ptr<CloudPolicyClient> cloud_policy_client =
      base::MakeUnique<CloudPolicyClient>(
          std::string() /* machine_id */, std::string() /* machine_model */,
          device_management_service, system_request_context,
          nullptr /* signing_service */);
  CreateComponentCloudPolicyService(
      dm_protocol::kChromeExtensionPolicyType, component_policy_cache_path_,
      system_request_context, cloud_policy_client.get(), schema_registry());
  core()->Connect(std::move(cloud_policy_client));
  client()->AddObserver(this);

  external_data_manager_->Connect(system_request_context);

  // Determine the next step after the CloudPolicyService initializes.
  if (service()->IsInitializationComplete()) {
    OnInitializationCompleted(service());

    // The cloud policy client may be already registered by this point if the
    // store has already been loaded and contains a valid policy - the
    // registration setup in this case is performed by the CloudPolicyService
    // that is instantiated inside the CloudPolicyCore::Connect() method call.
    // If that's the case and |waiting_for_initial_policy_fetch_| is true, then
    // the policy fetch needs to be issued (it happens otherwise after the
    // client registration is finished, in OnRegistrationStateChanged()).
    if (client()->is_registered() && waiting_for_initial_policy_fetch_) {
      service()->RefreshPolicy(
          base::Bind(&UserCloudPolicyManagerChromeOS::CancelWaitForPolicyFetch,
                     base::Unretained(this)));
    }
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
    return !waiting_for_initial_policy_fetch_;
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
  // If |waiting_for_initial_policy_fetch_| is true then Profile initialization
  // is blocking on the initial policy fetch, so the token must be fetched
  // immediately. In that case, the signin Profile is used to authenticate a
  // Gaia request to fetch a refresh token, and then the policy token is
  // fetched.
  //
  // If |waiting_for_initial_policy_fetch_| is false then the
  // UserCloudPolicyTokenForwarder service will eventually call
  // OnAccessTokenAvailable() once an access token is available. That call may
  // have already happened while waiting for initialization of the
  // CloudPolicyService, so in that case check if an access token is already
  // available.
  if (!client()->is_registered()) {
    if (waiting_for_initial_policy_fetch_) {
      FetchPolicyOAuthToken();
    } else if (!access_token_.empty()) {
      OnAccessTokenAvailable(access_token_);
    }
  }

  if (!waiting_for_initial_policy_fetch_) {
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
  // completion of it through OnInitialPolicyFetchComplete(), or through the
  // CancelWaitForPolicyFetch() callback.
}

void UserCloudPolicyManagerChromeOS::OnRegistrationStateChanged(
    CloudPolicyClient* cloud_policy_client) {
  DCHECK_EQ(client(), cloud_policy_client);

  if (waiting_for_initial_policy_fetch_) {
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
      CancelWaitForPolicyFetch(true);
    }
  }
}

void UserCloudPolicyManagerChromeOS::OnClientError(
    CloudPolicyClient* cloud_policy_client) {
  DCHECK_EQ(client(), cloud_policy_client);
  if (waiting_for_initial_policy_fetch_) {
    UMA_HISTOGRAM_SPARSE_SLOWLY(kUMAInitialFetchClientError,
                                cloud_policy_client->status());
  }
  switch (client()->status()) {
    case DM_STATUS_SUCCESS:
    case DM_STATUS_SERVICE_MANAGEMENT_NOT_SUPPORTED:
      // If management is not supported for this user, then a registration
      // error is to be expected.
      CancelWaitForPolicyFetch(true);
      break;
    default:
      // Unexpected error fetching policy.
      CancelWaitForPolicyFetch(false);
      break;
  }
}

void UserCloudPolicyManagerChromeOS::OnComponentCloudPolicyUpdated() {
  CloudPolicyManager::OnComponentCloudPolicyUpdated();
  StartRefreshSchedulerIfReady();
}

void UserCloudPolicyManagerChromeOS::OnStoreLoaded(
    CloudPolicyStore* cloud_policy_store) {
  CloudPolicyManager::OnStoreLoaded(cloud_policy_store);

  em::PolicyData const* const policy_data = cloud_policy_store->policy();

  if (policy_data) {
    DCHECK(policy_data->has_username());
    chromeos::AffiliationIDSet set_of_user_affiliation_ids(
        policy_data->user_affiliation_ids().begin(),
        policy_data->user_affiliation_ids().end());

    chromeos::ChromeUserManager::Get()->SetUserAffiliation(
        policy_data->username(), set_of_user_affiliation_ids);
  }
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

void UserCloudPolicyManagerChromeOS::FetchPolicyOAuthToken() {
  // By-pass token fetching for test.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kDisableGaiaServices)) {
    OnOAuth2PolicyTokenFetched(
        "fake_policy_token",
        GoogleServiceAuthError(GoogleServiceAuthError::NONE));
    return;
  }

  const std::string& refresh_token = chromeos::UserSessionManager::GetInstance()
                                         ->user_context()
                                         .GetRefreshToken();
  if (!refresh_token.empty()) {
    token_fetcher_.reset(PolicyOAuth2TokenFetcher::CreateInstance());
    token_fetcher_->StartWithRefreshToken(
        refresh_token, g_browser_process->system_request_context(),
        base::Bind(&UserCloudPolicyManagerChromeOS::OnOAuth2PolicyTokenFetched,
                   base::Unretained(this)));
    return;
  }

  scoped_refptr<net::URLRequestContextGetter> signin_context =
      chromeos::login::GetSigninContext();
  if (!signin_context.get()) {
    LOG(ERROR) << "No signin context for policy oauth token fetch!";
    OnOAuth2PolicyTokenFetched(
        std::string(), GoogleServiceAuthError(GoogleServiceAuthError::NONE));
    return;
  }

  token_fetcher_.reset(PolicyOAuth2TokenFetcher::CreateInstance());
  token_fetcher_->StartWithSigninContext(
      signin_context.get(), g_browser_process->system_request_context(),
      base::Bind(&UserCloudPolicyManagerChromeOS::OnOAuth2PolicyTokenFetched,
                 base::Unretained(this)));
}

void UserCloudPolicyManagerChromeOS::OnOAuth2PolicyTokenFetched(
    const std::string& policy_token,
    const GoogleServiceAuthError& error) {
  DCHECK(!client()->is_registered());
  time_token_available_ = base::Time::Now();
  if (waiting_for_initial_policy_fetch_) {
    UMA_HISTOGRAM_MEDIUM_TIMES(kUMAInitialFetchDelayOAuth2Token,
                               time_token_available_ - time_init_completed_);
  }

  if (error.state() == GoogleServiceAuthError::NONE) {
    // Start client registration. Either OnRegistrationStateChanged() or
    // OnClientError() will be called back.
    client()->Register(em::DeviceRegisterRequest::USER,
                       em::DeviceRegisterRequest::FLAVOR_USER_REGISTRATION,
                       em::LicenseType::UNDEFINED, policy_token, std::string(),
                       std::string(), std::string());
  } else {
    UMA_HISTOGRAM_ENUMERATION(kUMAInitialFetchOAuth2Error,
                              error.state(),
                              GoogleServiceAuthError::NUM_STATES);
    if (error.state() == GoogleServiceAuthError::CONNECTION_FAILED) {
      // Network errors are negative in the code, but the histogram data type
      // expects the corresponding positive value.
      UMA_HISTOGRAM_SPARSE_SLOWLY(kUMAInitialFetchOAuth2NetworkError,
                                  -error.network_error());
    }
    // Failed to get a token, stop waiting if policy is not required for this
    // user.
    CancelWaitForPolicyFetch(false);
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
  CancelWaitForPolicyFetch(success);
}

void UserCloudPolicyManagerChromeOS::OnBlockingFetchTimeout() {
  DCHECK(waiting_for_initial_policy_fetch_);
  LOG(WARNING) << "Timed out while waiting for the policy fetch. "
               << "The session will start with the cached policy.";
  CancelWaitForPolicyFetch(false);
}

void UserCloudPolicyManagerChromeOS::CancelWaitForPolicyFetch(bool success) {
  if (!waiting_for_initial_policy_fetch_)
    return;

  policy_fetch_timeout_.Stop();

  // If there was an error, and we don't want to allow profile initialization
  // to go forward after a failed policy fetch, then just return (profile
  // initialization will not complete).
  // TODO(atwilson): Add code to retry policy fetching.
  if (!success && !initial_policy_fetch_may_fail_) {
    LOG(ERROR) << "Policy fetch failed for the user. "
                  "Aborting profile initialization";
    // Need to exit the current user, because we've already started this user's
    // session.
    chrome::AttemptUserExit();
    return;
  }

  waiting_for_initial_policy_fetch_ = false;

  CheckAndPublishPolicy();
  // Now that |waiting_for_initial_policy_fetch_| is guaranteed to be false, the
  // scheduler can be started.
  StartRefreshSchedulerIfReady();
}

void UserCloudPolicyManagerChromeOS::StartRefreshSchedulerIfReady() {
  if (core()->refresh_scheduler())
    return;  // Already started.

  if (waiting_for_initial_policy_fetch_)
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
