// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/device_management_backend_impl.h"

#include <utility>
#include <vector>

#include "base/stl_util-inl.h"
#include "base/stringprintf.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/common/net/url_request_context_getter.h"
#include "net/base/cookie_monster.h"
#include "net/base/escape.h"
#include "net/base/host_resolver.h"
#include "net/base/ssl_config_service_defaults.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_network_layer.h"
#include "net/proxy/proxy_service.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_status.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/io_thread.h"
#include "chrome/browser/net/chrome_net_log.h"
#include "chrome/common/chrome_version_info.h"

namespace policy {

namespace {

// Name constants for URL query parameters.
const char kServiceParamRequest[] = "request";
const char kServiceParamDeviceType[] = "devicetype";
const char kServiceParamDeviceID[] = "deviceid";
const char kServiceParamAgent[] = "agent";

// String constants for the device type and agent we report to the service.
const char kServiceValueDeviceType[] = "Chrome";
const char kServiceValueAgent[] =
    "%s enterprise management client version %s (%s)";

const char kServiceTokenAuthHeader[] = "Authorization: GoogleLogin auth=";
const char kDMTokenAuthHeader[] = "Authorization: GoogleDMToken token=";

}  // namespace

// Custom request context implementation that allows to override the user agent,
// amongst others. Using the default request context is not an option since this
// service may be constructed before the default request context is created
// (i.e. before the profile has been loaded).
class DeviceManagementBackendRequestContext : public URLRequestContext {
 public:
  explicit DeviceManagementBackendRequestContext(IOThread::Globals* io_globals);
  virtual ~DeviceManagementBackendRequestContext();

 private:
  virtual const std::string& GetUserAgent(const GURL& url) const;

  std::string user_agent_;
};

DeviceManagementBackendRequestContext::DeviceManagementBackendRequestContext(
    IOThread::Globals* io_globals) {
  net_log_ = io_globals->net_log.get();
  host_resolver_ = io_globals->host_resolver.get();
  proxy_service_ = net::ProxyService::CreateDirect();
  ssl_config_service_ = net::SSLConfigService::CreateSystemSSLConfigService();
  http_auth_handler_factory_ =
      net::HttpAuthHandlerFactory::CreateDefault(host_resolver_);
  http_transaction_factory_ =
      net::HttpNetworkLayer::CreateFactory(host_resolver_,
                                           io_globals->dnsrr_resolver.get(),
                                           NULL /* ssl_host_info_factory */,
                                           proxy_service_,
                                           ssl_config_service_,
                                           http_auth_handler_factory_,
                                           NULL /* network_delegate */,
                                           net_log_);
  cookie_store_ = new net::CookieMonster(NULL, NULL);
  user_agent_ = DeviceManagementBackendImpl::GetAgentString();
  accept_language_ = "*";
  accept_charset_ = "*";
}

DeviceManagementBackendRequestContext
    ::~DeviceManagementBackendRequestContext() {
  delete http_transaction_factory_;
  delete http_auth_handler_factory_;
}

const std::string&
DeviceManagementBackendRequestContext::GetUserAgent(const GURL& url) const {
  return user_agent_;
}

// Request context holder.
class DeviceManagementBackendRequestContextGetter
    : public URLRequestContextGetter {
 public:
  DeviceManagementBackendRequestContextGetter()
      : io_thread_(g_browser_process->io_thread()) {}

  // URLRequestContextGetter overrides.
  virtual URLRequestContext* GetURLRequestContext();
  virtual scoped_refptr<base::MessageLoopProxy> GetIOMessageLoopProxy() const;

 private:
  scoped_refptr<URLRequestContext> context_;
  IOThread* io_thread_;
};


URLRequestContext*
DeviceManagementBackendRequestContextGetter::GetURLRequestContext() {
  if (!context_)
    context_ = new DeviceManagementBackendRequestContext(io_thread_->globals());

  return context_.get();
}

scoped_refptr<base::MessageLoopProxy>
DeviceManagementBackendRequestContextGetter::GetIOMessageLoopProxy() const {
  return BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO);
}

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

// Wraps common response parsing and handling functionality.
class ResponseHandler {
 public:
  ResponseHandler() {}
  virtual ~ResponseHandler() {}

  // Handles the URL request response.
  void HandleResponse(const URLRequestStatus& status,
                      int response_code,
                      const ResponseCookies& cookies,
                      const std::string& data);

  // Forwards the given error to the delegate.
  virtual void OnError(DeviceManagementBackend::ErrorCode error) = 0;

 private:
  // Implemented by subclasses to handle the decoded response.
  virtual void ProcessResponse(
      const em::DeviceManagementResponse& response) = 0;
};

void ResponseHandler::HandleResponse(const URLRequestStatus& status,
                                     int response_code,
                                     const ResponseCookies& cookies,
                                     const std::string& data) {
  if (status.status() != URLRequestStatus::SUCCESS) {
    OnError(DeviceManagementBackend::kErrorRequestFailed);
    return;
  }

  if (response_code != 200) {
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
      break;
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
    default:
      // This should be caught by the protobuf decoder.
      NOTREACHED();
      OnError(DeviceManagementBackend::kErrorResponseDecoding);
      return;
  }

  ProcessResponse(response);
}

// Handles device registration responses.
class RegisterResponseHandler : public ResponseHandler {
 public:
  RegisterResponseHandler(
      DeviceManagementBackend::DeviceRegisterResponseDelegate* delegate)
      : delegate_(delegate) {}

 private:
  // ResponseHandler overrides.
  virtual void OnError(DeviceManagementBackend::ErrorCode error) {
    delegate_->OnError(error);
  }
  virtual void ProcessResponse(const em::DeviceManagementResponse& response) {
    delegate_->HandleRegisterResponse(response.register_response());
  }

  DeviceManagementBackend::DeviceRegisterResponseDelegate* delegate_;
};

// Handles device unregister responses.
class UnregisterResponseHandler : public ResponseHandler {
 public:
  UnregisterResponseHandler(
      DeviceManagementBackend::DeviceUnregisterResponseDelegate* delegate)
      : delegate_(delegate) {}

 private:
  // ResponseHandler overrides.
  virtual void OnError(DeviceManagementBackend::ErrorCode error) {
    delegate_->OnError(error);
  }
  virtual void ProcessResponse(const em::DeviceManagementResponse& response) {
    delegate_->HandleUnregisterResponse(response.unregister_response());
  }

  DeviceManagementBackend::DeviceUnregisterResponseDelegate* delegate_;
};

// Handles device policy responses.
class PolicyResponseHandler : public ResponseHandler {
 public:
  PolicyResponseHandler(
      DeviceManagementBackend::DevicePolicyResponseDelegate* delegate)
      : delegate_(delegate) {}

 private:
  // ResponseHandler overrides.
  virtual void OnError(DeviceManagementBackend::ErrorCode error) {
    delegate_->OnError(error);
  }
  virtual void ProcessResponse(const em::DeviceManagementResponse& response) {
    delegate_->HandlePolicyResponse(response.policy_response());
  }

  DeviceManagementBackend::DevicePolicyResponseDelegate* delegate_;
};

DeviceManagementBackendImpl::DeviceManagementBackendImpl(
    const std::string& server_url)
    : server_url_(server_url),
      request_context_getter_(
          new DeviceManagementBackendRequestContextGetter()) {
}

DeviceManagementBackendImpl::~DeviceManagementBackendImpl() {
  // Cancel all pending requests.
  STLDeleteContainerPairPointers(response_handlers_.begin(),
                                 response_handlers_.end());
}

void DeviceManagementBackendImpl::ProcessRegisterRequest(
    const std::string& auth_token,
    const std::string& device_id,
    const em::DeviceRegisterRequest& request,
    DeviceRegisterResponseDelegate* delegate) {
  em::DeviceManagementRequest request_wrapper;
  request_wrapper.mutable_register_request()->CopyFrom(request);

  URLQueryParameters params;
  PutCommonQueryParameters(&params);
  params.Put(kServiceParamRequest, "register");
  params.Put(kServiceParamDeviceID, device_id);

  CreateFetcher(request_wrapper,
                new RegisterResponseHandler(delegate),
                params.Encode(),
                kServiceTokenAuthHeader + auth_token);
}

void DeviceManagementBackendImpl::ProcessUnregisterRequest(
    const std::string& device_management_token,
    const em::DeviceUnregisterRequest& request,
    DeviceUnregisterResponseDelegate* delegate) {
  em::DeviceManagementRequest request_wrapper;
  request_wrapper.mutable_unregister_request()->CopyFrom(request);

  URLQueryParameters params;
  PutCommonQueryParameters(&params);
  params.Put(kServiceParamRequest, "unregister");

  CreateFetcher(request_wrapper,
                new UnregisterResponseHandler(delegate),
                params.Encode(),
                kDMTokenAuthHeader + device_management_token);
}

void DeviceManagementBackendImpl::ProcessPolicyRequest(
    const std::string& device_management_token,
    const em::DevicePolicyRequest& request,
    DevicePolicyResponseDelegate* delegate) {
  em::DeviceManagementRequest request_wrapper;
  request_wrapper.mutable_policy_request()->CopyFrom(request);

  URLQueryParameters params;
  PutCommonQueryParameters(&params);
  params.Put(kServiceParamRequest, "policy");

  CreateFetcher(request_wrapper,
                new PolicyResponseHandler(delegate),
                params.Encode(),
                kDMTokenAuthHeader + device_management_token);
}

// static
std::string DeviceManagementBackendImpl::GetAgentString() {
  chrome::VersionInfo version_info;
  return base::StringPrintf(kServiceValueAgent,
                            version_info.Name().c_str(),
                            version_info.Version().c_str(),
                            version_info.LastChange().c_str());
}

void DeviceManagementBackendImpl::OnURLFetchComplete(
    const URLFetcher* source,
    const GURL& url,
    const URLRequestStatus& status,
    int response_code,
    const ResponseCookies& cookies,
    const std::string& data) {
  ResponseHandlerMap::iterator entry(response_handlers_.find(source));
  if (entry != response_handlers_.end()) {
    ResponseHandler* handler = entry->second;
    handler->HandleResponse(status, response_code, cookies, data);
    response_handlers_.erase(entry);
    delete handler;
  } else {
    NOTREACHED() << "Callback from foreign URL fetcher";
  }
  delete source;
}

void DeviceManagementBackendImpl::CreateFetcher(
    const em::DeviceManagementRequest& request,
    ResponseHandler* handler,
    const std::string& query_params,
    const std::string& extra_headers) {
  scoped_ptr<ResponseHandler> handler_ptr(handler);

  // Construct the payload.
  std::string payload;
  if (!request.SerializeToString(&payload)) {
    handler->OnError(DeviceManagementBackend::kErrorRequestInvalid);
    return;
  }

  // Instantiate the fetcher.
  GURL url(server_url_ + '?' + query_params);
  URLFetcher* fetcher = URLFetcher::Create(0, url, URLFetcher::POST, this);
  fetcher->set_request_context(request_context_getter_.get());
  fetcher->set_upload_data("application/octet-stream", payload);
  fetcher->set_extra_request_headers(extra_headers);
  response_handlers_[fetcher] = handler_ptr.release();

  // Start the request. The fetcher will call OnURLFetchComplete when done.
  fetcher->Start();
}

void DeviceManagementBackendImpl::PutCommonQueryParameters(
    URLQueryParameters* params) {
  params->Put(kServiceParamDeviceType, kServiceValueDeviceType);
  params->Put(kServiceParamAgent, GetAgentString());
}

}  // namespace policy
