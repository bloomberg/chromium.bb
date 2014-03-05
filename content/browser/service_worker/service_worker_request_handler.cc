// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_request_handler.h"

#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_provider_host.h"
#include "content/browser/service_worker/service_worker_url_request_job.h"
#include "content/browser/service_worker/service_worker_utils.h"
#include "content/common/service_worker/service_worker_types.h"
#include "net/url_request/url_request.h"

namespace content {

namespace {
int kUserDataKey;  // Key value is not important.
}

void ServiceWorkerRequestHandler::InitializeHandler(
    net::URLRequest* request,
    base::WeakPtr<ServiceWorkerContextCore> context,
    int process_id,
    int provider_id,
    ResourceType::Type resource_type) {
  if (!context || provider_id == kInvalidServiceWorkerProviderId)
    return;

  ServiceWorkerProviderHost* provider_host = context->GetProviderHost(
      process_id, provider_id);
  if (!provider_host)
    return;

  if (!provider_host->ShouldHandleRequest(resource_type))
    return;

  scoped_ptr<ServiceWorkerRequestHandler> handler(
      new ServiceWorkerRequestHandler(context,
                                      provider_host->AsWeakPtr(),
                                      resource_type));
  request->SetUserData(&kUserDataKey, handler.release());
}

ServiceWorkerRequestHandler* ServiceWorkerRequestHandler::GetHandler(
    net::URLRequest* request) {
  return reinterpret_cast<ServiceWorkerRequestHandler*>(
      request->GetUserData(&kUserDataKey));
}

ServiceWorkerRequestHandler::~ServiceWorkerRequestHandler() {
}

net::URLRequestJob* ServiceWorkerRequestHandler::MaybeCreateJob(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate) {
  if (ShouldFallbackToNetwork()) {
    job_ = NULL;
    return NULL;
  }

  DCHECK(!job_.get());
  job_ = new ServiceWorkerURLRequestJob(request, network_delegate);
  return job_.get();
}

ServiceWorkerRequestHandler::ServiceWorkerRequestHandler(
    base::WeakPtr<ServiceWorkerContextCore> context,
    base::WeakPtr<ServiceWorkerProviderHost> provider_host,
    ResourceType::Type resource_type)
    : context_(context),
      provider_host_(provider_host),
      resource_type_(resource_type) {
}

bool ServiceWorkerRequestHandler::ShouldFallbackToNetwork() const {
  NOTIMPLEMENTED();
  return true;
}

}  // namespace content
