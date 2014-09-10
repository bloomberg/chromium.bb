// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_request_handler.h"

#include <string>

#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_provider_host.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_url_request_job.h"
#include "content/browser/service_worker/service_worker_utils.h"
#include "content/common/resource_request_body.h"
#include "content/common/service_worker/service_worker_types.h"
#include "net/base/net_util.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_interceptor.h"
#include "storage/browser/blob/blob_storage_context.h"

namespace content {

namespace {

int kUserDataKey;  // Key value is not important.

class ServiceWorkerRequestInterceptor
    : public net::URLRequestInterceptor {
 public:
  ServiceWorkerRequestInterceptor() {}
  virtual ~ServiceWorkerRequestInterceptor() {}
  virtual net::URLRequestJob* MaybeInterceptRequest(
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

// This is work around to avoid hijacking CORS preflight.
// TODO(horo): Remove this check when we implement "HTTP fetch" correctly.
// http://fetch.spec.whatwg.org/#concept-http-fetch
bool IsMethodSupportedForServiceWroker(const std::string& method) {
  return method != "OPTIONS";
}

}  // namespace

void ServiceWorkerRequestHandler::InitializeHandler(
    net::URLRequest* request,
    ServiceWorkerContextWrapper* context_wrapper,
    storage::BlobStorageContext* blob_storage_context,
    int process_id,
    int provider_id,
    bool skip_service_worker,
    ResourceType resource_type,
    scoped_refptr<ResourceRequestBody> body) {
  if (!request->url().SchemeIsHTTPOrHTTPS() ||
      !IsMethodSupportedForServiceWroker(request->method())) {
    return;
  }

  if (!context_wrapper || !context_wrapper->context() ||
      provider_id == kInvalidServiceWorkerProviderId) {
    return;
  }

  ServiceWorkerProviderHost* provider_host =
      context_wrapper->context()->GetProviderHost(process_id, provider_id);
  if (!provider_host || !provider_host->IsContextAlive())
    return;

  if (skip_service_worker) {
    if (ServiceWorkerUtils::IsMainResourceType(resource_type))
      provider_host->SetDocumentUrl(net::SimplifyUrlForRequest(request->url()));
    return;
  }

  scoped_ptr<ServiceWorkerRequestHandler> handler(
      provider_host->CreateRequestHandler(
          resource_type, blob_storage_context->AsWeakPtr(), body));
  if (!handler)
    return;

  request->SetUserData(&kUserDataKey, handler.release());
}

ServiceWorkerRequestHandler* ServiceWorkerRequestHandler::GetHandler(
    net::URLRequest* request) {
  return reinterpret_cast<ServiceWorkerRequestHandler*>(
      request->GetUserData(&kUserDataKey));
}

scoped_ptr<net::URLRequestInterceptor>
ServiceWorkerRequestHandler::CreateInterceptor() {
  return scoped_ptr<net::URLRequestInterceptor>(
      new ServiceWorkerRequestInterceptor);
}

ServiceWorkerRequestHandler::~ServiceWorkerRequestHandler() {
}

ServiceWorkerRequestHandler::ServiceWorkerRequestHandler(
    base::WeakPtr<ServiceWorkerContextCore> context,
    base::WeakPtr<ServiceWorkerProviderHost> provider_host,
    base::WeakPtr<storage::BlobStorageContext> blob_storage_context,
    ResourceType resource_type)
    : context_(context),
      provider_host_(provider_host),
      blob_storage_context_(blob_storage_context),
      resource_type_(resource_type) {
}

}  // namespace content
