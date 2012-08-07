// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud_policy_client.h"

#include "base/bind.h"
#include "base/guid.h"
#include "base/logging.h"
#include "chrome/browser/policy/device_management_service.h"
#include "chrome/browser/policy/proto/device_management_backend.pb.h"

namespace em = enterprise_management;

namespace policy {

CloudPolicyClient::Observer::~Observer() {}

CloudPolicyClient::StatusProvider::~StatusProvider() {}

CloudPolicyClient::CloudPolicyClient(const std::string& machine_id,
                                     const std::string& machine_model,
                                     UserAffiliation user_affiliation,
                                     PolicyScope scope,
                                     StatusProvider* status_provider,
                                     DeviceManagementService* service)
    : machine_id_(machine_id),
      machine_model_(machine_model),
      user_affiliation_(user_affiliation),
      scope_(scope),
      submit_machine_id_(false),
      public_key_version_(-1),
      public_key_version_valid_(false),
      service_(service),
      status_provider_(status_provider),
      status_(DM_STATUS_SUCCESS) {}

CloudPolicyClient::~CloudPolicyClient() {}

void CloudPolicyClient::SetupRegistration(const std::string& dm_token,
                                          const std::string& client_id) {
  DCHECK(!dm_token.empty());
  DCHECK(!client_id.empty());
  DCHECK(!is_registered());

  dm_token_ = dm_token;
  client_id_ = client_id;
  request_job_.reset();
  policy_.reset();

  NotifyRegistrationStateChanged();
}

void CloudPolicyClient::Register(const std::string& auth_token) {
  DCHECK(!auth_token.empty());
  DCHECK(!is_registered());

  // Generate a new client ID. This is intentionally done on each new
  // registration request in order to preserve privacy. Reusing IDs would mean
  // the server could track clients by their registration attempts.
  client_id_ = base::GenerateGUID();

  request_job_.reset(
      service_->CreateJob(DeviceManagementRequestJob::TYPE_REGISTRATION));
  request_job_->SetOAuthToken(auth_token);
  request_job_->SetClientID(client_id_);

  em::DeviceRegisterRequest* request =
      request_job_->GetRequest()->mutable_register_request();
  SetRegistrationType(request);
  if (!machine_id_.empty())
    request->set_machine_id(machine_id_);
  if (!machine_model_.empty())
    request->set_machine_model(machine_model_);

  request_job_->Start(base::Bind(&CloudPolicyClient::OnRegisterCompleted,
                                 base::Unretained(this)));
}

void CloudPolicyClient::FetchPolicy() {
  CHECK(is_registered());

  request_job_.reset(
      service_->CreateJob(DeviceManagementRequestJob::TYPE_POLICY_FETCH));
  request_job_->SetDMToken(dm_token_);
  request_job_->SetClientID(client_id_);
  request_job_->SetUserAffiliation(user_affiliation_);

  em::DeviceManagementRequest* request = request_job_->GetRequest();

  // Build policy fetch request.
  em::PolicyFetchRequest* policy_request =
      request->mutable_policy_request()->add_request();
  policy_request->set_signature_type(em::PolicyFetchRequest::SHA1_RSA);
  SetPolicyType(policy_request);
  if (!last_policy_timestamp_.is_null()) {
    base::TimeDelta timestamp(last_policy_timestamp_ - base::Time::UnixEpoch());
    policy_request->set_timestamp(timestamp.InMilliseconds());
  }
  if (submit_machine_id_ && !machine_id_.empty())
    policy_request->set_machine_id(machine_id_);
  if (public_key_version_valid_)
    policy_request->set_public_key_version(public_key_version_);

  // Add status data.
  if (status_provider_) {
    if (!status_provider_->GetDeviceStatus(
            request->mutable_device_status_report_request())) {
      request->clear_device_status_report_request();
    }
    if (!status_provider_->GetSessionStatus(
            request->mutable_session_status_report_request())) {
      request->clear_session_status_report_request();
    }
  }

  // Fire the job.
  request_job_->Start(base::Bind(&CloudPolicyClient::OnPolicyFetchCompleted,
                                 base::Unretained(this)));
}

void CloudPolicyClient::Unregister() {
  request_job_.reset(
      service_->CreateJob(DeviceManagementRequestJob::TYPE_UNREGISTRATION));
  request_job_->SetDMToken(dm_token_);
  request_job_->SetClientID(client_id_);
  request_job_->GetRequest()->mutable_unregister_request();
  request_job_->Start(base::Bind(&CloudPolicyClient::OnUnregisterCompleted,
                                 base::Unretained(this)));
}

void CloudPolicyClient::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void CloudPolicyClient::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void CloudPolicyClient::SetRegistrationType(
    em::DeviceRegisterRequest* request) const {
  switch (scope_) {
    case POLICY_SCOPE_USER:
      request->set_type(em::DeviceRegisterRequest::USER);
      return;
    case POLICY_SCOPE_MACHINE:
      request->set_type(em::DeviceRegisterRequest::DEVICE);
      return;
  }
  NOTREACHED() << "Invalid policy scope " << scope_;
}

void CloudPolicyClient::SetPolicyType(em::PolicyFetchRequest* request) const {
  switch (scope_) {
    case POLICY_SCOPE_USER:
      request->set_policy_type(dm_protocol::kChromeUserPolicyType);
      return;
    case POLICY_SCOPE_MACHINE:
      request->set_policy_type(dm_protocol::kChromeDevicePolicyType);
      return;
  }
  NOTREACHED() << "Invalid policy scope " << scope_;
}

void CloudPolicyClient::OnRegisterCompleted(
    DeviceManagementStatus status,
    const em::DeviceManagementResponse& response) {
  if (status == DM_STATUS_SUCCESS &&
      (!response.has_register_response() ||
       !response.register_response().has_device_management_token())) {
    LOG(WARNING) << "Empty registration response.";
    status = DM_STATUS_RESPONSE_DECODING_ERROR;
  }

  status_ = status;
  if (status == DM_STATUS_SUCCESS) {
    dm_token_ = response.register_response().device_management_token();
    NotifyRegistrationStateChanged();
  } else {
    NotifyClientError();
  }
}

void CloudPolicyClient::OnPolicyFetchCompleted(
    DeviceManagementStatus status,
    const em::DeviceManagementResponse& response) {
  if (status == DM_STATUS_SUCCESS) {
    if (!response.has_policy_response() ||
        response.policy_response().response_size() == 0) {
      LOG(WARNING) << "Empty policy response.";
      status = DM_STATUS_RESPONSE_DECODING_ERROR;
    } else if (response.policy_response().response_size() > 1) {
      LOG(WARNING) << "More than one response entries, ignoring all but first.";
    }
  }

  status_ = status;
  if (status == DM_STATUS_SUCCESS) {
    policy_.reset(new enterprise_management::PolicyFetchResponse(
        response.policy_response().response(0)));
    if (status_provider_)
      status_provider_->OnSubmittedSuccessfully();
    NotifyPolicyFetched();
  } else {
    NotifyClientError();
  }
}

void CloudPolicyClient::OnUnregisterCompleted(
    DeviceManagementStatus status,
    const em::DeviceManagementResponse& response) {
  if (status == DM_STATUS_SUCCESS && !response.has_unregister_response()) {
    // Assume unregistration has succeeded either way.
    LOG(WARNING) << "Empty unregistration response.";
  }

  status_ = status;
  if (status == DM_STATUS_SUCCESS) {
    dm_token_.clear();
    NotifyRegistrationStateChanged();
  } else {
    NotifyClientError();
  }
}

void CloudPolicyClient::NotifyPolicyFetched() {
  FOR_EACH_OBSERVER(Observer, observers_, OnPolicyFetched(this));
}

void CloudPolicyClient::NotifyRegistrationStateChanged() {
  FOR_EACH_OBSERVER(Observer, observers_, OnRegistrationStateChanged(this));
}

void CloudPolicyClient::NotifyClientError() {
  FOR_EACH_OBSERVER(Observer, observers_, OnClientError(this));
}

}  // namespace policy
