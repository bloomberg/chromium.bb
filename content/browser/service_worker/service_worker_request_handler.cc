// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_request_handler.h"

#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_provider_host.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_url_request_job.h"
#include "content/browser/service_worker/service_worker_utils.h"
#include "content/common/service_worker/service_worker_types.h"
#include "net/url_request/url_request.h"

namespace content {

namespace {

int kUserDataKey;  // Key value is not important.

class ServiceWorkerRequestInterceptor
    : public net::URLRequestJobFactory::ProtocolHandler {
 public:
  ServiceWorkerRequestInterceptor() {}
  virtual ~ServiceWorkerRequestInterceptor() {}
  virtual net::URLRequestJob* MaybeCreateJob(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const OVERRIDE {
    ServiceWorkerRequestHandler* handler =
        ServiceWorkerRequestHandler::GetHandler(request);
    if (!handler)
      return NULL;
    return handler->MaybeCreateJob(request, network_delegate);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerRequestInterceptor);
};

bool IsMethodSupported(const std::string& method) {
  return (method == "GET") || (method == "HEAD");
}

bool IsSchemeAndMethodSupported(const net::URLRequest* request) {
  return request->url().SchemeIsHTTPOrHTTPS() &&
         IsMethodSupported(request->method());
}

}  // namespace

void ServiceWorkerRequestHandler::InitializeHandler(
    net::URLRequest* request,
    ServiceWorkerContextWrapper* context_wrapper,
    int process_id,
    int provider_id,
    ResourceType::Type resource_type) {
  if (!ServiceWorkerUtils::IsFeatureEnabled() ||
      !IsSchemeAndMethodSupported(request)) {
    return;
  }

  if (!context_wrapper || !context_wrapper->context() ||
      provider_id == kInvalidServiceWorkerProviderId) {
    return;
  }

  ServiceWorkerProviderHost* provider_host =
      context_wrapper->context()->GetProviderHost(process_id, provider_id);
  if (!provider_host)
    return;

  scoped_ptr<ServiceWorkerRequestHandler> handler(
      provider_host->CreateRequestHandler(resource_type));
  if (!handler)
    return;

  request->SetUserData(&kUserDataKey, handler.release());
}

ServiceWorkerRequestHandler* ServiceWorkerRequestHandler::GetHandler(
    net::URLRequest* request) {
  return reinterpret_cast<ServiceWorkerRequestHandler*>(
      request->GetUserData(&kUserDataKey));
}

scoped_ptr<net::URLRequestJobFactory::ProtocolHandler>
ServiceWorkerRequestHandler::CreateInterceptor() {
  return make_scoped_ptr<net::URLRequestJobFactory::ProtocolHandler>(
      new ServiceWorkerRequestInterceptor);
}

ServiceWorkerRequestHandler::~ServiceWorkerRequestHandler() {
}

ServiceWorkerRequestHandler::ServiceWorkerRequestHandler(
    base::WeakPtr<ServiceWorkerContextCore> context,
    base::WeakPtr<ServiceWorkerProviderHost> provider_host,
    ResourceType::Type resource_type)
    : context_(context),
      provider_host_(provider_host),
      resource_type_(resource_type) {
}

}  // namespace content
