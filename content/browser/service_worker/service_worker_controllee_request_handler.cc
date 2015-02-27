// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_controllee_request_handler.h"

#include "base/trace_event/trace_event.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_metrics.h"
#include "content/browser/service_worker/service_worker_provider_host.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_url_request_job.h"
#include "content/browser/service_worker/service_worker_utils.h"
#include "content/common/resource_request_body.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "net/base/load_flags.h"
#include "net/base/net_util.h"
#include "net/url_request/url_request.h"

namespace content {

ServiceWorkerControlleeRequestHandler::ServiceWorkerControlleeRequestHandler(
    base::WeakPtr<ServiceWorkerContextCore> context,
    base::WeakPtr<ServiceWorkerProviderHost> provider_host,
    base::WeakPtr<storage::BlobStorageContext> blob_storage_context,
    FetchRequestMode request_mode,
    FetchCredentialsMode credentials_mode,
    ResourceType resource_type,
    RequestContextType request_context_type,
    RequestContextFrameType frame_type,
    scoped_refptr<ResourceRequestBody> body)
    : ServiceWorkerRequestHandler(context,
                                  provider_host,
                                  blob_storage_context,
                                  resource_type),
      is_main_resource_load_(
          ServiceWorkerUtils::IsMainResourceType(resource_type)),
      request_mode_(request_mode),
      credentials_mode_(credentials_mode),
      request_context_type_(request_context_type),
      frame_type_(frame_type),
      body_(body),
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

  if (is_main_resource_load_ && provider_host_)
    provider_host_->SetAllowAssociation(true);
}

net::URLRequestJob* ServiceWorkerControlleeRequestHandler::MaybeCreateJob(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate,
    ResourceContext* resource_context) {
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

  job_ = new ServiceWorkerURLRequestJob(request,
                                        network_delegate,
                                        provider_host_,
                                        blob_storage_context_,
                                        resource_context,
                                        request_mode_,
                                        credentials_mode_,
                                        request_context_type_,
                                        frame_type_,
                                        body_);
  resource_context_ = resource_context;

  if (is_main_resource_load_)
    PrepareForMainResource(request);
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
    bool* was_fallback_required_by_service_worker,
    GURL* original_url_via_service_worker,
    blink::WebServiceWorkerResponseType* response_type_via_service_worker,
    base::TimeTicks* fetch_start_time,
    base::TimeTicks* fetch_ready_time,
    base::TimeTicks* fetch_end_time) const {
  if (!job_.get()) {
    *was_fetched_via_service_worker = false;
    *was_fallback_required_by_service_worker = false;
    *original_url_via_service_worker = GURL();
    return;
  }
  job_->GetExtraResponseInfo(was_fetched_via_service_worker,
                             was_fallback_required_by_service_worker,
                             original_url_via_service_worker,
                             response_type_via_service_worker,
                             fetch_start_time,
                             fetch_ready_time,
                             fetch_end_time);
}

void ServiceWorkerControlleeRequestHandler::PrepareForMainResource(
    const net::URLRequest* request) {
  DCHECK(job_.get());
  DCHECK(context_);
  DCHECK(provider_host_);
  TRACE_EVENT_ASYNC_BEGIN1(
      "ServiceWorker",
      "ServiceWorkerControlleeRequestHandler::PrepareForMainResource",
      job_.get(),
      "URL", request->url().spec());
  // The corresponding provider_host may already have associated a registration
  // in redirect case, unassociate it now.
  provider_host_->DisassociateRegistration();

  // Also prevent a registrater job for establishing an association to a new
  // registration while we're finding an existing registration.
  provider_host_->SetAllowAssociation(false);

  stripped_url_ = net::SimplifyUrlForRequest(request->url());
  provider_host_->SetDocumentUrl(stripped_url_);
  provider_host_->SetTopmostFrameUrl(request->first_party_for_cookies());
  context_->storage()->FindRegistrationForDocument(
      stripped_url_, base::Bind(&self::DidLookupRegistrationForMainResource,
                                weak_factory_.GetWeakPtr()));
}

void
ServiceWorkerControlleeRequestHandler::DidLookupRegistrationForMainResource(
    ServiceWorkerStatusCode status,
    const scoped_refptr<ServiceWorkerRegistration>& registration) {
  DCHECK(job_.get());
  if (provider_host_)
    provider_host_->SetAllowAssociation(true);
  if (status != SERVICE_WORKER_OK || !provider_host_) {
    job_->FallbackToNetwork();
    TRACE_EVENT_ASYNC_END1(
        "ServiceWorker",
        "ServiceWorkerControlleeRequestHandler::PrepareForMainResource",
        job_.get(),
        "Status", status);
    return;
  }
  DCHECK(registration.get());

  if (!GetContentClient()->browser()->AllowServiceWorker(
          registration->pattern(),
          provider_host_->topmost_frame_url(),
          resource_context_)) {
    job_->FallbackToNetwork();
    TRACE_EVENT_ASYNC_END2(
        "ServiceWorker",
        "ServiceWorkerControlleeRequestHandler::PrepareForMainResource",
        job_.get(),
        "Status", status,
        "Info", "ServiceWorker is blocked");
    return;
  }

  // Initiate activation of a waiting version.
  // Usually a register job initiates activation but that
  // doesn't happen if the browser exits prior to activation
  // having occurred. This check handles that case.
  if (registration->waiting_version())
    registration->ActivateWaitingVersionWhenReady();

  scoped_refptr<ServiceWorkerVersion> active_version =
      registration->active_version();

  // Wait until it's activated before firing fetch events.
  if (active_version.get() &&
      active_version->status() == ServiceWorkerVersion::ACTIVATING) {
    provider_host_->SetAllowAssociation(false);
    registration->active_version()->RegisterStatusChangeCallback(
        base::Bind(&self::OnVersionStatusChanged,
                   weak_factory_.GetWeakPtr(),
                   registration,
                   active_version));
    TRACE_EVENT_ASYNC_END2(
        "ServiceWorker",
        "ServiceWorkerControlleeRequestHandler::PrepareForMainResource",
        job_.get(),
        "Status", status,
        "Info", "Wait until finished SW activation");
    return;
  }

  if (!active_version.get() ||
      active_version->status() != ServiceWorkerVersion::ACTIVATED) {
    job_->FallbackToNetwork();
    TRACE_EVENT_ASYNC_END2(
        "ServiceWorker",
        "ServiceWorkerControlleeRequestHandler::PrepareForMainResource",
        job_.get(),
        "Status", status,
        "Info",
        "ServiceWorkerVersion is not available, so falling back to network");
    return;
  }

  ServiceWorkerMetrics::CountControlledPageLoad(stripped_url_);

  provider_host_->AssociateRegistration(registration.get());
  job_->ForwardToServiceWorker();
  TRACE_EVENT_ASYNC_END2(
      "ServiceWorker",
      "ServiceWorkerControlleeRequestHandler::PrepareForMainResource",
      job_.get(),
      "Status", status,
      "Info",
      "Forwarded to the ServiceWorker");
}

void ServiceWorkerControlleeRequestHandler::OnVersionStatusChanged(
    ServiceWorkerRegistration* registration,
    ServiceWorkerVersion* version) {
  if (provider_host_)
    provider_host_->SetAllowAssociation(true);
  if (version != registration->active_version() ||
      version->status() != ServiceWorkerVersion::ACTIVATED ||
      !provider_host_) {
    job_->FallbackToNetwork();
    return;
  }

  ServiceWorkerMetrics::CountControlledPageLoad(stripped_url_);

  provider_host_->AssociateRegistration(registration);
  job_->ForwardToServiceWorker();
}

void ServiceWorkerControlleeRequestHandler::PrepareForSubResource() {
  DCHECK(job_.get());
  DCHECK(context_);
  DCHECK(provider_host_->active_version());
  job_->ForwardToServiceWorker();
}

}  // namespace content
