// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_controllee_request_handler.h"

#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_metrics.h"
#include "content/browser/service_worker/service_worker_provider_host.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_url_request_job.h"
#include "content/browser/service_worker/service_worker_utils.h"
#include "content/common/service_worker/service_worker_types.h"
#include "net/base/load_flags.h"
#include "net/base/net_util.h"
#include "net/url_request/url_request.h"

namespace content {

ServiceWorkerControlleeRequestHandler::ServiceWorkerControlleeRequestHandler(
    base::WeakPtr<ServiceWorkerContextCore> context,
    base::WeakPtr<ServiceWorkerProviderHost> provider_host,
    base::WeakPtr<webkit_blob::BlobStorageContext> blob_storage_context,
    ResourceType::Type resource_type)
    : ServiceWorkerRequestHandler(context,
                                  provider_host,
                                  blob_storage_context,
                                  resource_type),
      is_main_resource_load_(
          ServiceWorkerUtils::IsMainResourceType(resource_type)),
      weak_factory_(this) {
}

ServiceWorkerControlleeRequestHandler::
    ~ServiceWorkerControlleeRequestHandler() {
  // Navigation triggers an update to occur shortly after the page and
  // its initial subresources load.
  if (provider_host_ && provider_host_->active_version()) {
    if (is_main_resource_load_)
      provider_host_->active_version()->ScheduleUpdate();
    else
      provider_host_->active_version()->DeferScheduledUpdate();
  }
}

net::URLRequestJob* ServiceWorkerControlleeRequestHandler::MaybeCreateJob(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate) {
  if (!context_ || !provider_host_) {
    // We can't do anything other than to fall back to network.
    job_ = NULL;
    return NULL;
  }

  if (request->load_flags() & net::LOAD_BYPASS_CACHE) {
    if (is_main_resource_load_) {
      provider_host_->SetDocumentUrl(
          net::SimplifyUrlForRequest(request->url()));
    }
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

  job_ = new ServiceWorkerURLRequestJob(
      request, network_delegate, provider_host_, blob_storage_context_);
  if (is_main_resource_load_)
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

void ServiceWorkerControlleeRequestHandler::GetExtraResponseInfo(
    bool* was_fetched_via_service_worker,
    GURL* original_url_via_service_worker) const {
  if (!job_) {
    *was_fetched_via_service_worker = false;
    *original_url_via_service_worker = GURL();
    return;
  }
  job_->GetExtraResponseInfo(was_fetched_via_service_worker,
                             original_url_via_service_worker);
}

void ServiceWorkerControlleeRequestHandler::PrepareForMainResource(
    const GURL& url) {
  DCHECK(job_.get());
  DCHECK(context_);
  // The corresponding provider_host may already have associate version in
  // redirect case, unassociate it now.
  provider_host_->UnsetVersion(NULL);

  GURL stripped_url = net::SimplifyUrlForRequest(url);
  provider_host_->SetDocumentUrl(stripped_url);
  context_->storage()->FindRegistrationForDocument(
      stripped_url,
      base::Bind(&self::DidLookupRegistrationForMainResource,
                 weak_factory_.GetWeakPtr()));
}

void
ServiceWorkerControlleeRequestHandler::DidLookupRegistrationForMainResource(
    ServiceWorkerStatusCode status,
    const scoped_refptr<ServiceWorkerRegistration>& registration) {
  DCHECK(job_.get());
  if (status != SERVICE_WORKER_OK || !registration->active_version()) {
    // No registration, or no active version for the registration is available.
    job_->FallbackToNetwork();
    // TODO(michaeln): If there's a waiting version, activate it instead of
    // using the network.
    return;
  }

  ServiceWorkerMetrics::CountControlledPageLoad();

  // TODO(michaeln): if 'activating' wait until it's activated before
  // forwarding the request to the serviceworker.
  DCHECK(registration);
  provider_host_->SetControllerVersion(registration->active_version());
  provider_host_->SetActiveVersion(registration->active_version());
  provider_host_->SetWaitingVersion(registration->waiting_version());
  provider_host_->SetInstallingVersion(registration->installing_version());

  job_->ForwardToServiceWorker();
}

void ServiceWorkerControlleeRequestHandler::PrepareForSubResource() {
  DCHECK(job_.get());
  DCHECK(context_);
  DCHECK(provider_host_->active_version());
  job_->ForwardToServiceWorker();
}

}  // namespace content
