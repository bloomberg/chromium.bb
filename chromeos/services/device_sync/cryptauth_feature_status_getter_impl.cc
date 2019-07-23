// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/device_sync/cryptauth_feature_status_getter_impl.h"

#include <array>
#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/stl_util.h"
#include "chromeos/components/multidevice/logging/logging.h"
#include "chromeos/components/multidevice/software_feature.h"
#include "chromeos/components/multidevice/software_feature_state.h"
#include "chromeos/services/device_sync/cryptauth_better_together_feature_types.h"
#include "chromeos/services/device_sync/cryptauth_client.h"

namespace chromeos {

namespace device_sync {

namespace {

// Timeout value for asynchronous operation.
// TODO(https://crbug.com/933656): Tune this value.
constexpr base::TimeDelta kWaitingForBatchGetFeatureStatusesResponseTimeout =
    base::TimeDelta::FromSeconds(10);

constexpr std::array<multidevice::SoftwareFeature, 8> kAllSoftwareFeatures = {
    multidevice::SoftwareFeature::kBetterTogetherHost,
    multidevice::SoftwareFeature::kBetterTogetherClient,
    multidevice::SoftwareFeature::kSmartLockHost,
    multidevice::SoftwareFeature::kSmartLockClient,
    multidevice::SoftwareFeature::kInstantTetheringHost,
    multidevice::SoftwareFeature::kInstantTetheringClient,
    multidevice::SoftwareFeature::kMessagesForWebHost,
    multidevice::SoftwareFeature::kMessagesForWebClient};

CryptAuthDeviceSyncResult::ResultCode
BatchGetFeatureStatusesNetworkRequestErrorToResultCode(
    NetworkRequestError error) {
  switch (error) {
    case NetworkRequestError::kOffline:
      return CryptAuthDeviceSyncResult::ResultCode::
          kErrorBatchGetFeatureStatusesApiCallOffline;
    case NetworkRequestError::kEndpointNotFound:
      return CryptAuthDeviceSyncResult::ResultCode::
          kErrorBatchGetFeatureStatusesApiCallEndpointNotFound;
    case NetworkRequestError::kAuthenticationError:
      return CryptAuthDeviceSyncResult::ResultCode::
          kErrorBatchGetFeatureStatusesApiCallAuthenticationError;
    case NetworkRequestError::kBadRequest:
      return CryptAuthDeviceSyncResult::ResultCode::
          kErrorBatchGetFeatureStatusesApiCallBadRequest;
    case NetworkRequestError::kResponseMalformed:
      return CryptAuthDeviceSyncResult::ResultCode::
          kErrorBatchGetFeatureStatusesApiCallResponseMalformed;
    case NetworkRequestError::kInternalServerError:
      return CryptAuthDeviceSyncResult::ResultCode::
          kErrorBatchGetFeatureStatusesApiCallInternalServerError;
    case NetworkRequestError::kUnknown:
      return CryptAuthDeviceSyncResult::ResultCode::
          kErrorBatchGetFeatureStatusesApiCallUnknownError;
  }
}

CryptAuthFeatureStatusGetter::FeatureStatusMap
ConvertFeatureStatusesToSoftwareFeatureMap(
    const ::google::protobuf::RepeatedPtrField<
        cryptauthv2::DeviceFeatureStatus::FeatureStatus>& feature_statuses,
    bool* did_non_fatal_error_occur) {
  base::flat_set<multidevice::SoftwareFeature> marked_supported;
  base::flat_set<multidevice::SoftwareFeature> marked_enabled;
  for (const cryptauthv2::DeviceFeatureStatus::FeatureStatus& status :
       feature_statuses) {
    // TODO(https://crbug.com/936273): Add metrics to track unknown feature type
    // occurrences.
    if (!base::Contains(GetBetterTogetherFeatureTypes(),
                        status.feature_type())) {
      PA_LOG(ERROR) << "Unknown feature type: " << status.feature_type();
      *did_non_fatal_error_occur = true;
      continue;
    }

    multidevice::SoftwareFeature feature =
        BetterTogetherFeatureTypeStringToSoftwareFeature(status.feature_type());

    if (base::Contains(GetSupportedBetterTogetherFeatureTypes(),
                       status.feature_type()) &&
        status.enabled()) {
      marked_supported.insert(feature);
      continue;
    }

    if (base::Contains(GetEnabledBetterTogetherFeatureTypes(),
                       status.feature_type()) &&
        status.enabled()) {
      marked_enabled.insert(feature);
      continue;
    }
  }

  CryptAuthFeatureStatusGetter::FeatureStatusMap feature_states;
  for (const multidevice::SoftwareFeature& feature : kAllSoftwareFeatures) {
    bool is_marked_supported = base::Contains(marked_supported, feature);
    bool is_marked_enabled = base::Contains(marked_enabled, feature);
    if (is_marked_supported) {
      feature_states[feature] =
          is_marked_enabled ? multidevice::SoftwareFeatureState::kEnabled
                            : multidevice::SoftwareFeatureState::kSupported;
      continue;
    }

    // TODO(https://crbug.com/936273): Add metrics to track occurrences of
    // unsupported features marked as enabled.
    if (!is_marked_supported && is_marked_enabled) {
      PA_LOG(ERROR) << "SoftwareFeature " << feature << " flagged as enabled "
                    << "but not supported. Marking unsupported.";
      *did_non_fatal_error_occur = true;
    }

    feature_states[feature] = multidevice::SoftwareFeatureState::kNotSupported;
  }

  return feature_states;
}

}  // namespace

// static
CryptAuthFeatureStatusGetterImpl::Factory*
    CryptAuthFeatureStatusGetterImpl::Factory::test_factory_ = nullptr;

// static
CryptAuthFeatureStatusGetterImpl::Factory*
CryptAuthFeatureStatusGetterImpl::Factory::Get() {
  if (test_factory_)
    return test_factory_;

  static base::NoDestructor<CryptAuthFeatureStatusGetterImpl::Factory> factory;
  return factory.get();
}

// static
void CryptAuthFeatureStatusGetterImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

CryptAuthFeatureStatusGetterImpl::Factory::~Factory() = default;

std::unique_ptr<CryptAuthFeatureStatusGetter>
CryptAuthFeatureStatusGetterImpl::Factory::BuildInstance(
    CryptAuthClientFactory* client_factory,
    std::unique_ptr<base::OneShotTimer> timer) {
  return base::WrapUnique(
      new CryptAuthFeatureStatusGetterImpl(client_factory, std::move(timer)));
}

CryptAuthFeatureStatusGetterImpl::CryptAuthFeatureStatusGetterImpl(
    CryptAuthClientFactory* client_factory,
    std::unique_ptr<base::OneShotTimer> timer)
    : client_factory_(client_factory), timer_(std::move(timer)) {
  DCHECK(client_factory);
}

CryptAuthFeatureStatusGetterImpl::~CryptAuthFeatureStatusGetterImpl() = default;

void CryptAuthFeatureStatusGetterImpl::OnAttemptStarted(
    const cryptauthv2::RequestContext& request_context,
    const base::flat_set<std::string>& device_ids) {
  cryptauthv2::BatchGetFeatureStatusesRequest request;
  request.mutable_context()->CopyFrom(request_context);
  *request.mutable_device_ids() = {device_ids.begin(), device_ids.end()};
  *request.mutable_feature_types() = {GetBetterTogetherFeatureTypes().begin(),
                                      GetBetterTogetherFeatureTypes().end()};

  // TODO(https://crbug.com/936273): Add metrics to track failure rates due to
  // async timeouts.
  timer_->Start(
      FROM_HERE, kWaitingForBatchGetFeatureStatusesResponseTimeout,
      base::BindOnce(
          &CryptAuthFeatureStatusGetterImpl::OnBatchGetFeatureStatusesTimeout,
          base::Unretained(this)));

  cryptauth_client_ = client_factory_->CreateInstance();
  cryptauth_client_->BatchGetFeatureStatuses(
      request,
      base::Bind(
          &CryptAuthFeatureStatusGetterImpl::OnBatchGetFeatureStatusesSuccess,
          base::Unretained(this), device_ids),
      base::Bind(
          &CryptAuthFeatureStatusGetterImpl::OnBatchGetFeatureStatusesFailure,
          base::Unretained(this)));
}

void CryptAuthFeatureStatusGetterImpl::OnBatchGetFeatureStatusesSuccess(
    const base::flat_set<std::string>& input_device_ids,
    const cryptauthv2::BatchGetFeatureStatusesResponse& feature_response) {
  DCHECK(id_to_feature_status_map_.empty());

  bool did_non_fatal_error_occur = false;
  for (const cryptauthv2::DeviceFeatureStatus& device_feature_status :
       feature_response.device_feature_statuses()) {
    const std::string& id = device_feature_status.device_id();

    // TODO(https://crbug.com/936273): Log metrics for unrequested devices
    // in response.
    bool was_id_requested = base::Contains(input_device_ids, id);
    if (!was_id_requested) {
      PA_LOG(ERROR) << "Unrequested device (ID: " << id
                    << ") in BatchGetFeatureStatuses response.";
      did_non_fatal_error_occur = true;
      continue;
    }

    // TODO(https://crbug.com/936273): Log metrics for duplicate device IDs.
    bool is_duplicate_id = base::Contains(id_to_feature_status_map_, id);
    if (is_duplicate_id) {
      PA_LOG(ERROR) << "Duplicate device IDs (" << id
                    << ") in BatchGetFeatureStatuses response.";
      did_non_fatal_error_occur = true;
      continue;
    }

    id_to_feature_status_map_[device_feature_status.device_id()] =
        ConvertFeatureStatusesToSoftwareFeatureMap(
            device_feature_status.feature_statuses(),
            &did_non_fatal_error_occur);
  }

  // TODO(https://crbug.com/936273): Log metrics for missing devices.
  if (input_device_ids.size() != id_to_feature_status_map_.size()) {
    PA_LOG(ERROR) << "Devices missing in BatchGetFeatureStatuses response.";
    did_non_fatal_error_occur = true;
  }

  CryptAuthDeviceSyncResult::ResultCode result_code =
      did_non_fatal_error_occur
          ? CryptAuthDeviceSyncResult::ResultCode::kFinishedWithNonFatalErrors
          : CryptAuthDeviceSyncResult::ResultCode::kSuccess;
  FinishAttempt(result_code);
}

void CryptAuthFeatureStatusGetterImpl::OnBatchGetFeatureStatusesFailure(
    NetworkRequestError error) {
  FinishAttempt(BatchGetFeatureStatusesNetworkRequestErrorToResultCode(error));
}

void CryptAuthFeatureStatusGetterImpl::OnBatchGetFeatureStatusesTimeout() {
  FinishAttempt(CryptAuthDeviceSyncResult::ResultCode::
                    kErrorTimeoutWaitingForBatchGetFeatureStatusesResponse);
}

void CryptAuthFeatureStatusGetterImpl::FinishAttempt(
    const CryptAuthDeviceSyncResult::ResultCode& result_code) {
  cryptauth_client_.reset();
  timer_->Stop();

  OnAttemptFinished(id_to_feature_status_map_, result_code);
}

}  // namespace device_sync

}  // namespace chromeos
