// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/device_token_fetcher.h"

#include <algorithm>

#include "base/bind.h"
#include "base/metrics/histogram.h"
#include "base/time.h"
#include "chrome/browser/policy/cloud_policy_cache_base.h"
#include "chrome/browser/policy/cloud_policy_constants.h"
#include "chrome/browser/policy/cloud_policy_data_store.h"
#include "chrome/browser/policy/delayed_work_scheduler.h"
#include "chrome/browser/policy/device_management_service.h"
#include "chrome/browser/policy/enterprise_metrics.h"
#include "chrome/browser/policy/policy_notifier.h"
#include "chrome/browser/policy/proto/device_management_local.pb.h"

namespace em = enterprise_management;

namespace policy {

namespace {

// Retry after 5 minutes (with exponential backoff) after token fetch errors.
const int64 kTokenFetchErrorDelayMilliseconds = 5 * 60 * 1000;
// Retry after max 3 hours after token fetch errors.
const int64 kTokenFetchErrorMaxDelayMilliseconds = 3 * 60 * 60 * 1000;
// For unmanaged devices, check once per day whether they're still unmanaged.
const int64 kUnmanagedDeviceRefreshRateMilliseconds = 24 * 60 * 60 * 1000;

// Records the UMA metric corresponding to |status|, if it represents an error.
// Also records that a fetch response was received.
void SampleErrorStatus(DeviceManagementStatus status) {
  UMA_HISTOGRAM_ENUMERATION(kMetricToken, kMetricTokenFetchResponseReceived,
                            kMetricTokenSize);
  int sample = -1;
  switch (status) {
    case DM_STATUS_SUCCESS:
      return;
    case DM_STATUS_REQUEST_FAILED:
    case DM_STATUS_REQUEST_INVALID:
    case DM_STATUS_SERVICE_MANAGEMENT_TOKEN_INVALID:
      sample = kMetricTokenFetchRequestFailed;
      break;
    case DM_STATUS_SERVICE_MANAGEMENT_NOT_SUPPORTED:
      sample = kMetricTokenFetchManagementNotSupported;
      break;
    case DM_STATUS_SERVICE_DEVICE_NOT_FOUND:
      sample = kMetricTokenFetchDeviceNotFound;
      break;
    case DM_STATUS_SERVICE_DEVICE_ID_CONFLICT:
      sample = kMetricTokenFetchDeviceIdConflict;
      break;
    case DM_STATUS_SERVICE_INVALID_SERIAL_NUMBER:
      sample = kMetricTokenFetchInvalidSerialNumber;
      break;
    case DM_STATUS_RESPONSE_DECODING_ERROR:
      sample = kMetricTokenFetchBadResponse;
      break;
    case DM_STATUS_TEMPORARY_UNAVAILABLE:
    case DM_STATUS_SERVICE_ACTIVATION_PENDING:
    case DM_STATUS_SERVICE_POLICY_NOT_FOUND:
    case DM_STATUS_HTTP_STATUS_ERROR:
      sample = kMetricTokenFetchServerFailed;
      break;
  }
  if (sample != -1)
    UMA_HISTOGRAM_ENUMERATION(kMetricToken, sample, kMetricTokenSize);
  else
    NOTREACHED();
}

// Translates the DeviceRegisterResponse::DeviceMode |mode| to the enum used
// internally to represent different device modes.
DeviceMode TranslateProtobufDeviceMode(
    em::DeviceRegisterResponse::DeviceMode mode) {
  switch (mode) {
    case em::DeviceRegisterResponse::ENTERPRISE:
      return DEVICE_MODE_ENTERPRISE;
    case em::DeviceRegisterResponse::RETAIL:
      return DEVICE_MODE_KIOSK;
  }
  LOG(ERROR) << "Unknown enrollment mode in registration response: " << mode;
  return  DEVICE_MODE_PENDING;
}

}  // namespace

DeviceTokenFetcher::DeviceTokenFetcher(
    DeviceManagementService* service,
    CloudPolicyCacheBase* cache,
    CloudPolicyDataStore* data_store,
    PolicyNotifier* notifier)
    : effective_token_fetch_error_delay_ms_(
          kTokenFetchErrorDelayMilliseconds) {
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
    DelayedWorkScheduler* scheduler)
    : effective_token_fetch_error_delay_ms_(
          kTokenFetchErrorDelayMilliseconds) {
  Initialize(service, cache, data_store, notifier, scheduler);
}

DeviceTokenFetcher::~DeviceTokenFetcher() {
  scheduler_->CancelDelayedWork();
}

void DeviceTokenFetcher::FetchToken() {
  SetState(STATE_INACTIVE);
  FetchTokenInternal();
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

void DeviceTokenFetcher::FetchTokenInternal() {
  DCHECK(state_ != STATE_TOKEN_AVAILABLE);
  if (!data_store_->has_auth_token() || data_store_->device_id().empty()) {
    // Maybe this device is unmanaged, just exit. The CloudPolicyController
    // will call FetchToken() again if something changes.
    return;
  }
  // Reinitialize |request_job_|, discarding any previous requests.
  request_job_.reset(
      service_->CreateJob(DeviceManagementRequestJob::TYPE_REGISTRATION));
  request_job_->SetGaiaToken(data_store_->gaia_token());
  request_job_->SetOAuthToken(data_store_->oauth_token());
  request_job_->SetClientID(data_store_->device_id());
  em::DeviceRegisterRequest* request =
      request_job_->GetRequest()->mutable_register_request();
  request->set_type(data_store_->policy_register_type());
  if (!data_store_->machine_id().empty())
    request->set_machine_id(data_store_->machine_id());
  if (!data_store_->machine_model().empty())
    request->set_machine_model(data_store_->machine_model());
  if (data_store_->known_machine_id())
    request->set_auto_enrolled(true);
  if (data_store_->reregister())
    request->set_reregister(true);
  request_job_->Start(base::Bind(&DeviceTokenFetcher::OnTokenFetchCompleted,
                                 base::Unretained(this)));
  UMA_HISTOGRAM_ENUMERATION(kMetricToken, kMetricTokenFetchRequested,
                            kMetricTokenSize);
}

void DeviceTokenFetcher::OnTokenFetchCompleted(
    DeviceManagementStatus status,
    const em::DeviceManagementResponse& response) {
  if (status == DM_STATUS_SUCCESS && !response.has_register_response()) {
    // Handled below.
    status = DM_STATUS_RESPONSE_DECODING_ERROR;
  }

  LOG_IF(ERROR, status != DM_STATUS_SUCCESS) << "DMServer returned error code: "
                                             << status;
  SampleErrorStatus(status);

  switch (status) {
    case DM_STATUS_SUCCESS: {
      const em::DeviceRegisterResponse& register_response =
          response.register_response();
      if (register_response.has_device_management_token()) {
        UMA_HISTOGRAM_ENUMERATION(kMetricToken, kMetricTokenFetchOK,
                                  kMetricTokenSize);

        if (data_store_->policy_register_type() ==
                em::DeviceRegisterRequest::DEVICE) {
          // TODO(pastarmovj): Default to DEVICE_MODE_UNKNOWN once DM server has
          // been updated. http://crosbug.com/26624
          DeviceMode mode = DEVICE_MODE_ENTERPRISE;
          if (register_response.has_enrollment_type()) {
            mode = TranslateProtobufDeviceMode(
                register_response.enrollment_type());
          }
          if (mode == DEVICE_MODE_PENDING) {
            LOG(ERROR) << "Enrollment mode missing or unknown!";
            SetState(STATE_BAD_ENROLLMENT_MODE);
            return;
          }
          data_store_->set_device_mode(mode);
        }
        data_store_->SetDeviceToken(register_response.device_management_token(),
                                    false);
        SetState(STATE_TOKEN_AVAILABLE);
      } else {
        NOTREACHED();
        UMA_HISTOGRAM_ENUMERATION(kMetricToken, kMetricTokenFetchBadResponse,
                                  kMetricTokenSize);
        SetState(STATE_ERROR);
      }
      return;
    }
    case DM_STATUS_SERVICE_MANAGEMENT_NOT_SUPPORTED:
      SetUnmanagedState();
      return;
    case DM_STATUS_REQUEST_FAILED:
    case DM_STATUS_TEMPORARY_UNAVAILABLE:
    case DM_STATUS_SERVICE_DEVICE_NOT_FOUND:
    case DM_STATUS_SERVICE_DEVICE_ID_CONFLICT:
      SetState(STATE_TEMPORARY_ERROR);
      return;
    case DM_STATUS_SERVICE_MANAGEMENT_TOKEN_INVALID:
      // Most probably the GAIA auth cookie has expired. We can not do anything
      // until the user logs-in again.
      SetState(STATE_BAD_AUTH);
      return;
    case DM_STATUS_SERVICE_INVALID_SERIAL_NUMBER:
      SetSerialNumberInvalidState();
      return;
    case DM_STATUS_REQUEST_INVALID:
    case DM_STATUS_HTTP_STATUS_ERROR:
    case DM_STATUS_RESPONSE_DECODING_ERROR:
    case DM_STATUS_SERVICE_ACTIVATION_PENDING:
    case DM_STATUS_SERVICE_POLICY_NOT_FOUND:
      SetState(STATE_ERROR);
      return;
  }
  NOTREACHED();
  SetState(STATE_ERROR);
}

void DeviceTokenFetcher::SetState(FetcherState state) {
  state_ = state;
  if (state_ != STATE_TEMPORARY_ERROR)
    effective_token_fetch_error_delay_ms_ = kTokenFetchErrorDelayMilliseconds;

  request_job_.reset();  // Stop any pending requests.

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
    case STATE_BAD_ENROLLMENT_MODE:
      notifier_->Inform(CloudPolicySubsystem::UNENROLLED,
                        CloudPolicySubsystem::BAD_ENROLLMENT_MODE,
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
    case STATE_BAD_ENROLLMENT_MODE:
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
