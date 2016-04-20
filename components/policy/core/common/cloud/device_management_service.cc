// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/device_management_service.h"

#include <utility>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "components/data_use_measurement/core/data_use_user_data.h"
#include "net/base/escape.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_status.h"
#include "url/gurl.h"

namespace em = enterprise_management;

namespace policy {

namespace {

const char kPostContentType[] = "application/protobuf";

const char kServiceTokenAuthHeader[] = "Authorization: GoogleLogin auth=";
const char kDMTokenAuthHeader[] = "Authorization: GoogleDMToken token=";

// Number of times to retry on ERR_NETWORK_CHANGED errors.
const int kMaxNetworkChangedRetries = 3;

// HTTP Error Codes of the DM Server with their concrete meanings in the context
// of the DM Server communication.
const int kSuccess = 200;
const int kInvalidArgument = 400;
const int kInvalidAuthCookieOrDMToken = 401;
const int kMissingLicenses = 402;
const int kDeviceManagementNotAllowed = 403;
const int kInvalidURL = 404;  // This error is not coming from the GFE.
const int kInvalidSerialNumber = 405;
const int kDomainMismatch = 406;
const int kDeviceIdConflict = 409;
const int kDeviceNotFound = 410;
const int kPendingApproval = 412;
const int kInternalServerError = 500;
const int kServiceUnavailable = 503;
const int kPolicyNotFound = 902;
const int kDeprovisioned = 903;

bool IsProxyError(const net::URLRequestStatus status) {
  switch (status.error()) {
    case net::ERR_PROXY_CONNECTION_FAILED:
    case net::ERR_TUNNEL_CONNECTION_FAILED:
    case net::ERR_PROXY_AUTH_UNSUPPORTED:
    case net::ERR_HTTPS_PROXY_TUNNEL_RESPONSE:
    case net::ERR_MANDATORY_PROXY_CONFIGURATION_FAILED:
    case net::ERR_PROXY_CERTIFICATE_INVALID:
    case net::ERR_SOCKS_CONNECTION_FAILED:
    case net::ERR_SOCKS_CONNECTION_HOST_UNREACHABLE:
      return true;
  }
  return false;
}

bool IsProtobufMimeType(const net::URLFetcher* fetcher) {
  return fetcher->GetResponseHeaders()->HasHeaderValue(
      "content-type", "application/x-protobuffer");
}

bool FailedWithProxy(const net::URLFetcher* fetcher) {
  if ((fetcher->GetLoadFlags() & net::LOAD_BYPASS_PROXY) != 0) {
    // The request didn't use a proxy.
    return false;
  }

  if (!fetcher->GetStatus().is_success() &&
      IsProxyError(fetcher->GetStatus())) {
    LOG(WARNING) << "Proxy failed while contacting dmserver.";
    return true;
  }

  if (fetcher->GetStatus().is_success() &&
      fetcher->GetResponseCode() == kSuccess &&
      fetcher->WasFetchedViaProxy() &&
      !IsProtobufMimeType(fetcher)) {
    // The proxy server can be misconfigured but pointing to an existing
    // server that replies to requests. Try to recover if a successful
    // request that went through a proxy returns an unexpected mime type.
    LOG(WARNING) << "Got bad mime-type in response from dmserver that was "
                 << "fetched via a proxy.";
    return true;
  }

  return false;
}

const char* JobTypeToRequestType(DeviceManagementRequestJob::JobType type) {
  switch (type) {
    case DeviceManagementRequestJob::TYPE_AUTO_ENROLLMENT:
      return dm_protocol::kValueRequestAutoEnrollment;
    case DeviceManagementRequestJob::TYPE_REGISTRATION:
      return dm_protocol::kValueRequestRegister;
    case DeviceManagementRequestJob::TYPE_POLICY_FETCH:
      return dm_protocol::kValueRequestPolicy;
    case DeviceManagementRequestJob::TYPE_API_AUTH_CODE_FETCH:
      return dm_protocol::kValueRequestApiAuthorization;
    case DeviceManagementRequestJob::TYPE_UNREGISTRATION:
      return dm_protocol::kValueRequestUnregister;
    case DeviceManagementRequestJob::TYPE_UPLOAD_CERTIFICATE:
      return dm_protocol::kValueRequestUploadCertificate;
    case DeviceManagementRequestJob::TYPE_DEVICE_STATE_RETRIEVAL:
      return dm_protocol::kValueRequestDeviceStateRetrieval;
    case DeviceManagementRequestJob::TYPE_UPLOAD_STATUS:
      return dm_protocol::kValueRequestUploadStatus;
    case DeviceManagementRequestJob::TYPE_REMOTE_COMMANDS:
      return dm_protocol::kValueRequestRemoteCommands;
    case DeviceManagementRequestJob::TYPE_ATTRIBUTE_UPDATE_PERMISSION:
      return dm_protocol::kValueRequestDeviceAttributeUpdatePermission;
    case DeviceManagementRequestJob::TYPE_ATTRIBUTE_UPDATE:
      return dm_protocol::kValueRequestDeviceAttributeUpdate;
    case DeviceManagementRequestJob::TYPE_GCM_ID_UPDATE:
      return dm_protocol::kValueRequestGcmIdUpdate;
  }
  NOTREACHED() << "Invalid job type " << type;
  return "";
}

}  // namespace

// Request job implementation used with DeviceManagementService.
class DeviceManagementRequestJobImpl : public DeviceManagementRequestJob {
 public:
  DeviceManagementRequestJobImpl(
      JobType type,
      const std::string& agent_parameter,
      const std::string& platform_parameter,
      DeviceManagementService* service,
      const scoped_refptr<net::URLRequestContextGetter>& request_context);
  ~DeviceManagementRequestJobImpl() override;

  // Handles the URL request response.
  void HandleResponse(const net::URLRequestStatus& status,
                      int response_code,
                      const net::ResponseCookies& cookies,
                      const std::string& data);

  // Gets the URL to contact.
  GURL GetURL(const std::string& server_url);

  // Configures the fetcher, setting up payload and headers.
  void ConfigureRequest(net::URLFetcher* fetcher);

  // Returns true if this job should be retried. |fetcher| has just completed,
  // and can be inspected to determine if the request failed and should be
  // retried.
  bool ShouldRetry(const net::URLFetcher* fetcher);

  // Invoked right before retrying this job.
  void PrepareRetry();

 protected:
  // DeviceManagementRequestJob:
  void Run() override;

 private:
  // Invokes the callback with the given error code.
  void ReportError(DeviceManagementStatus code);

  // Pointer to the service this job is associated with.
  DeviceManagementService* service_;

  // Whether the BYPASS_PROXY flag should be set by ConfigureRequest().
  bool bypass_proxy_;

  // Number of times that this job has been retried due to ERR_NETWORK_CHANGED.
  int retries_count_;

  // The request context to use for this job.
  scoped_refptr<net::URLRequestContextGetter> request_context_;

  DISALLOW_COPY_AND_ASSIGN(DeviceManagementRequestJobImpl);
};

DeviceManagementRequestJobImpl::DeviceManagementRequestJobImpl(
    JobType type,
    const std::string& agent_parameter,
    const std::string& platform_parameter,
    DeviceManagementService* service,
    const scoped_refptr<net::URLRequestContextGetter>& request_context)
    : DeviceManagementRequestJob(type, agent_parameter, platform_parameter),
      service_(service),
      bypass_proxy_(false),
      retries_count_(0),
      request_context_(request_context) {
}

DeviceManagementRequestJobImpl::~DeviceManagementRequestJobImpl() {
  service_->RemoveJob(this);
}

void DeviceManagementRequestJobImpl::Run() {
  service_->AddJob(this);
}

void DeviceManagementRequestJobImpl::HandleResponse(
    const net::URLRequestStatus& status,
    int response_code,
    const net::ResponseCookies& cookies,
    const std::string& data) {
  if (status.status() != net::URLRequestStatus::SUCCESS) {
    LOG(WARNING) << "DMServer request failed, status: " << status.status()
                 << ", error: " << status.error();
    em::DeviceManagementResponse dummy_response;
    callback_.Run(DM_STATUS_REQUEST_FAILED, status.error(), dummy_response);
    return;
  }

  if (response_code != kSuccess)
    LOG(WARNING) << "DMServer sent an error response: " << response_code;

  switch (response_code) {
    case kSuccess: {
      em::DeviceManagementResponse response;
      if (!response.ParseFromString(data)) {
        ReportError(DM_STATUS_RESPONSE_DECODING_ERROR);
        return;
      }
      callback_.Run(DM_STATUS_SUCCESS, net::OK, response);
      return;
    }
    case kInvalidArgument:
      ReportError(DM_STATUS_REQUEST_INVALID);
      return;
    case kInvalidAuthCookieOrDMToken:
      ReportError(DM_STATUS_SERVICE_MANAGEMENT_TOKEN_INVALID);
      return;
    case kMissingLicenses:
      ReportError(DM_STATUS_SERVICE_MISSING_LICENSES);
      return;
    case kDeviceManagementNotAllowed:
      ReportError(DM_STATUS_SERVICE_MANAGEMENT_NOT_SUPPORTED);
      return;
    case kPendingApproval:
      ReportError(DM_STATUS_SERVICE_ACTIVATION_PENDING);
      return;
    case kInvalidURL:
    case kInternalServerError:
    case kServiceUnavailable:
      ReportError(DM_STATUS_TEMPORARY_UNAVAILABLE);
      return;
    case kDeviceNotFound:
      ReportError(DM_STATUS_SERVICE_DEVICE_NOT_FOUND);
      return;
    case kPolicyNotFound:
      ReportError(DM_STATUS_SERVICE_POLICY_NOT_FOUND);
      return;
    case kInvalidSerialNumber:
      ReportError(DM_STATUS_SERVICE_INVALID_SERIAL_NUMBER);
      return;
    case kDomainMismatch:
      ReportError(DM_STATUS_SERVICE_DOMAIN_MISMATCH);
      return;
    case kDeprovisioned:
      ReportError(DM_STATUS_SERVICE_DEPROVISIONED);
      return;
    case kDeviceIdConflict:
      ReportError(DM_STATUS_SERVICE_DEVICE_ID_CONFLICT);
      return;
    default:
      // Handle all unknown 5xx HTTP error codes as temporary and any other
      // unknown error as one that needs more time to recover.
      if (response_code >= 500 && response_code <= 599)
        ReportError(DM_STATUS_TEMPORARY_UNAVAILABLE);
      else
        ReportError(DM_STATUS_HTTP_STATUS_ERROR);
      return;
  }
}

GURL DeviceManagementRequestJobImpl::GetURL(
    const std::string& server_url) {
  std::string result(server_url);
  result += '?';
  for (ParameterMap::const_iterator entry(query_params_.begin());
       entry != query_params_.end();
       ++entry) {
    if (entry != query_params_.begin())
      result += '&';
    result += net::EscapeQueryParamValue(entry->first, true);
    result += '=';
    result += net::EscapeQueryParamValue(entry->second, true);
  }
  return GURL(result);
}

void DeviceManagementRequestJobImpl::ConfigureRequest(
    net::URLFetcher* fetcher) {
  // TODO(dcheng): It might make sense to make this take a const
  // scoped_refptr<T>& too eventually.
  fetcher->SetRequestContext(request_context_.get());
  fetcher->SetLoadFlags(net::LOAD_DO_NOT_SEND_COOKIES |
                        net::LOAD_DO_NOT_SAVE_COOKIES |
                        net::LOAD_DISABLE_CACHE |
                        (bypass_proxy_ ? net::LOAD_BYPASS_PROXY : 0));
  std::string payload;
  CHECK(request_.SerializeToString(&payload));
  fetcher->SetUploadData(kPostContentType, payload);
  std::string extra_headers;
  if (!gaia_token_.empty())
    extra_headers += kServiceTokenAuthHeader + gaia_token_ + "\n";
  if (!dm_token_.empty())
    extra_headers += kDMTokenAuthHeader + dm_token_ + "\n";
  fetcher->SetExtraRequestHeaders(extra_headers);
}

bool DeviceManagementRequestJobImpl::ShouldRetry(
    const net::URLFetcher* fetcher) {
  if (FailedWithProxy(fetcher) && !bypass_proxy_) {
    // Retry the job if it failed due to a broken proxy, by bypassing the
    // proxy on the next try.
    bypass_proxy_ = true;
    return true;
  }

  // Early device policy fetches on ChromeOS and Auto-Enrollment checks are
  // often interrupted during ChromeOS startup when network change notifications
  // are sent. Allowing the fetcher to retry once after that is enough to
  // recover; allow it to retry up to 3 times just in case.
  if (fetcher->GetStatus().error() == net::ERR_NETWORK_CHANGED &&
      retries_count_ < kMaxNetworkChangedRetries) {
    ++retries_count_;
    return true;
  }

  // The request didn't fail, or the limit of retry attempts has been reached;
  // forward the result to the job owner.
  return false;
}

void DeviceManagementRequestJobImpl::PrepareRetry() {
  if (!retry_callback_.is_null())
    retry_callback_.Run(this);
}

void DeviceManagementRequestJobImpl::ReportError(DeviceManagementStatus code) {
  em::DeviceManagementResponse dummy_response;
  callback_.Run(code, net::OK, dummy_response);
}

DeviceManagementRequestJob::~DeviceManagementRequestJob() {}

void DeviceManagementRequestJob::SetGaiaToken(const std::string& gaia_token) {
  gaia_token_ = gaia_token;
}

void DeviceManagementRequestJob::SetOAuthToken(const std::string& oauth_token) {
  AddParameter(dm_protocol::kParamOAuthToken, oauth_token);
}

void DeviceManagementRequestJob::SetDMToken(const std::string& dm_token) {
  dm_token_ = dm_token;
}

void DeviceManagementRequestJob::SetClientID(const std::string& client_id) {
  AddParameter(dm_protocol::kParamDeviceID, client_id);
}

em::DeviceManagementRequest* DeviceManagementRequestJob::GetRequest() {
  return &request_;
}

DeviceManagementRequestJob::DeviceManagementRequestJob(
    JobType type,
    const std::string& agent_parameter,
    const std::string& platform_parameter) {
  AddParameter(dm_protocol::kParamRequest, JobTypeToRequestType(type));
  AddParameter(dm_protocol::kParamDeviceType, dm_protocol::kValueDeviceType);
  AddParameter(dm_protocol::kParamAppType, dm_protocol::kValueAppType);
  AddParameter(dm_protocol::kParamAgent, agent_parameter);
  AddParameter(dm_protocol::kParamPlatform, platform_parameter);
}

void DeviceManagementRequestJob::SetRetryCallback(
    const RetryCallback& retry_callback) {
  retry_callback_ = retry_callback;
}

void DeviceManagementRequestJob::Start(const Callback& callback) {
  callback_ = callback;
  Run();
}

void DeviceManagementRequestJob::AddParameter(const std::string& name,
                                              const std::string& value) {
  query_params_.push_back(std::make_pair(name, value));
}

// A random value that other fetchers won't likely use.
const int DeviceManagementService::kURLFetcherID = 0xde71ce1d;

DeviceManagementService::~DeviceManagementService() {
  // All running jobs should have been cancelled by now.
  DCHECK(pending_jobs_.empty());
  DCHECK(queued_jobs_.empty());
}

DeviceManagementRequestJob* DeviceManagementService::CreateJob(
    DeviceManagementRequestJob::JobType type,
    const scoped_refptr<net::URLRequestContextGetter>& request_context) {
  return new DeviceManagementRequestJobImpl(
      type,
      configuration_->GetAgentParameter(),
      configuration_->GetPlatformParameter(),
      this,
      request_context);
}

void DeviceManagementService::ScheduleInitialization(
    int64_t delay_milliseconds) {
  if (initialized_)
    return;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::Bind(&DeviceManagementService::Initialize,
                            weak_ptr_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(delay_milliseconds));
}

void DeviceManagementService::Initialize() {
  if (initialized_)
    return;
  initialized_ = true;

  while (!queued_jobs_.empty()) {
    StartJob(queued_jobs_.front());
    queued_jobs_.pop_front();
  }
}

void DeviceManagementService::Shutdown() {
  for (JobFetcherMap::iterator job(pending_jobs_.begin());
       job != pending_jobs_.end();
       ++job) {
    delete job->first;
    queued_jobs_.push_back(job->second);
  }
  pending_jobs_.clear();
}

DeviceManagementService::DeviceManagementService(
    std::unique_ptr<Configuration> configuration)
    : configuration_(std::move(configuration)),
      initialized_(false),
      weak_ptr_factory_(this) {
  DCHECK(configuration_);
}

void DeviceManagementService::StartJob(DeviceManagementRequestJobImpl* job) {
  std::string server_url = GetServerUrl();
  net::URLFetcher* fetcher =
      net::URLFetcher::Create(kURLFetcherID, job->GetURL(server_url),
                              net::URLFetcher::POST, this).release();
  data_use_measurement::DataUseUserData::AttachToFetcher(
      fetcher, data_use_measurement::DataUseUserData::POLICY);
  job->ConfigureRequest(fetcher);
  pending_jobs_[fetcher] = job;
  fetcher->Start();
}

std::string DeviceManagementService::GetServerUrl() {
  return configuration_->GetServerUrl();
}

void DeviceManagementService::OnURLFetchComplete(
    const net::URLFetcher* source) {
  JobFetcherMap::iterator entry(pending_jobs_.find(source));
  if (entry == pending_jobs_.end()) {
    NOTREACHED() << "Callback from foreign URL fetcher";
    return;
  }

  DeviceManagementRequestJobImpl* job = entry->second;
  pending_jobs_.erase(entry);

  if (job->ShouldRetry(source)) {
    VLOG(1) << "Retrying dmserver request.";
    job->PrepareRetry();
    StartJob(job);
  } else {
    std::string data;
    source->GetResponseAsString(&data);
    job->HandleResponse(source->GetStatus(), source->GetResponseCode(),
                        source->GetCookies(), data);
  }
  delete source;
}

void DeviceManagementService::AddJob(DeviceManagementRequestJobImpl* job) {
  if (initialized_)
    StartJob(job);
  else
    queued_jobs_.push_back(job);
}

void DeviceManagementService::RemoveJob(DeviceManagementRequestJobImpl* job) {
  for (JobFetcherMap::iterator entry(pending_jobs_.begin());
       entry != pending_jobs_.end();
       ++entry) {
    if (entry->second == job) {
      delete entry->first;
      pending_jobs_.erase(entry);
      return;
    }
  }

  const JobQueue::iterator elem =
      std::find(queued_jobs_.begin(), queued_jobs_.end(), job);
  if (elem != queued_jobs_.end())
    queued_jobs_.erase(elem);
}

}  // namespace policy
