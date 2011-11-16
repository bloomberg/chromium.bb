// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/device_management_backend_impl.h"

#include <utility>
#include <vector>

#include "base/metrics/histogram.h"
#include "base/stringprintf.h"
#include "base/sys_info.h"
#include "chrome/browser/policy/device_management_service.h"
#include "chrome/browser/policy/enterprise_metrics.h"
#include "chrome/common/chrome_version_info.h"
#include "content/public/common/url_fetcher.h"
#include "net/base/escape.h"
#include "net/url_request/url_request_status.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/system/statistics_provider.h"
#endif

namespace policy {

// Name constants for URL query parameters.
const char DeviceManagementBackendImpl::kParamAgent[] = "agent";
const char DeviceManagementBackendImpl::kParamAppType[] = "apptype";
const char DeviceManagementBackendImpl::kParamDeviceID[] = "deviceid";
const char DeviceManagementBackendImpl::kParamDeviceType[] = "devicetype";
const char DeviceManagementBackendImpl::kParamOAuthToken[] = "oauth_token";
const char DeviceManagementBackendImpl::kParamPlatform[] = "platform";
const char DeviceManagementBackendImpl::kParamRequest[] = "request";
const char DeviceManagementBackendImpl::kParamUserAffiliation[] =
    "user_affiliation";

// String constants for the device and app type we report to the server.
const char DeviceManagementBackendImpl::kValueAppType[] = "Chrome";
const char DeviceManagementBackendImpl::kValueDeviceType[] = "2";
const char DeviceManagementBackendImpl::kValueRequestPolicy[] = "policy";
const char DeviceManagementBackendImpl::kValueRequestRegister[] = "register";
const char DeviceManagementBackendImpl::kValueRequestUnregister[] =
    "unregister";
const char DeviceManagementBackendImpl::kValueUserAffiliationManaged[] =
    "managed";
const char DeviceManagementBackendImpl::kValueUserAffiliationNone[] = "none";

namespace {

const char kValueAgent[] = "%s %s(%s)";
const char kValuePlatform[] = "%s|%s|%s";

const char kPostContentType[] = "application/protobuf";

const char kServiceTokenAuthHeader[] = "Authorization: GoogleLogin auth=";
const char kDMTokenAuthHeader[] = "Authorization: GoogleDMToken token=";

// HTTP Error Codes of the DM Server with their concrete meanings in the context
// of the DM Server communication.
const int kSuccess = 200;
const int kInvalidArgument = 400;
const int kInvalidAuthCookieOrDMToken = 401;
const int kDeviceManagementNotAllowed = 403;
const int kInvalidURL = 404; // This error is not coming from the GFE.
const int kInvalidSerialNumber = 405;
const int kDeviceIdConflict = 409;
const int kDeviceNotFound = 410;
const int kPendingApproval = 412;
const int kInternalServerError = 500;
const int kServiceUnavailable = 503;
const int kPolicyNotFound = 902; // This error is not sent as HTTP status code.

// TODO(pastarmovj): Legacy error codes are here for compatibility only. They
// should be removed once the DM Server has been updated.
const int kPendingApprovalLegacy = 491;
const int kDeviceNotFoundLegacy = 901;

#if defined(OS_CHROMEOS)
// Machine info keys.
const char kMachineInfoHWClass[] = "hardware_class";
const char kMachineInfoBoard[] = "CHROMEOS_RELEASE_BOARD";
#endif

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
    result += net::EscapeQueryParamValue(entry->first, true);
    result += '=';
    result += net::EscapeQueryParamValue(entry->second, true);
  }
  return result;
}

}  // namespace

// A base class containing the common code for the jobs created by the backend
// implementation. Subclasses provide custom code for handling actual register,
// unregister, and policy jobs.
class DeviceManagementJobBase
    : public DeviceManagementService::DeviceManagementJob {
 public:
  virtual ~DeviceManagementJobBase() {}

  // DeviceManagementJob overrides:
  virtual void HandleResponse(const net::URLRequestStatus& status,
                              int response_code,
                              const net::ResponseCookies& cookies,
                              const std::string& data);
  virtual GURL GetURL(const std::string& server_url);
  virtual void ConfigureRequest(content::URLFetcher* fetcher);

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
    query_params_.Put(DeviceManagementBackendImpl::kParamPlatform,
                      DeviceManagementBackendImpl::GetPlatformString());
  }

  void SetQueryParam(const std::string& name, const std::string& value) {
    query_params_.Put(name, value);
  }

  void SetGaiaAuthToken(const std::string& gaia_auth_token) {
    gaia_auth_token_ = gaia_auth_token;
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
  std::string gaia_auth_token_;

  // Device management token (if applicable).
  std::string device_management_token_;

  // The payload.
  std::string payload_;

  DISALLOW_COPY_AND_ASSIGN(DeviceManagementJobBase);
};

void DeviceManagementJobBase::HandleResponse(
    const net::URLRequestStatus& status,
    int response_code,
    const net::ResponseCookies& cookies,
    const std::string& data) {
  // Delete ourselves when this is done.
  scoped_ptr<DeviceManagementJob> scoped_killer(this);
  backend_impl_->JobDone(this);
  backend_impl_ = NULL;

  if (status.status() != net::URLRequestStatus::SUCCESS) {
    OnError(DeviceManagementBackend::kErrorRequestFailed);
    return;
  }

  switch (response_code) {
    case kSuccess: {
      em::DeviceManagementResponse response;
      if (!response.ParseFromString(data)) {
        OnError(DeviceManagementBackend::kErrorResponseDecoding);
        return;
      }
      OnResponse(response);
      return;
    }
    case kInvalidArgument: {
      OnError(DeviceManagementBackend::kErrorRequestInvalid);
      return;
    }
    case kInvalidAuthCookieOrDMToken: {
      OnError(DeviceManagementBackend::kErrorServiceManagementTokenInvalid);
      return;
    }
    case kDeviceManagementNotAllowed: {
      OnError(DeviceManagementBackend::kErrorServiceManagementNotSupported);
      return;
    }
    case kPendingApprovalLegacy:
    case kPendingApproval: {
      OnError(DeviceManagementBackend::kErrorServiceActivationPending);
      return;
    }
    case kInvalidURL:
    case kInternalServerError:
    case kServiceUnavailable: {
      OnError(DeviceManagementBackend::kErrorTemporaryUnavailable);
      return;
    }
    case kDeviceNotFoundLegacy:
    case kDeviceNotFound: {
      OnError(DeviceManagementBackend::kErrorServiceDeviceNotFound);
      return;
    }
    case kPolicyNotFound: {
      OnError(DeviceManagementBackend::kErrorServicePolicyNotFound);
      break;
    }
    case kInvalidSerialNumber: {
      OnError(DeviceManagementBackend::kErrorServiceInvalidSerialNumber);
      break;
    }
    case kDeviceIdConflict: {
      OnError(DeviceManagementBackend::kErrorServiceDeviceIdConflict);
      break;
    }
    default: {
      VLOG(1) << "Unexpected HTTP status in response from DMServer : "
              << response_code << ".";
      // Handle all unknown 5xx HTTP error codes as temporary and any other
      // unknown error as one that needs more time to recover.
      if (response_code >= 500 && response_code <= 599)
        OnError(DeviceManagementBackend::kErrorTemporaryUnavailable);
      else
        OnError(DeviceManagementBackend::kErrorHttpStatus);
      return;
    }
  }
}

GURL DeviceManagementJobBase::GetURL(
    const std::string& server_url) {
  return GURL(server_url + '?' + query_params_.Encode());
}

void DeviceManagementJobBase::ConfigureRequest(content::URLFetcher* fetcher) {
  fetcher->SetUploadData(kPostContentType, payload_);
  std::string extra_headers;
  if (!gaia_auth_token_.empty())
    extra_headers += kServiceTokenAuthHeader + gaia_auth_token_ + "\n";
  if (!device_management_token_.empty())
    extra_headers += kDMTokenAuthHeader + device_management_token_ + "\n";
  fetcher->SetExtraRequestHeaders(extra_headers);
}

// Handles device registration jobs.
class DeviceManagementRegisterJob : public DeviceManagementJobBase {
 public:
  DeviceManagementRegisterJob(
      DeviceManagementBackendImpl* backend_impl,
      const std::string& gaia_auth_token,
      const std::string& oauth_token,
      const std::string& device_id,
      const em::DeviceRegisterRequest& request,
      DeviceManagementBackend::DeviceRegisterResponseDelegate* delegate)
      : DeviceManagementJobBase(
          backend_impl,
          DeviceManagementBackendImpl::kValueRequestRegister,
          device_id),
        delegate_(delegate) {
    if (!oauth_token.empty())
      SetQueryParam(DeviceManagementBackendImpl::kParamOAuthToken, oauth_token);
    else if (!gaia_auth_token.empty())
      SetGaiaAuthToken(gaia_auth_token);
    em::DeviceManagementRequest request_wrapper;
    request_wrapper.mutable_register_request()->CopyFrom(request);
    SetPayload(request_wrapper);
  }
  virtual ~DeviceManagementRegisterJob() {}

 private:
  // DeviceManagementJobBase overrides.
  virtual void OnError(DeviceManagementBackend::ErrorCode error) OVERRIDE {
    MetricToken sample;
    switch (error) {
      case DeviceManagementBackend::kErrorRequestInvalid:
      case DeviceManagementBackend::kErrorRequestFailed:
        sample = kMetricTokenFetchRequestFailed;
        break;
      case DeviceManagementBackend::kErrorServiceDeviceNotFound:
        sample = kMetricTokenFetchDeviceNotFound;
        break;
      case DeviceManagementBackend::kErrorServiceManagementNotSupported:
        sample = kMetricTokenFetchManagementNotSupported;
        break;
      case DeviceManagementBackend::kErrorResponseDecoding:
        sample = kMetricTokenFetchBadResponse;
        break;
      case DeviceManagementBackend::kErrorServiceInvalidSerialNumber:
        sample = kMetricTokenFetchInvalidSerialNumber;
        break;
      case DeviceManagementBackend::kErrorServiceDeviceIdConflict:
        sample = kMetricTokenFetchDeviceIdConflict;
        break;
      default:
        sample = kMetricTokenFetchServerFailed;
        break;
    }
    UMA_HISTOGRAM_ENUMERATION(kMetricToken, sample, kMetricTokenSize);
    delegate_->OnError(error);
  }
  virtual void OnResponse(
      const em::DeviceManagementResponse& response) OVERRIDE {
    UMA_HISTOGRAM_ENUMERATION(kMetricToken, kMetricTokenFetchResponseReceived,
                              kMetricTokenSize);
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
  virtual void OnError(DeviceManagementBackend::ErrorCode error) OVERRIDE {
    delegate_->OnError(error);
  }
  virtual void OnResponse(
      const em::DeviceManagementResponse& response) OVERRIDE {
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
      const std::string& user_affiliation,
      const em::DevicePolicyRequest& request,
      DeviceManagementBackend::DevicePolicyResponseDelegate* delegate)
      : DeviceManagementJobBase(
          backend_impl,
          DeviceManagementBackendImpl::kValueRequestPolicy,
          device_id),
        delegate_(delegate) {
    SetDeviceManagementToken(device_management_token);
    SetQueryParam(DeviceManagementBackendImpl::kParamUserAffiliation,
                  user_affiliation);
    em::DeviceManagementRequest request_wrapper;
    request_wrapper.mutable_policy_request()->CopyFrom(request);
    SetPayload(request_wrapper);
  }
  virtual ~DeviceManagementPolicyJob() {}

 private:
  // DeviceManagementJobBase overrides.
  virtual void OnError(DeviceManagementBackend::ErrorCode error) OVERRIDE {
    MetricPolicy sample;
    switch (error) {
      case DeviceManagementBackend::kErrorRequestInvalid:
      case DeviceManagementBackend::kErrorRequestFailed:
        sample = kMetricPolicyFetchRequestFailed;
        break;
      case DeviceManagementBackend::kErrorServicePolicyNotFound:
        sample = kMetricPolicyFetchNotFound;
        break;
      case DeviceManagementBackend::kErrorServiceManagementTokenInvalid:
        sample = kMetricPolicyFetchInvalidToken;
        break;
      case DeviceManagementBackend::kErrorResponseDecoding:
        sample = kMetricPolicyFetchBadResponse;
        break;
      default:
        sample = kMetricPolicyFetchServerFailed;
        break;
    }
    UMA_HISTOGRAM_ENUMERATION(kMetricPolicy, sample, kMetricPolicySize);
    delegate_->OnError(error);
  }
  virtual void OnResponse(
      const em::DeviceManagementResponse& response) OVERRIDE {
    UMA_HISTOGRAM_ENUMERATION(kMetricPolicy, kMetricPolicyFetchResponseReceived,
                              kMetricPolicySize);
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
  for (JobSet::iterator job(pending_jobs_.begin());
       job != pending_jobs_.end();
       ++job) {
    service_->RemoveJob(*job);
    delete *job;
  }
  pending_jobs_.clear();
}

std::string DeviceManagementBackendImpl::GetAgentString() {
  CR_DEFINE_STATIC_LOCAL(std::string, agent, ());
  if (!agent.empty())
    return agent;

  chrome::VersionInfo version_info;
  agent = base::StringPrintf(kValueAgent,
                             version_info.Name().c_str(),
                             version_info.Version().c_str(),
                             version_info.LastChange().c_str());
  return agent;
}

std::string DeviceManagementBackendImpl::GetPlatformString() {
  CR_DEFINE_STATIC_LOCAL(std::string, platform, ());
  if (!platform.empty())
    return platform;

  std::string os_name(base::SysInfo::OperatingSystemName());
  std::string os_hardware(base::SysInfo::CPUArchitecture());

#if defined(OS_CHROMEOS)
  chromeos::system::StatisticsProvider* provider =
      chromeos::system::StatisticsProvider::GetInstance();

  std::string hwclass;
  std::string board;
  if (!provider->GetMachineStatistic(kMachineInfoHWClass, &hwclass) ||
      !provider->GetMachineStatistic(kMachineInfoBoard, &board)) {
    LOG(ERROR) << "Failed to get machine information";
  }
  os_name += ",CrOS," + board;
  os_hardware += "," + hwclass;
#endif

  std::string os_version("-");
#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_CHROMEOS)
  int32 os_major_version = 0;
  int32 os_minor_version = 0;
  int32 os_bugfix_version = 0;
  base::SysInfo::OperatingSystemVersionNumbers(&os_major_version,
                                               &os_minor_version,
                                               &os_bugfix_version);
  os_version = base::StringPrintf("%d.%d.%d",
                                  os_major_version,
                                  os_minor_version,
                                  os_bugfix_version);
#endif

  platform = base::StringPrintf(kValuePlatform,
                                os_name.c_str(),
                                os_hardware.c_str(),
                                os_version.c_str());
  return platform;
}

void DeviceManagementBackendImpl::JobDone(DeviceManagementJobBase* job) {
  pending_jobs_.erase(job);
}

void DeviceManagementBackendImpl::AddJob(DeviceManagementJobBase* job) {
  pending_jobs_.insert(job);
  service_->AddJob(job);
}

void DeviceManagementBackendImpl::ProcessRegisterRequest(
    const std::string& gaia_auth_token,
    const std::string& oauth_token,
    const std::string& device_id,
    const em::DeviceRegisterRequest& request,
    DeviceRegisterResponseDelegate* delegate) {
  UMA_HISTOGRAM_ENUMERATION(kMetricToken, kMetricTokenFetchRequested,
                            kMetricTokenSize);
  AddJob(new DeviceManagementRegisterJob(this, gaia_auth_token, oauth_token,
                                         device_id, request, delegate));
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
    CloudPolicyDataStore::UserAffiliation affiliation,
    const em::DevicePolicyRequest& request,
    DevicePolicyResponseDelegate* delegate) {
  UMA_HISTOGRAM_ENUMERATION(kMetricPolicy, kMetricPolicyFetchRequested,
                            kMetricPolicySize);
  AddJob(new DeviceManagementPolicyJob(this, device_management_token, device_id,
                                       UserAffiliationToString(affiliation),
                                       request, delegate));
}

// static
const char* DeviceManagementBackendImpl::UserAffiliationToString(
    CloudPolicyDataStore::UserAffiliation affiliation) {
  switch (affiliation) {
    case CloudPolicyDataStore::USER_AFFILIATION_MANAGED:
      return kValueUserAffiliationManaged;
    case CloudPolicyDataStore::USER_AFFILIATION_NONE:
      return kValueUserAffiliationNone;
  }
  NOTREACHED();
  return kValueUserAffiliationNone;
}

}  // namespace policy
