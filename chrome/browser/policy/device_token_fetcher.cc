// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/device_token_fetcher.h"

#include <algorithm>

#include "base/message_loop.h"
#include "chrome/browser/policy/cloud_policy_cache_base.h"
#include "chrome/browser/policy/device_management_service.h"
#include "chrome/browser/policy/proto/device_management_constants.h"
#include "chrome/browser/policy/proto/device_management_local.pb.h"

namespace {

// Retry after 5 minutes (with exponential backoff) after token fetch errors.
const int64 kTokenFetchErrorDelayMilliseconds = 5 * 60 * 1000;
// Retry after max 3 hours after token fetch errors.
const int64 kTokenFetchErrorMaxDelayMilliseconds = 3 * 60 * 60 * 1000;
// For unmanaged devices, check once per day whether they're still unmanaged.
const int64 kUnmanagedDeviceRefreshRateMilliseconds = 24 * 60 * 60 * 1000;

}  // namespace

namespace policy {

namespace em = enterprise_management;

DeviceTokenFetcher::DeviceTokenFetcher(
    DeviceManagementService* service,
    CloudPolicyCacheBase* cache,
    PolicyNotifier* notifier)
    : ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)) {
  Initialize(service,
             cache,
             notifier,
             kTokenFetchErrorDelayMilliseconds,
             kTokenFetchErrorMaxDelayMilliseconds,
             kUnmanagedDeviceRefreshRateMilliseconds);
}

DeviceTokenFetcher::DeviceTokenFetcher(
    DeviceManagementService* service,
    CloudPolicyCacheBase* cache,
    PolicyNotifier* notifier,
    int64 token_fetch_error_delay_ms,
    int64 token_fetch_error_max_delay_ms,
    int64 unmanaged_device_refresh_rate_ms)
    : ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)) {
  Initialize(service,
             cache,
             notifier,
             token_fetch_error_delay_ms,
             token_fetch_error_max_delay_ms,
             unmanaged_device_refresh_rate_ms);
}

DeviceTokenFetcher::~DeviceTokenFetcher() {
  CancelRetryTask();
}

void DeviceTokenFetcher::FetchToken(
    const std::string& auth_token,
    const std::string& device_id,
    em::DeviceRegisterRequest_Type policy_type,
    const std::string& machine_id,
    const std::string& machine_model) {
  SetState(STATE_INACTIVE);
  auth_token_ = auth_token;
  device_id_ = device_id;
  policy_type_ = policy_type;
  machine_id_ = machine_id;
  machine_model_ = machine_model;
  FetchTokenInternal();
}

void DeviceTokenFetcher::FetchTokenInternal() {
  DCHECK(state_ != STATE_TOKEN_AVAILABLE);
  if (auth_token_.empty() || device_id_.empty()) {
    // Maybe this device is unmanaged, just exit. The CloudPolicyController
    // will call FetchToken() again if something changes.
    return;
  }
  // Construct a new backend, which will discard any previous requests.
  backend_.reset(service_->CreateBackend());
  em::DeviceRegisterRequest request;
  request.set_type(policy_type_);
  if (!machine_id_.empty())
    request.set_machine_id(machine_id_);
  if (!machine_model_.empty())
    request.set_machine_model(machine_model_);
  backend_->ProcessRegisterRequest(auth_token_, device_id_, request, this);
}

void DeviceTokenFetcher::SetUnmanagedState() {
  // The call to |cache_->SetUnmanaged()| has to happen first because it sets
  // the timestamp that |SetState()| needs to determine the correct refresh
  // time.
  cache_->SetUnmanaged();
  SetState(STATE_UNMANAGED);
}

const std::string& DeviceTokenFetcher::GetDeviceToken() {
  return device_token_;
}

void DeviceTokenFetcher::StopAutoRetry() {
  CancelRetryTask();
  backend_.reset();
  device_token_.clear();
  auth_token_.clear();
  device_id_.clear();
}

void DeviceTokenFetcher::AddObserver(DeviceTokenFetcher::Observer* observer) {
  observer_list_.AddObserver(observer);
}

void DeviceTokenFetcher::RemoveObserver(
    DeviceTokenFetcher::Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void DeviceTokenFetcher::HandleRegisterResponse(
    const em::DeviceRegisterResponse& response) {
  if (response.has_device_management_token()) {
    device_token_ = response.device_management_token();
    SetState(STATE_TOKEN_AVAILABLE);
  } else {
    NOTREACHED();
    SetState(STATE_ERROR);
  }
}

void DeviceTokenFetcher::OnError(DeviceManagementBackend::ErrorCode code) {
  switch (code) {
    case DeviceManagementBackend::kErrorServiceManagementNotSupported:
      cache_->SetUnmanaged();
      SetState(STATE_UNMANAGED);
      break;
    case DeviceManagementBackend::kErrorRequestFailed:
    case DeviceManagementBackend::kErrorTemporaryUnavailable:
    case DeviceManagementBackend::kErrorServiceDeviceNotFound:
      SetState(STATE_TEMPORARY_ERROR);
      break;
    case DeviceManagementBackend::kErrorServiceManagementTokenInvalid:
      // Most probably the GAIA auth cookie has expired. We can not do anything
      // until the user logs-in again.
      SetState(STATE_BAD_AUTH);
      break;
    default:
      SetState(STATE_ERROR);
  }
}

void DeviceTokenFetcher::Initialize(DeviceManagementService* service,
                                    CloudPolicyCacheBase* cache,
                                    PolicyNotifier* notifier,
                                    int64 token_fetch_error_delay_ms,
                                    int64 token_fetch_error_max_delay_ms,
                                    int64 unmanaged_device_refresh_rate_ms) {
  service_ = service;
  cache_ = cache;
  notifier_ = notifier;
  token_fetch_error_delay_ms_ = token_fetch_error_delay_ms;
  token_fetch_error_max_delay_ms_ = token_fetch_error_max_delay_ms;
  effective_token_fetch_error_delay_ms_ = token_fetch_error_delay_ms;
  unmanaged_device_refresh_rate_ms_ = unmanaged_device_refresh_rate_ms;
  state_ = STATE_INACTIVE;
  retry_task_ = NULL;

  if (cache_->is_unmanaged())
    SetState(STATE_UNMANAGED);
}

void DeviceTokenFetcher::SetState(FetcherState state) {
  state_ = state;
  if (state_ != STATE_ERROR)
    effective_token_fetch_error_delay_ms_ = token_fetch_error_delay_ms_;

  base::Time delayed_work_at;
  switch (state_) {
    case STATE_INACTIVE:
      device_token_.clear();
      auth_token_.clear();
      device_id_.clear();
      notifier_->Inform(CloudPolicySubsystem::UNENROLLED,
                        CloudPolicySubsystem::NO_DETAILS,
                        PolicyNotifier::TOKEN_FETCHER);
      break;
    case STATE_TOKEN_AVAILABLE:
      FOR_EACH_OBSERVER(Observer, observer_list_, OnDeviceTokenAvailable());
      notifier_->Inform(CloudPolicySubsystem::SUCCESS,
                        CloudPolicySubsystem::NO_DETAILS,
                        PolicyNotifier::TOKEN_FETCHER);
      break;
    case STATE_UNMANAGED:
      delayed_work_at = cache_->last_policy_refresh_time() +
          base::TimeDelta::FromMilliseconds(unmanaged_device_refresh_rate_ms_);
      notifier_->Inform(CloudPolicySubsystem::UNMANAGED,
                        CloudPolicySubsystem::NO_DETAILS,
                        PolicyNotifier::TOKEN_FETCHER);
      break;
    case STATE_TEMPORARY_ERROR:
      delayed_work_at = base::Time::Now() +
          base::TimeDelta::FromMilliseconds(
              effective_token_fetch_error_delay_ms_);
      effective_token_fetch_error_delay_ms_ =
          std::min(effective_token_fetch_error_delay_ms_ * 2,
                   token_fetch_error_max_delay_ms_);
      notifier_->Inform(CloudPolicySubsystem::NETWORK_ERROR,
                        CloudPolicySubsystem::DMTOKEN_NETWORK_ERROR,
                        PolicyNotifier::TOKEN_FETCHER);
      break;
    case STATE_ERROR:
      effective_token_fetch_error_delay_ms_ = token_fetch_error_max_delay_ms_;
      delayed_work_at = base::Time::Now() +
          base::TimeDelta::FromMilliseconds(
              effective_token_fetch_error_delay_ms_);
      notifier_->Inform(CloudPolicySubsystem::NETWORK_ERROR,
                        CloudPolicySubsystem::DMTOKEN_NETWORK_ERROR,
                        PolicyNotifier::TOKEN_FETCHER);
      break;
    case STATE_BAD_AUTH:
      // Can't do anything, need to wait for new credentials.
      notifier_->Inform(CloudPolicySubsystem::BAD_GAIA_TOKEN,
                        CloudPolicySubsystem::NO_DETAILS,
                        PolicyNotifier::TOKEN_FETCHER);
      break;
  }

  CancelRetryTask();
  if (!delayed_work_at.is_null()) {
    base::Time now(base::Time::Now());
    int64 delay = std::max<int64>((delayed_work_at - now).InMilliseconds(), 0);
    retry_task_ = method_factory_.NewRunnableMethod(
            &DeviceTokenFetcher::ExecuteRetryTask);
    MessageLoop::current()->PostDelayedTask(FROM_HERE, retry_task_,
                                            delay);
  }
}

void DeviceTokenFetcher::ExecuteRetryTask() {
  DCHECK(retry_task_);
  retry_task_ = NULL;

  switch (state_) {
    case STATE_INACTIVE:
    case STATE_TOKEN_AVAILABLE:
      break;
    case STATE_UNMANAGED:
    case STATE_ERROR:
    case STATE_TEMPORARY_ERROR:
    case STATE_BAD_AUTH:
      FetchTokenInternal();
      break;
  }
}

void DeviceTokenFetcher::CancelRetryTask() {
  if (retry_task_) {
    retry_task_->Cancel();
    retry_task_ = NULL;
  }
}

}  // namespace policy
