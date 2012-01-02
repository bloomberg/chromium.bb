// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/device_management_backend_impl.h"

#include "base/bind.h"
#include "base/metrics/histogram.h"
#include "base/stl_util.h"
#include "chrome/browser/policy/device_management_service.h"
#include "chrome/browser/policy/enterprise_metrics.h"

namespace em = enterprise_management;

namespace policy {

namespace {

// CloudPolicyDataStore::DeviceManagement is being phased out; for now we
// convert here to support legacy code.
DeviceManagementBackend::ErrorCode ConvertStatus(
    DeviceManagementStatus status) {
  switch (status) {
    case DM_STATUS_SUCCESS:
      NOTREACHED();
      return DeviceManagementBackend::kErrorRequestFailed;
    case DM_STATUS_REQUEST_INVALID:
      return DeviceManagementBackend::kErrorRequestInvalid;
    case DM_STATUS_REQUEST_FAILED:
      return DeviceManagementBackend::kErrorRequestFailed;
    case DM_STATUS_TEMPORARY_UNAVAILABLE:
      return DeviceManagementBackend::kErrorTemporaryUnavailable;
    case DM_STATUS_HTTP_STATUS_ERROR:
      return DeviceManagementBackend::kErrorHttpStatus;
    case DM_STATUS_RESPONSE_DECODING_ERROR:
      return DeviceManagementBackend::kErrorResponseDecoding;
    case DM_STATUS_SERVICE_MANAGEMENT_NOT_SUPPORTED:
      return DeviceManagementBackend::kErrorServiceManagementNotSupported;
    case DM_STATUS_SERVICE_DEVICE_NOT_FOUND:
      return DeviceManagementBackend::kErrorServiceDeviceNotFound;
    case DM_STATUS_SERVICE_MANAGEMENT_TOKEN_INVALID:
      return DeviceManagementBackend::kErrorServiceManagementTokenInvalid;
    case DM_STATUS_SERVICE_ACTIVATION_PENDING:
      return DeviceManagementBackend::kErrorServiceActivationPending;
    case DM_STATUS_SERVICE_INVALID_SERIAL_NUMBER:
      return DeviceManagementBackend::kErrorServiceInvalidSerialNumber;
    case DM_STATUS_SERVICE_DEVICE_ID_CONFLICT:
      return DeviceManagementBackend::kErrorServiceDeviceIdConflict;
    case DM_STATUS_SERVICE_POLICY_NOT_FOUND:
      return DeviceManagementBackend::kErrorServicePolicyNotFound;
  }

  NOTREACHED();
  return DeviceManagementBackend::kErrorRequestFailed;
}

// Conversion helper to support legacy code.
UserAffiliation ConvertAffiliation(
    CloudPolicyDataStore::UserAffiliation affiliation) {
  switch (affiliation) {
    case CloudPolicyDataStore::USER_AFFILIATION_MANAGED:
      return USER_AFFILIATION_MANAGED;
    case CloudPolicyDataStore::USER_AFFILIATION_NONE:
      return USER_AFFILIATION_NONE;
  }

  NOTREACHED();
  return USER_AFFILIATION_NONE;
}

}  // namespace

DeviceManagementBackendImpl::DeviceManagementBackendImpl(
    DeviceManagementService* service)
    : service_(service) {
}

DeviceManagementBackendImpl::~DeviceManagementBackendImpl() {
  STLDeleteElements(&pending_jobs_);
}

void DeviceManagementBackendImpl::ProcessRegisterRequest(
    const std::string& gaia_auth_token,
    const std::string& oauth_token,
    const std::string& device_id,
    const em::DeviceRegisterRequest& request,
    DeviceRegisterResponseDelegate* delegate) {
  UMA_HISTOGRAM_ENUMERATION(kMetricToken, kMetricTokenFetchRequested,
                            kMetricTokenSize);
  DeviceManagementRequestJob* job =
      service_->CreateJob(DeviceManagementRequestJob::TYPE_REGISTRATION);
  job->SetGaiaToken(gaia_auth_token);
  job->SetOAuthToken(oauth_token);
  job->SetClientID(device_id);
  job->GetRequest()->mutable_register_request()->CopyFrom(request);
  job->Start(base::Bind(&DeviceManagementBackendImpl::OnRegistrationDone,
                        base::Unretained(this), job, delegate));
  pending_jobs_.insert(job);
}

void DeviceManagementBackendImpl::ProcessUnregisterRequest(
    const std::string& device_management_token,
    const std::string& device_id,
    const em::DeviceUnregisterRequest& request,
    DeviceUnregisterResponseDelegate* delegate) {
  DeviceManagementRequestJob* job =
      service_->CreateJob(DeviceManagementRequestJob::TYPE_UNREGISTRATION);
  job->SetDMToken(device_management_token);
  job->SetClientID(device_id);
  job->GetRequest()->mutable_unregister_request();
  job->Start(base::Bind(&DeviceManagementBackendImpl::OnUnregistrationDone,
                        base::Unretained(this), job, delegate));
  pending_jobs_.insert(job);
}

void DeviceManagementBackendImpl::ProcessPolicyRequest(
    const std::string& device_management_token,
    const std::string& device_id,
    CloudPolicyDataStore::UserAffiliation affiliation,
    const em::DevicePolicyRequest& request,
    const em::DeviceStatusReportRequest* device_status,
    DevicePolicyResponseDelegate* delegate) {
  UMA_HISTOGRAM_ENUMERATION(kMetricPolicy, kMetricPolicyFetchRequested,
                            kMetricPolicySize);
  DeviceManagementRequestJob* job =
      service_->CreateJob(DeviceManagementRequestJob::TYPE_POLICY_FETCH);
  job->SetDMToken(device_management_token);
  job->SetClientID(device_id);
  job->SetUserAffiliation(ConvertAffiliation(affiliation));
  job->GetRequest()->mutable_policy_request()->CopyFrom(request);
  if (device_status) {
    job->GetRequest()->mutable_device_status_report_request()->CopyFrom(
        *device_status);
  }
  job->Start(base::Bind(&DeviceManagementBackendImpl::OnPolicyFetchDone,
                        base::Unretained(this), job, delegate));
  pending_jobs_.insert(job);
}

void DeviceManagementBackendImpl::ProcessAutoEnrollmentRequest(
    const std::string& device_id,
    const em::DeviceAutoEnrollmentRequest& request,
    DeviceAutoEnrollmentResponseDelegate* delegate) {
  DeviceManagementRequestJob* job =
      service_->CreateJob(DeviceManagementRequestJob::TYPE_AUTO_ENROLLMENT);
  job->SetClientID(device_id);
  job->GetRequest()->mutable_auto_enrollment_request()->CopyFrom(request);
  job->Start(base::Bind(&DeviceManagementBackendImpl::OnAutoEnrollmentDone,
                        base::Unretained(this), job, delegate));
  pending_jobs_.insert(job);
}

void DeviceManagementBackendImpl::OnRegistrationDone(
    DeviceManagementRequestJob* job,
    DeviceRegisterResponseDelegate* delegate,
    DeviceManagementStatus status,
    const enterprise_management::DeviceManagementResponse& response) {
  pending_jobs_.erase(job);
  if (status == DM_STATUS_SUCCESS)
    delegate->HandleRegisterResponse(response.register_response());
  else
    delegate->OnError(ConvertStatus(status));
  delete job;
}

void DeviceManagementBackendImpl::OnUnregistrationDone(
    DeviceManagementRequestJob* job,
    DeviceUnregisterResponseDelegate* delegate,
    DeviceManagementStatus status,
    const enterprise_management::DeviceManagementResponse& response) {
  pending_jobs_.erase(job);
  if (status == DM_STATUS_SUCCESS)
    delegate->HandleUnregisterResponse(response.unregister_response());
  else
    delegate->OnError(ConvertStatus(status));
  delete job;
}

void DeviceManagementBackendImpl::OnPolicyFetchDone(
    DeviceManagementRequestJob* job,
    DevicePolicyResponseDelegate* delegate,
    DeviceManagementStatus status,
    const enterprise_management::DeviceManagementResponse& response) {
  pending_jobs_.erase(job);
  if (status == DM_STATUS_SUCCESS)
    delegate->HandlePolicyResponse(response.policy_response());
  else
    delegate->OnError(ConvertStatus(status));
  delete job;
}

void DeviceManagementBackendImpl::OnAutoEnrollmentDone(
    DeviceManagementRequestJob* job,
    DeviceAutoEnrollmentResponseDelegate* delegate,
    DeviceManagementStatus status,
    const enterprise_management::DeviceManagementResponse& response) {
  pending_jobs_.erase(job);
  if (status == DM_STATUS_SUCCESS)
    delegate->HandleAutoEnrollmentResponse(response.auto_enrollment_response());
  else
    delegate->OnError(ConvertStatus(status));
  delete job;
}

}  // namespace policy
