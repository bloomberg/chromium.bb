// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud_policy_controller.h"

#include <algorithm>

#include "base/logging.h"
#include "base/message_loop.h"
#include "base/rand_util.h"
#include "base/string_util.h"
#include "chrome/browser/policy/cloud_policy_cache_base.h"
#include "chrome/browser/policy/cloud_policy_subsystem.h"
#include "chrome/browser/policy/device_management_backend.h"
#include "chrome/browser/policy/device_management_service.h"
#include "chrome/browser/policy/proto/device_management_constants.h"

// Domain names that are known not to be managed.
// We don't register the device when such a user logs in.
static const char* kNonManagedDomains[] = {
  "@googlemail.com",
  "@gmail.com"
};

// Checks the domain part of the given username against the list of known
// non-managed domain names. Returns false if |username| is empty or
// in a domain known not to be managed.
static bool CanBeInManagedDomain(const std::string& username) {
  if (username.empty()) {
    // This means incognito user in case of ChromiumOS and
    // no logged-in user in case of Chromium (SigninService).
    return false;
  }
  for (size_t i = 0; i < arraysize(kNonManagedDomains); i++) {
    if (EndsWith(username, kNonManagedDomains[i], true)) {
      return false;
    }
  }
  return true;
}

namespace policy {

namespace em = enterprise_management;

// The maximum ratio in percent of the policy refresh rate we use for adjusting
// the policy refresh time instant. The rationale is to avoid load spikes from
// many devices that were set up in sync for some reason.
static const int kPolicyRefreshDeviationFactorPercent = 10;
// Maximum deviation we are willing to accept.
static const int64 kPolicyRefreshDeviationMaxInMilliseconds = 30 * 60 * 1000;

// These are the base values for delays before retrying after an error. They
// will be doubled each time they are used.
static const int64 kPolicyRefreshErrorDelayInMilliseconds =
    5 * 60 * 1000;  // 5 minutes

// Default value for the policy refresh rate.
static const int kPolicyRefreshRateInMilliseconds =
    3 * 60 * 60 * 1000;  // 3 hours.

CloudPolicyController::CloudPolicyController(
    DeviceManagementService* service,
    CloudPolicyCacheBase* cache,
    DeviceTokenFetcher* token_fetcher,
    CloudPolicyIdentityStrategy* identity_strategy,
    PolicyNotifier* notifier)
    : ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)) {
  Initialize(service,
             cache,
             token_fetcher,
             identity_strategy,
             notifier,
             kPolicyRefreshRateInMilliseconds,
             kPolicyRefreshDeviationFactorPercent,
             kPolicyRefreshDeviationMaxInMilliseconds,
             kPolicyRefreshErrorDelayInMilliseconds);
}

CloudPolicyController::~CloudPolicyController() {
  token_fetcher_->RemoveObserver(this);
  identity_strategy_->RemoveObserver(this);
  CancelDelayedWork();
}

void CloudPolicyController::SetRefreshRate(int64 refresh_rate_milliseconds) {
  policy_refresh_rate_ms_ = refresh_rate_milliseconds;

  // Reschedule the refresh task if necessary.
  if (state_ == STATE_POLICY_VALID)
    SetState(STATE_POLICY_VALID);
}

void CloudPolicyController::Retry() {
  CancelDelayedWork();
  DoWork();
}

void CloudPolicyController::StopAutoRetry() {
  CancelDelayedWork();
  backend_.reset();
}

void CloudPolicyController::HandlePolicyResponse(
    const em::DevicePolicyResponse& response) {
  if (response.response_size() > 0) {
    if (response.response_size() > 1) {
      LOG(WARNING) << "More than one policy in the response of the device "
                   << "management server, discarding.";
    }
    if (response.response(0).error_code() !=
        DeviceManagementBackend::kErrorServicePolicyNotFound) {
      cache_->SetPolicy(response.response(0));
      SetState(STATE_POLICY_VALID);
    } else {
      SetState(STATE_POLICY_UNAVAILABLE);
    }
  }
}

void CloudPolicyController::OnError(DeviceManagementBackend::ErrorCode code) {
  switch (code) {
    case DeviceManagementBackend::kErrorServiceDeviceNotFound:
    case DeviceManagementBackend::kErrorServiceManagementTokenInvalid: {
      LOG(WARNING) << "The device token was either invalid or unknown to the "
                   << "device manager, re-registering device.";
      // Will retry fetching a token but gracefully backing off.
      SetState(STATE_TOKEN_ERROR);
      break;
    }
    case DeviceManagementBackend::kErrorServiceManagementNotSupported: {
      VLOG(1) << "The device is no longer managed.";
      token_fetcher_->SetUnmanagedState();
      SetState(STATE_TOKEN_UNMANAGED);
      break;
    }
    case DeviceManagementBackend::kErrorServicePolicyNotFound:
    case DeviceManagementBackend::kErrorRequestInvalid:
    case DeviceManagementBackend::kErrorServiceActivationPending:
    case DeviceManagementBackend::kErrorResponseDecoding:
    case DeviceManagementBackend::kErrorHttpStatus: {
      VLOG(1) << "An error in the communication with the policy server occurred"
              << ", will retry in a few hours.";
      SetState(STATE_POLICY_UNAVAILABLE);
      break;
    }
    case DeviceManagementBackend::kErrorRequestFailed:
    case DeviceManagementBackend::kErrorTemporaryUnavailable: {
      VLOG(1) << "A temporary error in the communication with the policy server"
              << " occurred.";
      // Will retry last operation but gracefully backing off.
      SetState(STATE_POLICY_ERROR);
    }
  }
}

void CloudPolicyController::OnDeviceTokenAvailable() {
  identity_strategy_->OnDeviceTokenAvailable(token_fetcher_->GetDeviceToken());
}

void CloudPolicyController::OnDeviceTokenChanged() {
  if (identity_strategy_->GetDeviceToken().empty())
    SetState(STATE_TOKEN_UNAVAILABLE);
  else
    SetState(STATE_TOKEN_VALID);
}

void CloudPolicyController::OnCredentialsChanged() {
  effective_policy_refresh_error_delay_ms_ = policy_refresh_error_delay_ms_;
  SetState(STATE_TOKEN_UNAVAILABLE);
}

CloudPolicyController::CloudPolicyController(
    DeviceManagementService* service,
    CloudPolicyCacheBase* cache,
    DeviceTokenFetcher* token_fetcher,
    CloudPolicyIdentityStrategy* identity_strategy,
    PolicyNotifier* notifier,
    int64 policy_refresh_rate_ms,
    int policy_refresh_deviation_factor_percent,
    int64 policy_refresh_deviation_max_ms,
    int64 policy_refresh_error_delay_ms)
    : ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)) {
  Initialize(service,
             cache,
             token_fetcher,
             identity_strategy,
             notifier,
             policy_refresh_rate_ms,
             policy_refresh_deviation_factor_percent,
             policy_refresh_deviation_max_ms,
             policy_refresh_error_delay_ms);
}

void CloudPolicyController::Initialize(
    DeviceManagementService* service,
    CloudPolicyCacheBase* cache,
    DeviceTokenFetcher* token_fetcher,
    CloudPolicyIdentityStrategy* identity_strategy,
    PolicyNotifier* notifier,
    int64 policy_refresh_rate_ms,
    int policy_refresh_deviation_factor_percent,
    int64 policy_refresh_deviation_max_ms,
    int64 policy_refresh_error_delay_ms) {
  DCHECK(cache);

  service_ = service;
  cache_ = cache;
  token_fetcher_ = token_fetcher;
  identity_strategy_ = identity_strategy;
  notifier_ = notifier;
  state_ = STATE_TOKEN_UNAVAILABLE;
  delayed_work_task_ = NULL;
  policy_refresh_rate_ms_ = policy_refresh_rate_ms;
  policy_refresh_deviation_factor_percent_ =
      policy_refresh_deviation_factor_percent;
  policy_refresh_deviation_max_ms_ = policy_refresh_deviation_max_ms;
  policy_refresh_error_delay_ms_ = policy_refresh_error_delay_ms;
  effective_policy_refresh_error_delay_ms_ = policy_refresh_error_delay_ms;

  token_fetcher_->AddObserver(this);
  identity_strategy_->AddObserver(this);
  if (!identity_strategy_->GetDeviceToken().empty())
    SetState(STATE_TOKEN_VALID);
  else
    SetState(STATE_TOKEN_UNAVAILABLE);
}

void CloudPolicyController::FetchToken() {
  std::string username;
  std::string auth_token;
  std::string device_id = identity_strategy_->GetDeviceID();
  std::string machine_id = identity_strategy_->GetMachineID();
  std::string machine_model = identity_strategy_->GetMachineModel();
  em::DeviceRegisterRequest_Type policy_type =
      identity_strategy_->GetPolicyRegisterType();
  if (identity_strategy_->GetCredentials(&username, &auth_token) &&
      CanBeInManagedDomain(username)) {
    token_fetcher_->FetchToken(auth_token, device_id, policy_type,
                               machine_id, machine_model);
  }
}

void CloudPolicyController::SendPolicyRequest() {
  backend_.reset(service_->CreateBackend());
  DCHECK(!identity_strategy_->GetDeviceToken().empty());
  em::DevicePolicyRequest policy_request;
  em::PolicyFetchRequest* fetch_request = policy_request.add_request();
  fetch_request->set_signature_type(em::PolicyFetchRequest::SHA1_RSA);
  fetch_request->set_policy_type(identity_strategy_->GetPolicyType());
  if (!cache_->is_unmanaged() &&
      !cache_->last_policy_refresh_time().is_null()) {
    base::TimeDelta timestamp =
        cache_->last_policy_refresh_time() - base::Time::UnixEpoch();
    fetch_request->set_timestamp(timestamp.InMilliseconds());
  }
  int key_version = 0;
  if (cache_->GetPublicKeyVersion(&key_version))
    fetch_request->set_public_key_version(key_version);

  backend_->ProcessPolicyRequest(identity_strategy_->GetDeviceToken(),
                                 identity_strategy_->GetDeviceID(),
                                 policy_request, this);
}

void CloudPolicyController::DoDelayedWork() {
  DCHECK(delayed_work_task_);
  delayed_work_task_ = NULL;
  DoWork();
}

void CloudPolicyController::DoWork() {
  switch (state_) {
    case STATE_TOKEN_UNAVAILABLE:
    case STATE_TOKEN_ERROR:
      FetchToken();
      return;
    case STATE_TOKEN_VALID:
    case STATE_POLICY_VALID:
    case STATE_POLICY_ERROR:
    case STATE_POLICY_UNAVAILABLE:
      SendPolicyRequest();
      return;
    case STATE_TOKEN_UNMANAGED:
      return;
  }

  NOTREACHED() << "Unhandled state" << state_;
}

void CloudPolicyController::CancelDelayedWork() {
  if (delayed_work_task_) {
    delayed_work_task_->Cancel();
    delayed_work_task_ = NULL;
  }
}

void CloudPolicyController::SetState(
    CloudPolicyController::ControllerState new_state) {
  state_ = new_state;
  backend_.reset();  // Discard any pending requests.

  base::Time now(base::Time::NowFromSystemTime());
  base::Time refresh_at;
  base::Time last_refresh(cache_->last_policy_refresh_time());
  if (last_refresh.is_null())
    last_refresh = now;

  // Determine when to take the next step.
  bool inform_notifier_done = false;
  switch (state_) {
    case STATE_TOKEN_UNMANAGED:
      notifier_->Inform(CloudPolicySubsystem::UNMANAGED,
                        CloudPolicySubsystem::NO_DETAILS,
                        PolicyNotifier::POLICY_CONTROLLER);
      break;
    case STATE_TOKEN_UNAVAILABLE:
      // The controller is not yet initialized and needs to immediately fetch
      // token and policy if present.
    case STATE_TOKEN_VALID:
      // Immediately try to fetch the token on initialization or policy after a
      // token update. Subsequent retries will respect the back-off strategy.
      refresh_at = now;
      // |notifier_| isn't informed about anything at this point, we wait for
      // the result of the next action first.
      break;
    case STATE_POLICY_VALID:
      // Delay is only reset if the policy fetch operation was successful. This
      // will ensure the server won't get overloaded with retries in case of
      // a bug on either side.
      effective_policy_refresh_error_delay_ms_ = policy_refresh_error_delay_ms_;
      refresh_at =
          last_refresh + base::TimeDelta::FromMilliseconds(GetRefreshDelay());
      notifier_->Inform(CloudPolicySubsystem::SUCCESS,
                        CloudPolicySubsystem::NO_DETAILS,
                        PolicyNotifier::POLICY_CONTROLLER);
      break;
    case STATE_TOKEN_ERROR:
      notifier_->Inform(CloudPolicySubsystem::NETWORK_ERROR,
                        CloudPolicySubsystem::BAD_DMTOKEN,
                        PolicyNotifier::POLICY_CONTROLLER);
      inform_notifier_done = true;
    case STATE_POLICY_ERROR:
      if (!inform_notifier_done) {
        notifier_->Inform(CloudPolicySubsystem::NETWORK_ERROR,
                          CloudPolicySubsystem::POLICY_NETWORK_ERROR,
                          PolicyNotifier::POLICY_CONTROLLER);
      }
      refresh_at = now + base::TimeDelta::FromMilliseconds(
                             effective_policy_refresh_error_delay_ms_);
      effective_policy_refresh_error_delay_ms_ =
          std::min(effective_policy_refresh_error_delay_ms_ * 2,
                   policy_refresh_rate_ms_);
      break;
    case STATE_POLICY_UNAVAILABLE:
      effective_policy_refresh_error_delay_ms_ = policy_refresh_rate_ms_;
      refresh_at = now + base::TimeDelta::FromMilliseconds(
                             effective_policy_refresh_error_delay_ms_);
      notifier_->Inform(CloudPolicySubsystem::NETWORK_ERROR,
                        CloudPolicySubsystem::POLICY_NETWORK_ERROR,
                        PolicyNotifier::POLICY_CONTROLLER);
      break;
  }

  // Update the delayed work task.
  CancelDelayedWork();
  if (!refresh_at.is_null()) {
    int64 delay = std::max<int64>((refresh_at - now).InMilliseconds(), 0);
    delayed_work_task_ = method_factory_.NewRunnableMethod(
        &CloudPolicyController::DoDelayedWork);
    MessageLoop::current()->PostDelayedTask(FROM_HERE, delayed_work_task_,
                                            delay);
  }
}

int64 CloudPolicyController::GetRefreshDelay() {
  int64 deviation = (policy_refresh_deviation_factor_percent_ *
                     policy_refresh_rate_ms_) / 100;
  deviation = std::min(deviation, policy_refresh_deviation_max_ms_);
  return policy_refresh_rate_ms_ - base::RandGenerator(deviation + 1);
}

}  // namespace policy
