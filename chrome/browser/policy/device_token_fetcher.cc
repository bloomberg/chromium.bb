// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/device_token_fetcher.h"

#include <algorithm>

#include "base/bind.h"
#include "base/metrics/histogram.h"
#include "base/time.h"
#include "chrome/browser/policy/cloud_policy_cache_base.h"
#include "chrome/browser/policy/cloud_policy_data_store.h"
#include "chrome/browser/policy/delayed_work_scheduler.h"
#include "chrome/browser/policy/device_management_service.h"
#include "chrome/browser/policy/enterprise_metrics.h"
#include "chrome/browser/policy/policy_notifier.h"
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
    CloudPolicyDataStore* data_store,
    PolicyNotifier* notifier) {
  Initialize(service,
             cache,
             data_store,
             notifier,
             new DelayedWorkScheduler);
}

DeviceTokenFetcher::DeviceTokenFetcher(
    DeviceManagementService* service,
    CloudPolicyCacheBase* cache,
    CloudPolicyDataStore* data_store,
    PolicyNotifier* notifier,
    DelayedWorkScheduler* scheduler) {
  Initialize(service, cache, data_store, notifier, scheduler);
}

DeviceTokenFetcher::~DeviceTokenFetcher() {
  scheduler_->CancelDelayedWork();
}

void DeviceTokenFetcher::FetchToken() {
  SetState(STATE_INACTIVE);
  FetchTokenInternal();
}

void DeviceTokenFetcher::FetchTokenInternal() {
  DCHECK(state_ != STATE_TOKEN_AVAILABLE);
  if (!data_store_->has_auth_token() || data_store_->device_id().empty()) {
    // Maybe this device is unmanaged, just exit. The CloudPolicyController
    // will call FetchToken() again if something changes.
    return;
  }
  // Construct a new backend, which will discard any previous requests.
  backend_.reset(service_->CreateBackend());
  em::DeviceRegisterRequest request;
  request.set_type(data_store_->policy_register_type());
  if (!data_store_->machine_id().empty())
    request.set_machine_id(data_store_->machine_id());
  if (!data_store_->machine_model().empty())
    request.set_machine_model(data_store_->machine_model());
  backend_->ProcessRegisterRequest(data_store_->gaia_token(),
                                   data_store_->oauth_token(),
                                   data_store_->device_id(),
                                   request, this);
}

void DeviceTokenFetcher::SetUnmanagedState() {
  // The call to |cache_->SetUnmanaged()| has to happen first because it sets
  // the timestamp that |SetState()| needs to determine the correct refresh
  // time.
  cache_->SetUnmanaged();
  SetState(STATE_UNMANAGED);
}

void DeviceTokenFetcher::SetSerialNumberInvalidState() {
  SetState(STATE_BAD_SERIAL);
}

void DeviceTokenFetcher::Reset() {
  SetState(STATE_INACTIVE);
}

void DeviceTokenFetcher::HandleRegisterResponse(
    const em::DeviceRegisterResponse& response) {
  if (response.has_device_management_token()) {
    UMA_HISTOGRAM_ENUMERATION(kMetricToken, kMetricTokenFetchOK,
                              kMetricTokenSize);
    data_store_->SetDeviceToken(response.device_management_token(), false);
    SetState(STATE_TOKEN_AVAILABLE);
  } else {
    NOTREACHED();
    UMA_HISTOGRAM_ENUMERATION(kMetricToken, kMetricTokenFetchBadResponse,
                              kMetricTokenSize);
    SetState(STATE_ERROR);
  }
}

void DeviceTokenFetcher::OnError(DeviceManagementBackend::ErrorCode code) {
  switch (code) {
    case DeviceManagementBackend::kErrorServiceManagementNotSupported:
      SetUnmanagedState();
      return;
    case DeviceManagementBackend::kErrorRequestFailed:
    case DeviceManagementBackend::kErrorTemporaryUnavailable:
    case DeviceManagementBackend::kErrorServiceDeviceNotFound:
    case DeviceManagementBackend::kErrorServiceDeviceIdConflict:
      SetState(STATE_TEMPORARY_ERROR);
      return;
    case DeviceManagementBackend::kErrorServiceManagementTokenInvalid:
      // Most probably the GAIA auth cookie has expired. We can not do anything
      // until the user logs-in again.
      SetState(STATE_BAD_AUTH);
      return;
    case DeviceManagementBackend::kErrorServiceInvalidSerialNumber:
      SetSerialNumberInvalidState();
      return;
    case DeviceManagementBackend::kErrorRequestInvalid:
    case DeviceManagementBackend::kErrorHttpStatus:
    case DeviceManagementBackend::kErrorResponseDecoding:
    case DeviceManagementBackend::kErrorServiceActivationPending:
    case DeviceManagementBackend::kErrorServicePolicyNotFound:
      SetState(STATE_ERROR);
      return;
  }
  NOTREACHED();
  SetState(STATE_ERROR);
}

void DeviceTokenFetcher::Initialize(DeviceManagementService* service,
                                    CloudPolicyCacheBase* cache,
                                    CloudPolicyDataStore* data_store,
                                    PolicyNotifier* notifier,
                                    DelayedWorkScheduler* scheduler) {
  service_ = service;
  cache_ = cache;
  notifier_ = notifier;
  data_store_ = data_store;
  effective_token_fetch_error_delay_ms_ = kTokenFetchErrorDelayMilliseconds;
  state_ = STATE_INACTIVE;
  scheduler_.reset(scheduler);

  if (cache_->is_unmanaged())
    SetState(STATE_UNMANAGED);
}

void DeviceTokenFetcher::SetState(FetcherState state) {
  state_ = state;
  if (state_ != STATE_TEMPORARY_ERROR)
    effective_token_fetch_error_delay_ms_ = kTokenFetchErrorDelayMilliseconds;

  backend_.reset();  // Stop any pending requests.

  base::Time delayed_work_at;
  switch (state_) {
    case STATE_INACTIVE:
      notifier_->Inform(CloudPolicySubsystem::UNENROLLED,
                        CloudPolicySubsystem::NO_DETAILS,
                        PolicyNotifier::TOKEN_FETCHER);
      break;
    case STATE_TOKEN_AVAILABLE:
      notifier_->Inform(CloudPolicySubsystem::SUCCESS,
                        CloudPolicySubsystem::NO_DETAILS,
                        PolicyNotifier::TOKEN_FETCHER);
      break;
    case STATE_BAD_SERIAL:
      notifier_->Inform(CloudPolicySubsystem::UNENROLLED,
                        CloudPolicySubsystem::BAD_SERIAL_NUMBER,
                        PolicyNotifier::TOKEN_FETCHER);
      break;
    case STATE_UNMANAGED:
      delayed_work_at = cache_->last_policy_refresh_time() +
          base::TimeDelta::FromMilliseconds(
              kUnmanagedDeviceRefreshRateMilliseconds);
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
                   kTokenFetchErrorMaxDelayMilliseconds);
      notifier_->Inform(CloudPolicySubsystem::NETWORK_ERROR,
                        CloudPolicySubsystem::DMTOKEN_NETWORK_ERROR,
                        PolicyNotifier::TOKEN_FETCHER);
      break;
    case STATE_ERROR:
      effective_token_fetch_error_delay_ms_ =
          kTokenFetchErrorMaxDelayMilliseconds;
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

  scheduler_->CancelDelayedWork();
  if (!delayed_work_at.is_null()) {
    base::Time now(base::Time::Now());
    int64 delay = std::max<int64>((delayed_work_at - now).InMilliseconds(), 0);
    scheduler_->PostDelayedWork(
        base::Bind(&DeviceTokenFetcher::DoWork, base::Unretained(this)), delay);
  }

  // Inform the cache if a token fetch attempt has failed.
  if (state_ != STATE_INACTIVE && state_ != STATE_TOKEN_AVAILABLE)
    cache_->SetFetchingDone();
}

void DeviceTokenFetcher::DoWork() {
  switch (state_) {
    case STATE_INACTIVE:
    case STATE_TOKEN_AVAILABLE:
    case STATE_BAD_SERIAL:
      break;
    case STATE_UNMANAGED:
    case STATE_ERROR:
    case STATE_TEMPORARY_ERROR:
    case STATE_BAD_AUTH:
      FetchTokenInternal();
      break;
  }
}

}  // namespace policy
