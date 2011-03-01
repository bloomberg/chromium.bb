// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/device_management_backend_impl.h"

#include <utility>
#include <vector>

#include "base/stringprintf.h"
#include "net/base/escape.h"
#include "net/url_request/url_request_status.h"
#include "chrome/browser/policy/device_management_service.h"
#include "chrome/common/chrome_version_info.h"

namespace policy {

// Name constants for URL query parameters.
const char DeviceManagementBackendImpl::kParamRequest[] = "request";
const char DeviceManagementBackendImpl::kParamDeviceType[] = "devicetype";
const char DeviceManagementBackendImpl::kParamAppType[] = "apptype";
const char DeviceManagementBackendImpl::kParamDeviceID[] = "deviceid";
const char DeviceManagementBackendImpl::kParamAgent[] = "agent";

// String constants for the device and app type we report to the server.
const char DeviceManagementBackendImpl::kValueRequestRegister[] = "register";
const char DeviceManagementBackendImpl::kValueRequestUnregister[] =
    "unregister";
const char DeviceManagementBackendImpl::kValueRequestPolicy[] = "policy";
const char DeviceManagementBackendImpl::kValueDeviceType[] = "2";
const char DeviceManagementBackendImpl::kValueAppType[] = "Chrome";

namespace {

const char kValueAgent[] = "%s enterprise management client %s (%s)";

const char kPostContentType[] = "application/protobuf";

const char kServiceTokenAuthHeader[] = "Authorization: GoogleLogin auth=";
const char kDMTokenAuthHeader[] = "Authorization: GoogleDMToken token=";

}  // namespace

// Helper class for URL query parameter encoding/decoding.
class URLQueryParameters {
 public:
  URLQueryParameters() {}

  // Add a query parameter.
  void Put(const std::string& name, const std::string& value);

  // Produce the query string, taking care of properly encoding and assembling
  // the names and values.
  std::string Encode();

 private:
  typedef std::vector<std::pair<std::string, std::string> > ParameterMap;
  ParameterMap params_;

  DISALLOW_COPY_AND_ASSIGN(URLQueryParameters);
};

void URLQueryParameters::Put(const std::string& name,
                             const std::string& value) {
  params_.push_back(std::make_pair(name, value));
}

std::string URLQueryParameters::Encode() {
  std::string result;
  for (ParameterMap::const_iterator entry(params_.begin());
       entry != params_.end();
       ++entry) {
    if (entry != params_.begin())
      result += '&';
    result += EscapeUrlEncodedData(entry->first);
    result += '=';
    result += EscapeUrlEncodedData(entry->second);
  }
  return result;
}

// A base class containing the common code for the jobs created by the backend
// implementation. Subclasses provide custom code for handling actual register,
// unregister, and policy jobs.
class DeviceManagementJobBase
    : public DeviceManagementService::DeviceManagementJob {
 public:
  virtual ~DeviceManagementJobBase() {
    backend_impl_->JobDone(this);
  }

  // DeviceManagementJob overrides:
  virtual void HandleResponse(const net::URLRequestStatus& status,
                              int response_code,
                              const ResponseCookies& cookies,
                              const std::string& data);
  virtual GURL GetURL(const std::string& server_url);
  virtual void ConfigureRequest(URLFetcher* fetcher);

 protected:
  // Constructs a device management job running for the given backend.
  DeviceManagementJobBase(DeviceManagementBackendImpl* backend_impl,
                          const std::string& request_type,
                          const std::string& device_id)
      : backend_impl_(backend_impl) {
    query_params_.Put(DeviceManagementBackendImpl::kParamRequest, request_type);
    query_params_.Put(DeviceManagementBackendImpl::kParamDeviceType,
                      DeviceManagementBackendImpl::kValueDeviceType);
    query_params_.Put(DeviceManagementBackendImpl::kParamAppType,
                      DeviceManagementBackendImpl::kValueAppType);
    query_params_.Put(DeviceManagementBackendImpl::kParamDeviceID, device_id);
    query_params_.Put(DeviceManagementBackendImpl::kParamAgent,
                      DeviceManagementBackendImpl::GetAgentString());
  }

  void SetQueryParam(const std::string& name, const std::string& value) {
    query_params_.Put(name, value);
  }

  void SetAuthToken(const std::string& auth_token) {
    auth_token_ = auth_token;
  }

  void SetDeviceManagementToken(const std::string& device_management_token) {
    device_management_token_ = device_management_token;
  }

  void SetPayload(const em::DeviceManagementRequest& request) {
    if (!request.SerializeToString(&payload_)) {
      NOTREACHED();
      LOG(ERROR) << "Failed to serialize request.";
    }
  }

 private:
  // Implemented by subclasses to handle decoded responses and errors.
  virtual void OnResponse(
      const em::DeviceManagementResponse& response) = 0;
  virtual void OnError(DeviceManagementBackend::ErrorCode error) = 0;

  // The backend this job is handling a request for.
  DeviceManagementBackendImpl* backend_impl_;

  // Query parameters.
  URLQueryParameters query_params_;

  // Auth token (if applicaple).
  std::string auth_token_;

  // Device management token (if applicable).
  std::string device_management_token_;

  // The payload.
  std::string payload_;

  DISALLOW_COPY_AND_ASSIGN(DeviceManagementJobBase);
};

void DeviceManagementJobBase::HandleResponse(
    const net::URLRequestStatus& status,
    int response_code,
    const ResponseCookies& cookies,
    const std::string& data) {
  // Delete ourselves when this is done.
  scoped_ptr<DeviceManagementJob> scoped_killer(this);

  if (status.status() != net::URLRequestStatus::SUCCESS) {
    OnError(DeviceManagementBackend::kErrorRequestFailed);
    return;
  }

  if (response_code != 200) {
    if (response_code == 400)
      OnError(DeviceManagementBackend::kErrorRequestInvalid);
    else
      OnError(DeviceManagementBackend::kErrorHttpStatus);
    return;
  }

  em::DeviceManagementResponse response;
  if (!response.ParseFromString(data)) {
    OnError(DeviceManagementBackend::kErrorResponseDecoding);
    return;
  }

  // Check service error code.
  switch (response.error()) {
    case em::DeviceManagementResponse::SUCCESS:
      OnResponse(response);
      return;
    case em::DeviceManagementResponse::DEVICE_MANAGEMENT_NOT_SUPPORTED:
      OnError(DeviceManagementBackend::kErrorServiceManagementNotSupported);
      return;
    case em::DeviceManagementResponse::DEVICE_NOT_FOUND:
      OnError(DeviceManagementBackend::kErrorServiceDeviceNotFound);
      return;
    case em::DeviceManagementResponse::DEVICE_MANAGEMENT_TOKEN_INVALID:
      OnError(DeviceManagementBackend::kErrorServiceManagementTokenInvalid);
      return;
    case em::DeviceManagementResponse::ACTIVATION_PENDING:
      OnError(DeviceManagementBackend::kErrorServiceActivationPending);
      return;
    case em::DeviceManagementResponse::POLICY_NOT_FOUND:
      OnError(DeviceManagementBackend::kErrorServicePolicyNotFound);
      return;
  }

  // This should be caught by the protobuf decoder.
  NOTREACHED();
  OnError(DeviceManagementBackend::kErrorResponseDecoding);
}

GURL DeviceManagementJobBase::GetURL(
    const std::string& server_url) {
  return GURL(server_url + '?' + query_params_.Encode());
}

void DeviceManagementJobBase::ConfigureRequest(URLFetcher* fetcher) {
  fetcher->set_upload_data(kPostContentType, payload_);
  std::string extra_headers;
  if (!auth_token_.empty())
    extra_headers += kServiceTokenAuthHeader + auth_token_ + "\n";
  if (!device_management_token_.empty())
    extra_headers += kDMTokenAuthHeader + device_management_token_ + "\n";
  fetcher->set_extra_request_headers(extra_headers);
}

// Handles device registration jobs.
class DeviceManagementRegisterJob : public DeviceManagementJobBase {
 public:
  DeviceManagementRegisterJob(
      DeviceManagementBackendImpl* backend_impl,
      const std::string& auth_token,
      const std::string& device_id,
      const em::DeviceRegisterRequest& request,
      DeviceManagementBackend::DeviceRegisterResponseDelegate* delegate)
      : DeviceManagementJobBase(
          backend_impl,
          DeviceManagementBackendImpl::kValueRequestRegister,
          device_id),
        delegate_(delegate) {
    SetAuthToken(auth_token);
    em::DeviceManagementRequest request_wrapper;
    request_wrapper.mutable_register_request()->CopyFrom(request);
    SetPayload(request_wrapper);
  }
  virtual ~DeviceManagementRegisterJob() {}

 private:
  // DeviceManagementJobBase overrides.
  virtual void OnError(DeviceManagementBackend::ErrorCode error) {
    delegate_->OnError(error);
  }
  virtual void OnResponse(const em::DeviceManagementResponse& response) {
    delegate_->HandleRegisterResponse(response.register_response());
  }

  DeviceManagementBackend::DeviceRegisterResponseDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(DeviceManagementRegisterJob);
};

// Handles device unregistration jobs.
class DeviceManagementUnregisterJob : public DeviceManagementJobBase {
 public:
  DeviceManagementUnregisterJob(
      DeviceManagementBackendImpl* backend_impl,
      const std::string& device_management_token,
      const std::string& device_id,
      const em::DeviceUnregisterRequest& request,
      DeviceManagementBackend::DeviceUnregisterResponseDelegate* delegate)
      : DeviceManagementJobBase(
          backend_impl,
          DeviceManagementBackendImpl::kValueRequestUnregister,
          device_id),
        delegate_(delegate) {
    SetDeviceManagementToken(device_management_token);
    em::DeviceManagementRequest request_wrapper;
    request_wrapper.mutable_unregister_request()->CopyFrom(request);
    SetPayload(request_wrapper);
  }
  virtual ~DeviceManagementUnregisterJob() {}

 private:
  // DeviceManagementJobBase overrides.
  virtual void OnError(DeviceManagementBackend::ErrorCode error) {
    delegate_->OnError(error);
  }
  virtual void OnResponse(const em::DeviceManagementResponse& response) {
    delegate_->HandleUnregisterResponse(response.unregister_response());
  }

  DeviceManagementBackend::DeviceUnregisterResponseDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(DeviceManagementUnregisterJob);
};

// Handles policy request jobs.
class DeviceManagementPolicyJob : public DeviceManagementJobBase {
 public:
  DeviceManagementPolicyJob(
      DeviceManagementBackendImpl* backend_impl,
      const std::string& device_management_token,
      const std::string& device_id,
      const em::DevicePolicyRequest& request,
      DeviceManagementBackend::DevicePolicyResponseDelegate* delegate)
      : DeviceManagementJobBase(
          backend_impl,
          DeviceManagementBackendImpl::kValueRequestPolicy,
          device_id),
        delegate_(delegate) {
    SetDeviceManagementToken(device_management_token);
    em::DeviceManagementRequest request_wrapper;
    request_wrapper.mutable_policy_request()->CopyFrom(request);
    SetPayload(request_wrapper);
  }
  virtual ~DeviceManagementPolicyJob() {}

 private:
  // DeviceManagementJobBase overrides.
  virtual void OnError(DeviceManagementBackend::ErrorCode error) {
    delegate_->OnError(error);
  }
  virtual void OnResponse(const em::DeviceManagementResponse& response) {
    delegate_->HandlePolicyResponse(response.policy_response());
  }

  DeviceManagementBackend::DevicePolicyResponseDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(DeviceManagementPolicyJob);
};

DeviceManagementBackendImpl::DeviceManagementBackendImpl(
    DeviceManagementService* service)
    : service_(service) {
}

DeviceManagementBackendImpl::~DeviceManagementBackendImpl() {
  // Swap to a helper, so we don't interfere with the unregistration on delete.
  JobSet to_be_deleted;
  to_be_deleted.swap(pending_jobs_);
  for (JobSet::iterator job(to_be_deleted.begin());
       job != to_be_deleted.end();
       ++job) {
    service_->RemoveJob(*job);
    delete *job;
  }
}

std::string DeviceManagementBackendImpl::GetAgentString() {
  chrome::VersionInfo version_info;
  return base::StringPrintf(kValueAgent,
                            version_info.Name().c_str(),
                            version_info.Version().c_str(),
                            version_info.LastChange().c_str());
}

void DeviceManagementBackendImpl::JobDone(DeviceManagementJobBase* job) {
  pending_jobs_.erase(job);
}

void DeviceManagementBackendImpl::AddJob(DeviceManagementJobBase* job) {
  pending_jobs_.insert(job);
  service_->AddJob(job);
}

void DeviceManagementBackendImpl::ProcessRegisterRequest(
    const std::string& auth_token,
    const std::string& device_id,
    const em::DeviceRegisterRequest& request,
    DeviceRegisterResponseDelegate* delegate) {
  AddJob(new DeviceManagementRegisterJob(this, auth_token, device_id, request,
                                         delegate));
}

void DeviceManagementBackendImpl::ProcessUnregisterRequest(
    const std::string& device_management_token,
    const std::string& device_id,
    const em::DeviceUnregisterRequest& request,
    DeviceUnregisterResponseDelegate* delegate) {
  AddJob(new DeviceManagementUnregisterJob(this, device_management_token,
                                           device_id, request, delegate));
}

void DeviceManagementBackendImpl::ProcessPolicyRequest(
    const std::string& device_management_token,
    const std::string& device_id,
    const em::DevicePolicyRequest& request,
    DevicePolicyResponseDelegate* delegate) {
  AddJob(new DeviceManagementPolicyJob(this, device_management_token, device_id,
                                       request, delegate));
}

}  // namespace policy
