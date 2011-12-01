// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud_policy_controller.h"

#include <algorithm>
#include <string>

#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/rand_util.h"
#include "base/string_util.h"
#include "base/time.h"
#include "chrome/browser/policy/cloud_policy_cache_base.h"
#include "chrome/browser/policy/cloud_policy_subsystem.h"
#include "chrome/browser/policy/delayed_work_scheduler.h"
#include "chrome/browser/policy/device_management_service.h"
#include "chrome/browser/policy/device_token_fetcher.h"
#include "chrome/browser/policy/enterprise_metrics.h"
#include "chrome/browser/policy/policy_notifier.h"
#include "chrome/browser/policy/proto/device_management_constants.h"
#include "chrome/common/guid.h"

namespace {

// The maximum ratio in percent of the policy refresh rate we use for adjusting
// the policy refresh time instant. The rationale is to avoid load spikes from
// many devices that were set up in sync for some reason.
const int kPolicyRefreshDeviationFactorPercent = 10;
// Maximum deviation we are willing to accept.
const int64 kPolicyRefreshDeviationMaxInMilliseconds = 30 * 60 * 1000;

// These are the base values for delays before retrying after an error. They
// will be doubled each time they are used.
const int64 kPolicyRefreshErrorDelayInMilliseconds =
    5 * 60 * 1000;  // 5 minutes.

// Default value for the policy refresh rate.
const int kPolicyRefreshRateInMilliseconds = 3 * 60 * 60 * 1000;  // 3 hours.

// Domain names that are known not to be managed.
// We don't register the device when such a user logs in.
const char* kNonManagedDomains[] = {
  "@googlemail.com",
  "@gmail.com"
};

// Checks the domain part of the given username against the list of known
// non-managed domain names. Returns false if |username| is empty or
// in a domain known not to be managed.
bool CanBeInManagedDomain(const std::string& username) {
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

}  // namespace

namespace policy {

namespace em = enterprise_management;

CloudPolicyController::CloudPolicyController(
    DeviceManagementService* service,
    CloudPolicyCacheBase* cache,
    DeviceTokenFetcher* token_fetcher,
    CloudPolicyDataStore* data_store,
    PolicyNotifier* notifier) {
  Initialize(service,
             cache,
             token_fetcher,
             data_store,
             notifier,
             new DelayedWorkScheduler);
}

CloudPolicyController::~CloudPolicyController() {
  data_store_->RemoveObserver(this);
  scheduler_->CancelDelayedWork();
}

void CloudPolicyController::SetRefreshRate(int64 refresh_rate_milliseconds) {
  policy_refresh_rate_ms_ = refresh_rate_milliseconds;

  // Reschedule the refresh task if necessary.
  if (state_ == STATE_POLICY_VALID)
    SetState(STATE_POLICY_VALID);
}

void CloudPolicyController::Retry() {
  scheduler_->CancelDelayedWork();
  DoWork();
}

void CloudPolicyController::Reset() {
  SetState(STATE_TOKEN_UNAVAILABLE);
}

void CloudPolicyController::HandlePolicyResponse(
    const em::DevicePolicyResponse& response) {
  if (response.response_size() > 0) {
    if (response.response_size() > 1) {
      LOG(WARNING) << "More than one policy in the response of the device "
                   << "management server, discarding.";
    }
    if (!response.response(0).has_error_code() ||
        response.response(0).error_code() ==
            DeviceManagementBackend::kPolicyFetchSuccess) {
      cache_->SetPolicy(response.response(0));
      SetState(STATE_POLICY_VALID);
    } else {
      UMA_HISTOGRAM_ENUMERATION(kMetricPolicy, kMetricPolicyFetchBadResponse,
                                kMetricPolicySize);
      SetState(STATE_POLICY_UNAVAILABLE);
    }
  } else {
    UMA_HISTOGRAM_ENUMERATION(kMetricPolicy, kMetricPolicyFetchBadResponse,
                              kMetricPolicySize);
    SetState(STATE_POLICY_UNAVAILABLE);
  }
}

void CloudPolicyController::OnError(DeviceManagementBackend::ErrorCode code) {
  switch (code) {
    case DeviceManagementBackend::kErrorServiceDeviceNotFound:
    case DeviceManagementBackend::kErrorServiceDeviceIdConflict:
    case DeviceManagementBackend::kErrorServiceManagementTokenInvalid: {
      LOG(WARNING) << "The device token was either invalid or unknown to the "
                   << "device manager, re-registering device.";
      // Will retry fetching a token but gracefully backing off.
      SetState(STATE_TOKEN_ERROR);
      break;
    }
    case DeviceManagementBackend::kErrorServiceInvalidSerialNumber: {
      VLOG(1) << "The device is no longer enlisted for the domain.";
      token_fetcher_->SetSerialNumberInvalidState();
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
    }
    default:
      // Will retry last operation but gracefully backing off.
      SetState(STATE_POLICY_ERROR);
  }
}

void CloudPolicyController::OnDeviceTokenChanged() {
  if (data_store_->device_token().empty())
    SetState(STATE_TOKEN_UNAVAILABLE);
  else
    SetState(STATE_TOKEN_VALID);
}

void CloudPolicyController::OnCredentialsChanged() {
  // This notification is only interesting if we don't have a device token.
  // If we already have a device token, that must be matching the current
  // user, because (1) we always recreate the policy subsystem after user
  // login (2) tokens are cached per user.
  if (data_store_->device_token().empty()) {
    notifier_->Inform(CloudPolicySubsystem::UNENROLLED,
                      CloudPolicySubsystem::NO_DETAILS,
                      PolicyNotifier::POLICY_CONTROLLER);
    effective_policy_refresh_error_delay_ms_ =
        kPolicyRefreshErrorDelayInMilliseconds;
    SetState(STATE_TOKEN_UNAVAILABLE);
  }
}

CloudPolicyController::CloudPolicyController(
    DeviceManagementService* service,
    CloudPolicyCacheBase* cache,
    DeviceTokenFetcher* token_fetcher,
    CloudPolicyDataStore* data_store,
    PolicyNotifier* notifier,
    DelayedWorkScheduler* scheduler) {
  Initialize(service,
             cache,
             token_fetcher,
             data_store,
             notifier,
             scheduler);
}

void CloudPolicyController::Initialize(
    DeviceManagementService* service,
    CloudPolicyCacheBase* cache,
    DeviceTokenFetcher* token_fetcher,
    CloudPolicyDataStore* data_store,
    PolicyNotifier* notifier,
    DelayedWorkScheduler* scheduler) {
  DCHECK(cache);

  service_ = service;
  cache_ = cache;
  token_fetcher_ = token_fetcher;
  data_store_ = data_store;
  notifier_ = notifier;
  state_ = STATE_TOKEN_UNAVAILABLE;
  policy_refresh_rate_ms_ = kPolicyRefreshRateInMilliseconds;
  effective_policy_refresh_error_delay_ms_ =
      kPolicyRefreshErrorDelayInMilliseconds;
  scheduler_.reset(scheduler);
  data_store_->AddObserver(this);
  if (!data_store_->device_token().empty())
    SetState(STATE_TOKEN_VALID);
  else
    SetState(STATE_TOKEN_UNAVAILABLE);
}

void CloudPolicyController::FetchToken() {
  if (data_store_->token_cache_loaded() &&
      !data_store_->user_name().empty() &&
      data_store_->has_auth_token()) {
    if (CanBeInManagedDomain(data_store_->user_name())) {
      // Generate a new random device id. (It'll only be kept if registration
      // succeeds.)
      data_store_->set_device_id(guid::GenerateGUID());
      token_fetcher_->FetchToken();
    } else {
      SetState(STATE_TOKEN_UNMANAGED);
    }
  } else {
    VLOG(1) << "Not ready to fetch DMToken yet, will try again later.";
  }
}

void CloudPolicyController::SendPolicyRequest() {
  backend_.reset(service_->CreateBackend());
  DCHECK(!data_store_->device_token().empty());
  em::DevicePolicyRequest policy_request;
  em::PolicyFetchRequest* fetch_request = policy_request.add_request();
  fetch_request->set_signature_type(em::PolicyFetchRequest::SHA1_RSA);
  fetch_request->set_policy_type(data_store_->policy_type());
  if (!cache_->is_unmanaged() &&
      !cache_->last_policy_refresh_time().is_null()) {
    base::TimeDelta timestamp =
        cache_->last_policy_refresh_time() - base::Time::UnixEpoch();
    fetch_request->set_timestamp(timestamp.InMilliseconds());
  }
  int key_version = 0;
  if (cache_->GetPublicKeyVersion(&key_version))
    fetch_request->set_public_key_version(key_version);

  backend_->ProcessPolicyRequest(data_store_->device_token(),
                                 data_store_->device_id(),
                                 data_store_->user_affiliation(),
                                 policy_request, this);
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

void CloudPolicyController::SetState(
    CloudPolicyController::ControllerState new_state) {
  state_ = new_state;

  backend_.reset();  // Stop any pending requests.

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
      effective_policy_refresh_error_delay_ms_ =
          kPolicyRefreshErrorDelayInMilliseconds;
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
  scheduler_->CancelDelayedWork();
  if (!refresh_at.is_null()) {
    int64 delay = std::max<int64>((refresh_at - now).InMilliseconds(), 0);
    scheduler_->PostDelayedWork(
        base::Bind(&CloudPolicyController::DoWork, base::Unretained(this)),
        delay);
  }

  // Inform the cache if a fetch attempt has completed. This happens if policy
  // has been succesfully fetched, or if token or policy fetching failed.
  if (state_ != STATE_TOKEN_UNAVAILABLE && state_ != STATE_TOKEN_VALID)
    cache_->SetFetchingDone();
}

int64 CloudPolicyController::GetRefreshDelay() {
  int64 deviation = (kPolicyRefreshDeviationFactorPercent *
                     policy_refresh_rate_ms_) / 100;
  deviation = std::min(deviation, kPolicyRefreshDeviationMaxInMilliseconds);
  return policy_refresh_rate_ms_ - base::RandGenerator(deviation + 1);
}

}  // namespace policy
