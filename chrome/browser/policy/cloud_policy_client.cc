// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud_policy_client.h"

#include "base/bind.h"
#include "base/guid.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "chrome/browser/policy/device_management_service.h"

namespace em = enterprise_management;

namespace policy {

namespace {

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
  return DEVICE_MODE_NOT_SET;
}

bool IsChromePolicy(const std::string& type) {
  return type == dm_protocol::kChromeDevicePolicyType ||
         type == dm_protocol::kChromeUserPolicyType;
}

}  // namespace

CloudPolicyClient::Observer::~Observer() {}

CloudPolicyClient::StatusProvider::~StatusProvider() {}

CloudPolicyClient::CloudPolicyClient(const std::string& machine_id,
                                     const std::string& machine_model,
                                     UserAffiliation user_affiliation,
                                     StatusProvider* status_provider,
                                     DeviceManagementService* service)
    : machine_id_(machine_id),
      machine_model_(machine_model),
      user_affiliation_(user_affiliation),
      device_mode_(DEVICE_MODE_NOT_SET),
      submit_machine_id_(false),
      public_key_version_(-1),
      public_key_version_valid_(false),
      service_(service),                  // Can be NULL for unit tests.
      status_provider_(status_provider),  // Can be NULL for unit tests.
      status_(DM_STATUS_SUCCESS) {
}

CloudPolicyClient::~CloudPolicyClient() {
  STLDeleteValues(&responses_);
}

void CloudPolicyClient::SetupRegistration(const std::string& dm_token,
                                          const std::string& client_id) {
  DCHECK(!dm_token.empty());
  DCHECK(!client_id.empty());
  DCHECK(!is_registered());

  dm_token_ = dm_token;
  client_id_ = client_id;
  request_job_.reset();
  STLDeleteValues(&responses_);

  NotifyRegistrationStateChanged();
}

void CloudPolicyClient::Register(em::DeviceRegisterRequest::Type type,
                                 const std::string& auth_token,
                                 const std::string& client_id,
                                 bool is_auto_enrollement) {
  DCHECK(service_);
  DCHECK(!auth_token.empty());
  DCHECK(!is_registered());

  if (client_id.empty()) {
    // Generate a new client ID. This is intentionally done on each new
    // registration request in order to preserve privacy. Reusing IDs would mean
    // the server could track clients by their registration attempts.
    client_id_ = base::GenerateGUID();
  } else {
    client_id_ = client_id;
  }

  request_job_.reset(
      service_->CreateJob(DeviceManagementRequestJob::TYPE_REGISTRATION));
  request_job_->SetOAuthToken(auth_token);
  request_job_->SetClientID(client_id_);

  em::DeviceRegisterRequest* request =
      request_job_->GetRequest()->mutable_register_request();
  if (!client_id.empty())
    request->set_reregister(true);
  request->set_type(type);
  if (!machine_id_.empty())
    request->set_machine_id(machine_id_);
  if (!machine_model_.empty())
    request->set_machine_model(machine_model_);
  if (is_auto_enrollement)
    request->set_auto_enrolled(true);

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

  // Build policy fetch requests.
  em::DevicePolicyRequest* policy_request = request->mutable_policy_request();
  for (NamespaceSet::iterator it = namespaces_to_fetch_.begin();
       it != namespaces_to_fetch_.end(); ++it) {
    em::PolicyFetchRequest* fetch_request = policy_request->add_request();
    fetch_request->set_policy_type(it->first);
    if (!it->second.empty())
      fetch_request->set_settings_entity_id(it->second);

    // All policy types ask for a signed policy blob.
    fetch_request->set_signature_type(em::PolicyFetchRequest::SHA1_RSA);
    if (public_key_version_valid_)
      fetch_request->set_public_key_version(public_key_version_);

    // These fields are included only in requests for chrome policy.
    if (IsChromePolicy(it->first)) {
      if (submit_machine_id_ && !machine_id_.empty())
        fetch_request->set_machine_id(machine_id_);
      if (!last_policy_timestamp_.is_null()) {
        base::TimeDelta timestamp(
            last_policy_timestamp_ - base::Time::UnixEpoch());
        fetch_request->set_timestamp(timestamp.InMilliseconds());
      }
    }
  }

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
  DCHECK(service_);
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

void CloudPolicyClient::AddNamespaceToFetch(const PolicyNamespaceKey& key) {
  namespaces_to_fetch_.insert(key);
}

void CloudPolicyClient::RemoveNamespaceToFetch(const PolicyNamespaceKey& key) {
  namespaces_to_fetch_.erase(key);
}

const em::PolicyFetchResponse* CloudPolicyClient::GetPolicyFor(
    const PolicyNamespaceKey& key) const {
  ResponseMap::const_iterator it = responses_.find(key);
  return it == responses_.end() ? NULL : it->second;
}

void CloudPolicyClient::OnRegisterCompleted(
    DeviceManagementStatus status,
    const em::DeviceManagementResponse& response) {
  if (status == DM_STATUS_SUCCESS &&
      (!response.has_register_response() ||
       !response.register_response().has_device_management_token())) {
    LOG(WARNING) << "Invalid registration response.";
    status = DM_STATUS_RESPONSE_DECODING_ERROR;
  }

  status_ = status;
  if (status == DM_STATUS_SUCCESS) {
    dm_token_ = response.register_response().device_management_token();

    // Device mode is only relevant for device policy really, it's the
    // responsibility of the consumer of the field to check validity.
    device_mode_ = DEVICE_MODE_NOT_SET;
    if (response.register_response().has_enrollment_type()) {
      device_mode_ = TranslateProtobufDeviceMode(
          response.register_response().enrollment_type());
    }

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
    }
  }

  status_ = status;
  if (status == DM_STATUS_SUCCESS) {
    const em::DevicePolicyResponse& policy_response =
        response.policy_response();
    STLDeleteValues(&responses_);
    for (int i = 0; i < policy_response.response_size(); ++i) {
      const em::PolicyFetchResponse& response = policy_response.response(i);
      em::PolicyData policy_data;
      if (!policy_data.ParseFromString(response.policy_data()) ||
          !policy_data.IsInitialized() ||
          !policy_data.has_policy_type()) {
        LOG(WARNING) << "Invalid PolicyData received, ignoring";
        continue;
      }
      const std::string& type = policy_data.policy_type();
      std::string entity_id;
      if (policy_data.has_settings_entity_id())
        entity_id = policy_data.settings_entity_id();
      PolicyNamespaceKey key(type, entity_id);
      if (ContainsKey(responses_, key)) {
        LOG(WARNING) << "Duplicate PolicyFetchResponse for type: "
            << type << ", entity: " << entity_id << ", ignoring";
        continue;
      }
      responses_[key] = new em::PolicyFetchResponse(response);
    }
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
