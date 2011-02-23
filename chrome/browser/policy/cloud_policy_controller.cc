// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud_policy_controller.h"

#include <algorithm>

#include "base/message_loop.h"
#include "base/rand_util.h"
#include "base/string_util.h"
#include "chrome/browser/policy/cloud_policy_cache.h"
#include "chrome/browser/policy/cloud_policy_subsystem.h"
#include "chrome/browser/policy/device_management_backend.h"
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
    3 * 1000;  // 3 seconds

// Default value for the policy refresh rate.
static const int kPolicyRefreshRateInMilliseconds =
    3 * 60 * 60 * 1000;  // 3 hours.

CloudPolicyController::CloudPolicyController(
    CloudPolicyCache* cache,
    DeviceManagementBackend* backend,
    DeviceTokenFetcher* token_fetcher,
    CloudPolicyIdentityStrategy* identity_strategy)
    : ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)) {
  Initialize(cache,
             backend,
             token_fetcher,
             identity_strategy,
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

void CloudPolicyController::HandlePolicyResponse(
    const em::DevicePolicyResponse& response) {
  if (state_ == STATE_TOKEN_UNAVAILABLE)
    return;

  cache_->SetDevicePolicy(response);
  SetState(STATE_POLICY_VALID);
}

void CloudPolicyController::HandleCloudPolicyResponse(
    const em::CloudPolicyResponse& response) {
  if (state_ == STATE_TOKEN_UNAVAILABLE)
    return;

  cache_->SetPolicy(response);
  SetState(STATE_POLICY_VALID);
}

void CloudPolicyController::OnError(DeviceManagementBackend::ErrorCode code) {
  if (state_ == STATE_TOKEN_UNAVAILABLE)
    return;

  if (code == DeviceManagementBackend::kErrorServiceDeviceNotFound ||
      code == DeviceManagementBackend::kErrorServiceManagementTokenInvalid) {
    LOG(WARNING) << "The device token was either invalid or unknown to the "
                 << "device manager, re-registering device.";
    SetState(STATE_TOKEN_UNAVAILABLE);
  } else if (code ==
             DeviceManagementBackend::kErrorServiceManagementNotSupported) {
    VLOG(1) << "The device is no longer managed, resetting device token.";
    SetState(STATE_TOKEN_UNAVAILABLE);
  } else if (!fallback_to_old_protocol_ &&
             code == DeviceManagementBackend::kErrorRequestInvalid) {
    LOG(WARNING) << "Device manager doesn't understand new protocol, falling "
                 << "back to old request.";
    fallback_to_old_protocol_ = true;
    SetState(STATE_TOKEN_VALID);  // Triggers SendPolicyRequest() immediately.
  } else {
    LOG(WARNING) << "Could not provide policy from the device manager (error = "
                 << code << "), will retry in "
                 << (effective_policy_refresh_error_delay_ms_ / 1000)
                 << " seconds.";
    SetState(STATE_POLICY_ERROR);
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
  SetState(STATE_TOKEN_UNAVAILABLE);
}

CloudPolicyController::CloudPolicyController(
    CloudPolicyCache* cache,
    DeviceManagementBackend* backend,
    DeviceTokenFetcher* token_fetcher,
    CloudPolicyIdentityStrategy* identity_strategy,
    int64 policy_refresh_rate_ms,
    int policy_refresh_deviation_factor_percent,
    int64 policy_refresh_deviation_max_ms,
    int64 policy_refresh_error_delay_ms)
    : ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)) {
  Initialize(cache,
             backend,
             token_fetcher,
             identity_strategy,
             policy_refresh_rate_ms,
             policy_refresh_deviation_factor_percent,
             policy_refresh_deviation_max_ms,
             policy_refresh_error_delay_ms);
}

void CloudPolicyController::Initialize(
    CloudPolicyCache* cache,
    DeviceManagementBackend* backend,
    DeviceTokenFetcher* token_fetcher,
    CloudPolicyIdentityStrategy* identity_strategy,
    int64 policy_refresh_rate_ms,
    int policy_refresh_deviation_factor_percent,
    int64 policy_refresh_deviation_max_ms,
    int64 policy_refresh_error_delay_ms) {
  DCHECK(cache);

  cache_ = cache;
  backend_.reset(backend);
  token_fetcher_ = token_fetcher;
  identity_strategy_ = identity_strategy;
  state_ = STATE_TOKEN_UNAVAILABLE;
  fallback_to_old_protocol_ = false;
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
  if (identity_strategy_->GetCredentials(&username, &auth_token) &&
      CanBeInManagedDomain(username)) {
    token_fetcher_->FetchToken(auth_token, device_id);
  }
}

void CloudPolicyController::SendPolicyRequest() {
  DCHECK(!identity_strategy_->GetDeviceToken().empty());
  if (!fallback_to_old_protocol_) {
    em::CloudPolicyRequest policy_request;
    policy_request.set_policy_scope(kChromePolicyScope);
    backend_->ProcessCloudPolicyRequest(identity_strategy_->GetDeviceToken(),
                                        identity_strategy_->GetDeviceID(),
                                        policy_request, this);
  } else {
    em::DevicePolicyRequest policy_request;
    policy_request.set_policy_scope(kChromePolicyScope);
    em::DevicePolicySettingRequest* setting =
        policy_request.add_setting_request();
    setting->set_key(kChromeDevicePolicySettingKey);
    setting->set_watermark("");
    backend_->ProcessPolicyRequest(identity_strategy_->GetDeviceToken(),
                                   identity_strategy_->GetDeviceID(),
                                   policy_request, this);
  }
}

void CloudPolicyController::DoDelayedWork() {
  DCHECK(delayed_work_task_);
  delayed_work_task_ = NULL;

  switch (state_) {
    case STATE_TOKEN_UNAVAILABLE:
      FetchToken();
      return;
    case STATE_TOKEN_VALID:
    case STATE_POLICY_VALID:
    case STATE_POLICY_ERROR:
      SendPolicyRequest();
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

  base::Time now(base::Time::NowFromSystemTime());
  base::Time refresh_at;
  base::Time last_refresh(cache_->last_policy_refresh_time());
  if (last_refresh.is_null())
    last_refresh = now;

  // Determine when to take the next step.
  switch (state_) {
    case STATE_TOKEN_UNAVAILABLE:
    case STATE_TOKEN_VALID:
      refresh_at = now;
      break;
    case STATE_POLICY_VALID:
      effective_policy_refresh_error_delay_ms_ = policy_refresh_error_delay_ms_;
      refresh_at =
          last_refresh + base::TimeDelta::FromMilliseconds(GetRefreshDelay());
      break;
    case STATE_POLICY_ERROR:
      refresh_at = now + base::TimeDelta::FromMilliseconds(
                             effective_policy_refresh_error_delay_ms_);
      effective_policy_refresh_error_delay_ms_ *= 2;
      if (effective_policy_refresh_error_delay_ms_ > policy_refresh_rate_ms_)
        effective_policy_refresh_error_delay_ms_ = policy_refresh_rate_ms_;
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
