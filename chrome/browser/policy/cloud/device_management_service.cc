// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud/device_management_service.h"

#include <utility>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "base/stringprintf.h"
#include "base/sys_info.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/basic_http_user_agent_settings.h"
#include "chrome/browser/net/chrome_net_log.h"
#include "chrome/common/chrome_version_info.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_client.h"
#include "googleurl/src/gurl.h"
#include "net/base/escape.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/cookies/cookie_monster.h"
#include "net/dns/host_resolver.h"
#include "net/http/http_network_layer.h"
#include "net/http/http_response_headers.h"
#include "net/proxy/proxy_service.h"
#include "net/ssl/ssl_config_service_defaults.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_status.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/system/statistics_provider.h"
#endif

using content::BrowserThread;

namespace em = enterprise_management;

namespace policy {

namespace {

const char kValueAgent[] = "%s %s(%s)";
const char kValuePlatform[] = "%s|%s|%s";

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
const int kDeviceIdConflict = 409;
const int kDeviceNotFound = 410;
const int kPendingApproval = 412;
const int kInternalServerError = 500;
const int kServiceUnavailable = 503;
const int kPolicyNotFound = 902;  // This error is not sent as HTTP status code.

#if defined(OS_CHROMEOS)
// Machine info keys.
const char kMachineInfoHWClass[] = "hardware_class";
const char kMachineInfoBoard[] = "CHROMEOS_RELEASE_BOARD";
#endif

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

const char* UserAffiliationToString(UserAffiliation affiliation) {
  switch (affiliation) {
    case USER_AFFILIATION_MANAGED:
      return dm_protocol::kValueUserAffiliationManaged;
    case USER_AFFILIATION_NONE:
      return dm_protocol::kValueUserAffiliationNone;
  }
  NOTREACHED() << "Invalid user affiliation " << affiliation;
  return dm_protocol::kValueUserAffiliationNone;
}

const char* JobTypeToRequestType(DeviceManagementRequestJob::JobType type) {
  switch (type) {
    case DeviceManagementRequestJob::TYPE_AUTO_ENROLLMENT:
      return dm_protocol::kValueRequestAutoEnrollment;
    case DeviceManagementRequestJob::TYPE_REGISTRATION:
      return dm_protocol::kValueRequestRegister;
    case DeviceManagementRequestJob::TYPE_POLICY_FETCH:
      return dm_protocol::kValueRequestPolicy;
    case DeviceManagementRequestJob::TYPE_UNREGISTRATION:
      return dm_protocol::kValueRequestUnregister;
  }
  NOTREACHED() << "Invalid job type " << type;
  return "";
}

const std::string& GetAgentString() {
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

const std::string& GetPlatformString() {
  CR_DEFINE_STATIC_LOCAL(std::string, platform, ());
  if (!platform.empty())
    return platform;

  std::string os_name(base::SysInfo::OperatingSystemName());
  std::string os_hardware(base::SysInfo::OperatingSystemArchitecture());

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

// Custom request context implementation that allows to override the user agent,
// amongst others. Wraps a baseline request context from which we reuse the
// networking components.
class DeviceManagementRequestContext : public net::URLRequestContext {
 public:
  explicit DeviceManagementRequestContext(net::URLRequestContext* base_context);
  virtual ~DeviceManagementRequestContext();

 private:
  BasicHttpUserAgentSettings basic_http_user_agent_settings_;
};

DeviceManagementRequestContext::DeviceManagementRequestContext(
    net::URLRequestContext* base_context)
    // Use sane Accept-Language value for our purposes.
    : basic_http_user_agent_settings_("*") {
  // Share resolver, proxy service and ssl bits with the baseline context. This
  // is important so we don't make redundant requests (e.g. when resolving proxy
  // auto configuration).
  set_net_log(base_context->net_log());
  set_host_resolver(base_context->host_resolver());
  set_proxy_service(base_context->proxy_service());
  set_ssl_config_service(base_context->ssl_config_service());

  // Share the http session.
  set_http_transaction_factory(
      new net::HttpNetworkLayer(
          base_context->http_transaction_factory()->GetSession()));

  // No cookies, please.
  set_cookie_store(new net::CookieMonster(NULL, NULL));

  set_http_user_agent_settings(&basic_http_user_agent_settings_);
}

DeviceManagementRequestContext::~DeviceManagementRequestContext() {
  delete http_transaction_factory();
}

// Request context holder.
class DeviceManagementRequestContextGetter
    : public net::URLRequestContextGetter {
 public:
  explicit DeviceManagementRequestContextGetter(
      net::URLRequestContextGetter* base_context_getter)
      : base_context_getter_(base_context_getter) {}

  // Overridden from net::URLRequestContextGetter:
  virtual net::URLRequestContext* GetURLRequestContext() OVERRIDE;
  virtual scoped_refptr<base::SingleThreadTaskRunner>
      GetNetworkTaskRunner() const OVERRIDE;

 protected:
  virtual ~DeviceManagementRequestContextGetter() {}

 private:
  scoped_ptr<net::URLRequestContext> context_;
  scoped_refptr<net::URLRequestContextGetter> base_context_getter_;
};


net::URLRequestContext*
DeviceManagementRequestContextGetter::GetURLRequestContext() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!context_.get()) {
    context_.reset(new DeviceManagementRequestContext(
        base_context_getter_->GetURLRequestContext()));
  }

  return context_.get();
}

scoped_refptr<base::SingleThreadTaskRunner>
DeviceManagementRequestContextGetter::GetNetworkTaskRunner() const {
  return BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO);
}

}  // namespace

// Request job implementation used with DeviceManagementService.
class DeviceManagementRequestJobImpl : public DeviceManagementRequestJob {
 public:
  DeviceManagementRequestJobImpl(JobType type,
                                 DeviceManagementService* service);
  virtual ~DeviceManagementRequestJobImpl();

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
  virtual void Run() OVERRIDE;

 private:
  // Invokes the callback with the given error code.
  void ReportError(DeviceManagementStatus code);

  // Pointer to the service this job is associated with.
  DeviceManagementService* service_;

  // Whether the BYPASS_PROXY flag should be set by ConfigureRequest().
  bool bypass_proxy_;

  // Number of times that this job has been retried due to ERR_NETWORK_CHANGED.
  int retries_count_;

  DISALLOW_COPY_AND_ASSIGN(DeviceManagementRequestJobImpl);
};

DeviceManagementRequestJobImpl::DeviceManagementRequestJobImpl(
    JobType type,
    DeviceManagementService* service)
    : DeviceManagementRequestJob(type),
      service_(service),
      bypass_proxy_(false),
      retries_count_(0) {}

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
    ReportError(DM_STATUS_REQUEST_FAILED);
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
      callback_.Run(DM_STATUS_SUCCESS, response);
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
  callback_.Run(code, dummy_response);
}

DeviceManagementRequestJob::~DeviceManagementRequestJob() {}

void DeviceManagementRequestJob::SetGaiaToken(const std::string& gaia_token) {
  gaia_token_ = gaia_token;
}

void DeviceManagementRequestJob::SetOAuthToken(const std::string& oauth_token) {
  AddParameter(dm_protocol::kParamOAuthToken, oauth_token);
}

void DeviceManagementRequestJob::SetUserAffiliation(
    UserAffiliation user_affiliation) {
  AddParameter(dm_protocol::kParamUserAffiliation,
               UserAffiliationToString(user_affiliation));
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

DeviceManagementRequestJob::DeviceManagementRequestJob(JobType type) {
  AddParameter(dm_protocol::kParamRequest, JobTypeToRequestType(type));
  AddParameter(dm_protocol::kParamDeviceType, dm_protocol::kValueDeviceType);
  AddParameter(dm_protocol::kParamAppType, dm_protocol::kValueAppType);
  AddParameter(dm_protocol::kParamAgent, GetAgentString());
  AddParameter(dm_protocol::kParamPlatform, GetPlatformString());
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
    DeviceManagementRequestJob::JobType type) {
  return new DeviceManagementRequestJobImpl(type, this);
}

void DeviceManagementService::ScheduleInitialization(int64 delay_milliseconds) {
  if (initialized_)
    return;
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&DeviceManagementService::Initialize,
                 weak_ptr_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(delay_milliseconds));
}

void DeviceManagementService::Initialize() {
  if (initialized_)
    return;
  DCHECK(!request_context_getter_);
  request_context_getter_ = new DeviceManagementRequestContextGetter(
      g_browser_process->system_request_context());
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
    const std::string& server_url)
    : server_url_(server_url),
      initialized_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
}

void DeviceManagementService::StartJob(DeviceManagementRequestJobImpl* job) {
  net::URLFetcher* fetcher = net::URLFetcher::Create(
      kURLFetcherID, job->GetURL(server_url_), net::URLFetcher::POST, this);
  fetcher->SetRequestContext(request_context_getter_.get());
  job->ConfigureRequest(fetcher);
  pending_jobs_[fetcher] = job;
  fetcher->Start();
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
