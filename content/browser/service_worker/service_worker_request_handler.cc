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

}  // namespace

void ServiceWorkerRequestHandler::InitializeHandler(
    net::URLRequest* request,
    ServiceWorkerContextWrapper* context_wrapper,
    int process_id,
    int provider_id,
    ResourceType::Type resource_type) {
  if (!ServiceWorkerUtils::IsFeatureEnabled())
    return;

  if (!context_wrapper || !context_wrapper->context() ||
      provider_id == kInvalidServiceWorkerProviderId) {
    return;
  }

  ServiceWorkerProviderHost* provider_host =
      context_wrapper->context()->GetProviderHost(process_id, provider_id);
  if (!provider_host)
    return;

  if (!provider_host->ShouldHandleRequest(resource_type))
    return;

  scoped_ptr<ServiceWorkerRequestHandler> handler(
      new ServiceWorkerRequestHandler(context_wrapper->context()->AsWeakPtr(),
                                      provider_host->AsWeakPtr(),
                                      resource_type));
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

net::URLRequestJob* ServiceWorkerRequestHandler::MaybeCreateJob(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate) {
  if (!context_ || !provider_host_) {
    // We can't do anything other than to fall back to network.
    job_ = NULL;
    return NULL;
  }

  // This may get called multiple times for original and redirect requests:
  // A. original request case: job_ is null, no previous location info.
  // B. redirect or restarted request case:
  //  a) job_ is non-null if the previous location was forwarded to SW.
  //  b) job_ is null if the previous location was fallback.
  //  c) job_ is non-null if additional restart was required to fall back.

  // We've come here by restart, we already have original request and it
  // tells we should fallback to network. (Case B-c)
  if (job_.get() && job_->ShouldFallbackToNetwork()) {
    job_ = NULL;
    return NULL;
  }

  // It's for original request (A) or redirect case (B-a or B-b).
  DCHECK(!job_.get() || job_->ShouldForwardToServiceWorker());

  job_ = new ServiceWorkerURLRequestJob(request, network_delegate,
                                        provider_host_);
  if (ServiceWorkerUtils::IsMainResourceType(resource_type_))
    PrepareForMainResource(request->url());
  else
    PrepareForSubResource();

  if (job_->ShouldFallbackToNetwork()) {
    // If we know we can fallback to network at this point (in case
    // the storage lookup returned immediately), just return NULL here to
    // fallback to network.
    job_ = NULL;
    return NULL;
  }

  return job_.get();
}

ServiceWorkerRequestHandler::ServiceWorkerRequestHandler(
    base::WeakPtr<ServiceWorkerContextCore> context,
    base::WeakPtr<ServiceWorkerProviderHost> provider_host,
    ResourceType::Type resource_type)
    : context_(context),
      provider_host_(provider_host),
      resource_type_(resource_type),
      weak_factory_(this) {
}

void ServiceWorkerRequestHandler::PrepareForMainResource(const GURL& url) {
  DCHECK(job_.get());
  DCHECK(context_);
  // The corresponding provider_host may already have associate version in
  // redirect case, unassociate it now.
  provider_host_->SetActiveVersion(NULL);
  provider_host_->SetPendingVersion(NULL);
  provider_host_->set_document_url(url);
  context_->storage()->FindRegistrationForDocument(
      url,
      base::Bind(&self::DidLookupRegistrationForMainResource,
                  weak_factory_.GetWeakPtr()));
}

void ServiceWorkerRequestHandler::DidLookupRegistrationForMainResource(
    ServiceWorkerStatusCode status,
    const scoped_refptr<ServiceWorkerRegistration>& registration) {
  DCHECK(job_.get());
  if (status != SERVICE_WORKER_OK || !registration->active_version()) {
    // No registration, or no active version for the registration is available.
    job_->FallbackToNetwork();
    return;
  }
  DCHECK(registration);
  provider_host_->SetActiveVersion(registration->active_version());
  provider_host_->SetPendingVersion(registration->pending_version());
  job_->ForwardToServiceWorker();
}

void ServiceWorkerRequestHandler::PrepareForSubResource() {
  DCHECK(job_.get());
  DCHECK(context_);
  DCHECK(provider_host_->active_version());
  job_->ForwardToServiceWorker();
}

}  // namespace content
