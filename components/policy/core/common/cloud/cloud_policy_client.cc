// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/cloud_policy_client.h"

#include "base/bind.h"
#include "base/guid.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/url_request/url_request_context_getter.h"

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
      return DEVICE_MODE_RETAIL_KIOSK;
  }
  LOG(ERROR) << "Unknown enrollment mode in registration response: " << mode;
  return DEVICE_MODE_NOT_SET;
}

bool IsChromePolicy(const std::string& type) {
  return type == dm_protocol::kChromeDevicePolicyType ||
         type == GetChromeUserPolicyType();
}

}  // namespace

CloudPolicyClient::Observer::~Observer() {}

void CloudPolicyClient::Observer::OnRobotAuthCodesFetched(
    CloudPolicyClient* client) {}

CloudPolicyClient::StatusProvider::~StatusProvider() {}

CloudPolicyClient::CloudPolicyClient(
    const std::string& machine_id,
    const std::string& machine_model,
    const std::string& verification_key_hash,
    UserAffiliation user_affiliation,
    StatusProvider* status_provider,
    DeviceManagementService* service,
    scoped_refptr<net::URLRequestContextGetter> request_context)
    : machine_id_(machine_id),
      machine_model_(machine_model),
      verification_key_hash_(verification_key_hash),
      user_affiliation_(user_affiliation),
      device_mode_(DEVICE_MODE_NOT_SET),
      submit_machine_id_(false),
      public_key_version_(-1),
      public_key_version_valid_(false),
      invalidation_version_(0),
      fetched_invalidation_version_(0),
      service_(service),                  // Can be NULL for unit tests.
      status_provider_(status_provider),  // Can be NULL for unit tests.
      status_(DM_STATUS_SUCCESS),
      request_context_(request_context) {
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
                                 bool is_auto_enrollement,
                                 const std::string& requisition,
                                 const std::string& current_state_key) {
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
      service_->CreateJob(DeviceManagementRequestJob::TYPE_REGISTRATION,
                          GetRequestContext()));
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
  if (!requisition.empty())
    request->set_requisition(requisition);
  if (!current_state_key.empty())
    request->set_server_backed_state_key(current_state_key);

  request_job_->SetRetryCallback(
      base::Bind(&CloudPolicyClient::OnRetryRegister, base::Unretained(this)));

  request_job_->Start(base::Bind(&CloudPolicyClient::OnRegisterCompleted,
                                 base::Unretained(this)));
}

void CloudPolicyClient::SetInvalidationInfo(
    int64 version,
    const std::string& payload) {
  invalidation_version_ = version;
  invalidation_payload_ = payload;
}

void CloudPolicyClient::FetchPolicy() {
  CHECK(is_registered());
  CHECK(!namespaces_to_fetch_.empty());

  request_job_.reset(
      service_->CreateJob(DeviceManagementRequestJob::TYPE_POLICY_FETCH,
                          GetRequestContext()));
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

    // Request signed policy blobs to help prevent tampering on the client.
    fetch_request->set_signature_type(em::PolicyFetchRequest::SHA1_RSA);
    if (public_key_version_valid_)
      fetch_request->set_public_key_version(public_key_version_);

    if (!verification_key_hash_.empty())
      fetch_request->set_verification_key_hash(verification_key_hash_);

    // These fields are included only in requests for chrome policy.
    if (IsChromePolicy(it->first)) {
      if (submit_machine_id_ && !machine_id_.empty())
        fetch_request->set_machine_id(machine_id_);
      if (!last_policy_timestamp_.is_null()) {
        base::TimeDelta timestamp(
            last_policy_timestamp_ - base::Time::UnixEpoch());
        fetch_request->set_timestamp(timestamp.InMilliseconds());
      }
      if (!invalidation_payload_.empty()) {
        fetch_request->set_invalidation_version(invalidation_version_);
        fetch_request->set_invalidation_payload(invalidation_payload_);
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

  // Add device state keys.
  if (!state_keys_to_upload_.empty()) {
    em::DeviceStateKeyUpdateRequest* key_update_request =
        request->mutable_device_state_key_update_request();
    for (std::vector<std::string>::const_iterator key(
             state_keys_to_upload_.begin());
         key != state_keys_to_upload_.end();
         ++key) {
      key_update_request->add_server_backed_state_key(*key);
    }
  }

  // Set the fetched invalidation version to the latest invalidation version
  // since it is now the invalidation version used for the latest fetch.
  fetched_invalidation_version_ = invalidation_version_;

  // Fire the job.
  request_job_->Start(base::Bind(&CloudPolicyClient::OnPolicyFetchCompleted,
                                 base::Unretained(this)));
}

void CloudPolicyClient::FetchRobotAuthCodes(const std::string& auth_token) {
  CHECK(is_registered());
  DCHECK(!auth_token.empty());

  request_job_.reset(service_->CreateJob(
      DeviceManagementRequestJob::TYPE_API_AUTH_CODE_FETCH,
      GetRequestContext()));
  // The credentials of a domain user are needed in order to mint a new OAuth2
  // authorization token for the robot account.
  request_job_->SetOAuthToken(auth_token);
  request_job_->SetDMToken(dm_token_);
  request_job_->SetClientID(client_id_);

  em::DeviceServiceApiAccessRequest* request =
      request_job_->GetRequest()->mutable_service_api_access_request();
  request->set_oauth2_client_id(
      GaiaUrls::GetInstance()->oauth2_chrome_client_id());
  request->add_auth_scope(GaiaConstants::kAnyApiOAuth2Scope);

  request_job_->Start(
      base::Bind(&CloudPolicyClient::OnFetchRobotAuthCodesCompleted,
                 base::Unretained(this)));
}

void CloudPolicyClient::Unregister() {
  DCHECK(service_);
  request_job_.reset(
      service_->CreateJob(DeviceManagementRequestJob::TYPE_UNREGISTRATION,
                          GetRequestContext()));
  request_job_->SetDMToken(dm_token_);
  request_job_->SetClientID(client_id_);
  request_job_->GetRequest()->mutable_unregister_request();
  request_job_->Start(base::Bind(&CloudPolicyClient::OnUnregisterCompleted,
                                 base::Unretained(this)));
}

void CloudPolicyClient::UploadCertificate(
    const std::string& certificate_data,
    const CloudPolicyClient::StatusCallback& callback) {
  CHECK(is_registered());
  request_job_.reset(
      service_->CreateJob(DeviceManagementRequestJob::TYPE_UPLOAD_CERTIFICATE,
                          GetRequestContext()));
  request_job_->SetDMToken(dm_token_);
  request_job_->SetClientID(client_id_);

  em::DeviceManagementRequest* request = request_job_->GetRequest();
  request->mutable_cert_upload_request()->set_device_certificate(
      certificate_data);

  DeviceManagementRequestJob::Callback job_callback = base::Bind(
      &CloudPolicyClient::OnCertificateUploadCompleted,
      base::Unretained(this),
      callback);
  request_job_->Start(job_callback);
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

void CloudPolicyClient::SetStateKeysToUpload(
    const std::vector<std::string>& keys) {
  state_keys_to_upload_ = keys;
}

const em::PolicyFetchResponse* CloudPolicyClient::GetPolicyFor(
    const PolicyNamespaceKey& key) const {
  ResponseMap::const_iterator it = responses_.find(key);
  return it == responses_.end() ? NULL : it->second;
}

scoped_refptr<net::URLRequestContextGetter>
CloudPolicyClient::GetRequestContext() {
  return request_context_;
}

void CloudPolicyClient::OnRetryRegister(DeviceManagementRequestJob* job) {
  DCHECK_EQ(request_job_.get(), job);
  // If the initial request managed to get to the server but the response didn't
  // arrive at the client then retrying with the same client ID will fail.
  // Set the re-registration flag so that the server accepts it.
  // If the server hasn't seen the client ID before then it will also accept
  // the re-registration.
  job->GetRequest()->mutable_register_request()->set_reregister(true);
}

void CloudPolicyClient::OnRegisterCompleted(
    DeviceManagementStatus status,
    int net_error,
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
    DVLOG(1) << "Client registration complete - DMToken = " << dm_token_;

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

void CloudPolicyClient::OnFetchRobotAuthCodesCompleted(
    DeviceManagementStatus status,
    int net_error,
    const em::DeviceManagementResponse& response) {
  if (status == DM_STATUS_SUCCESS &&
      (!response.has_service_api_access_response() ||
       response.service_api_access_response().auth_code().empty())) {
    LOG(WARNING) << "Invalid service api access response.";
    status = DM_STATUS_RESPONSE_DECODING_ERROR;
  }

  status_ = status;
  if (status == DM_STATUS_SUCCESS) {
    robot_api_auth_code_ = response.service_api_access_response().auth_code();
    DVLOG(1) << "Device robot account auth code fetch complete - code = "
             << robot_api_auth_code_;

    NotifyRobotAuthCodesFetched();
  } else {
    NotifyClientError();
  }
}

void CloudPolicyClient::OnPolicyFetchCompleted(
    DeviceManagementStatus status,
    int net_error,
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
    state_keys_to_upload_.clear();
    NotifyPolicyFetched();
  } else {
    NotifyClientError();
  }
}

void CloudPolicyClient::OnUnregisterCompleted(
    DeviceManagementStatus status,
    int net_error,
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

void CloudPolicyClient::OnCertificateUploadCompleted(
    const CloudPolicyClient::StatusCallback& callback,
    DeviceManagementStatus status,
    int net_error,
    const enterprise_management::DeviceManagementResponse& response) {
  if (status == DM_STATUS_SUCCESS && !response.has_cert_upload_response()) {
    LOG(WARNING) << "Empty upload certificate response.";
    callback.Run(false);
    return;
  }

  status_ = status;
  if (status != DM_STATUS_SUCCESS) {
    NotifyClientError();
    callback.Run(false);
    return;
  }
  callback.Run(true);
}

void CloudPolicyClient::NotifyPolicyFetched() {
  FOR_EACH_OBSERVER(Observer, observers_, OnPolicyFetched(this));
}

void CloudPolicyClient::NotifyRegistrationStateChanged() {
  FOR_EACH_OBSERVER(Observer, observers_, OnRegistrationStateChanged(this));
}

void CloudPolicyClient::NotifyRobotAuthCodesFetched() {
  FOR_EACH_OBSERVER(Observer, observers_, OnRobotAuthCodesFetched(this));
}

void CloudPolicyClient::NotifyClientError() {
  FOR_EACH_OBSERVER(Observer, observers_, OnClientError(this));
}

}  // namespace policy
