// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/foreign_fetch_request_handler.h"

#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_response_info.h"
#include "content/browser/service_worker/service_worker_url_request_job.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/origin_trial_policy.h"
#include "content/public/common/resource_request_body.h"
#include "content/public/common/service_worker_modes.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_interceptor.h"
#include "storage/browser/blob/blob_storage_context.h"

namespace content {

namespace {

int kUserDataKey;  // Only address is used.

class ForeignFetchRequestInterceptor : public net::URLRequestInterceptor {
 public:
  explicit ForeignFetchRequestInterceptor(ResourceContext* resource_context)
      : resource_context_(resource_context) {}
  ~ForeignFetchRequestInterceptor() override {}
  net::URLRequestJob* MaybeInterceptRequest(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override {
    ForeignFetchRequestHandler* handler =
        ForeignFetchRequestHandler::GetHandler(request);
    if (!handler)
      return nullptr;
    return handler->MaybeCreateJob(request, network_delegate,
                                   resource_context_);
  }

 private:
  ResourceContext* resource_context_;
  DISALLOW_COPY_AND_ASSIGN(ForeignFetchRequestInterceptor);
};

}  // namespace

bool ForeignFetchRequestHandler::IsForeignFetchEnabled() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableExperimentalWebPlatformFeatures)) {
    return true;
  }
  OriginTrialPolicy* origin_trial_policy =
      GetContentClient()->GetOriginTrialPolicy();
  return origin_trial_policy &&
         !origin_trial_policy->IsFeatureDisabled("ForeignFetch");
}

void ForeignFetchRequestHandler::InitializeHandler(
    net::URLRequest* request,
    ServiceWorkerContextWrapper* context_wrapper,
    storage::BlobStorageContext* blob_storage_context,
    int process_id,
    int provider_id,
    ServiceWorkerMode service_worker_mode,
    FetchRequestMode request_mode,
    FetchCredentialsMode credentials_mode,
    FetchRedirectMode redirect_mode,
    const std::string& integrity,
    ResourceType resource_type,
    RequestContextType request_context_type,
    RequestContextFrameType frame_type,
    scoped_refptr<ResourceRequestBody> body,
    bool initiated_in_secure_context) {
  if (!IsForeignFetchEnabled())
    return;

  if (!context_wrapper || !context_wrapper->context() ||
      provider_id == kInvalidServiceWorkerProviderId) {
    return;
  }

  if (service_worker_mode == ServiceWorkerMode::NONE)
    return;

  if (!initiated_in_secure_context)
    return;

  // ServiceWorkerUtils::IsMainResource doesn't consider all worker types to
  // be main resources. This code shouldn't handle any main resource requests
  // though, so explicitly exclude the extra worker types.
  if (ServiceWorkerUtils::IsMainResourceType(resource_type) ||
      resource_type == RESOURCE_TYPE_WORKER ||
      resource_type == RESOURCE_TYPE_SERVICE_WORKER)
    return;

  if (request->initiator().has_value() &&
      request->initiator()->IsSameOriginWith(url::Origin(request->url()))) {
    return;
  }

  ServiceWorkerProviderHost* provider_host =
      context_wrapper->context()->GetProviderHost(process_id, provider_id);
  if (!provider_host || !provider_host->IsContextAlive())
    return;

  base::Optional<base::TimeDelta> timeout;
  if (provider_host->IsHostToRunningServiceWorker()) {
    timeout = base::make_optional(
        provider_host->running_hosted_version()->remaining_timeout());
  }

  if (!context_wrapper->OriginHasForeignFetchRegistrations(
          request->url().GetOrigin())) {
    return;
  }

  // Any more precise checks to see if the request should be intercepted are
  // asynchronous, so just create our handler in all cases.
  std::unique_ptr<ForeignFetchRequestHandler> handler =
      base::WrapUnique(new ForeignFetchRequestHandler(
          context_wrapper, blob_storage_context->AsWeakPtr(), request_mode,
          credentials_mode, redirect_mode, integrity, resource_type,
          request_context_type, frame_type, body, timeout));
  request->SetUserData(&kUserDataKey, std::move(handler));
}

ForeignFetchRequestHandler* ForeignFetchRequestHandler::GetHandler(
    net::URLRequest* request) {
  return static_cast<ForeignFetchRequestHandler*>(
      request->GetUserData(&kUserDataKey));
}

std::unique_ptr<net::URLRequestInterceptor>
ForeignFetchRequestHandler::CreateInterceptor(
    ResourceContext* resource_context) {
  return std::unique_ptr<net::URLRequestInterceptor>(
      new ForeignFetchRequestInterceptor(resource_context));
}

ForeignFetchRequestHandler::~ForeignFetchRequestHandler() {}

net::URLRequestJob* ForeignFetchRequestHandler::MaybeCreateJob(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate,
    ResourceContext* resource_context) {
  ClearJob();
  ServiceWorkerResponseInfo::ResetDataForRequest(request);

  if (!context_) {
    // We can't do anything other than to fall back to network.
    job_.reset();
    return nullptr;
  }

  // This may get called multiple times for original and redirect requests:
  // A. original request case: use_network_ is false, no previous location info.
  // B. redirect or restarted request case:
  //  a) use_network_ is false if the previous location was forwarded to SW.
  //  b) use_network_ is false if the previous location was fallback.
  //  c) use_network_ is true if additional restart was required to fall back.

  // Fall back to network. (Case B-c)
  if (use_network_) {
    // TODO(mek): Determine if redirects should be able to be intercepted by
    // other foreign fetch service workers.
    return nullptr;
  }

  // It's for original request (A) or redirect case (B-a or B-b).
  DCHECK(!job_.get() || job_->ShouldForwardToServiceWorker());

  ServiceWorkerURLRequestJob* job = new ServiceWorkerURLRequestJob(
      request, network_delegate, std::string(), blob_storage_context_,
      resource_context, request_mode_, credentials_mode_, redirect_mode_,
      integrity_, resource_type_, request_context_type_, frame_type_, body_,
      ServiceWorkerFetchType::FOREIGN_FETCH, timeout_, this);
  job_ = job->GetWeakPtr();
  resource_context_ = resource_context;

  context_->FindReadyRegistrationForDocument(
      request->url(),
      base::Bind(&ForeignFetchRequestHandler::DidFindRegistration,
                 weak_factory_.GetWeakPtr(), job_));

  return job_.get();
}

ForeignFetchRequestHandler::ForeignFetchRequestHandler(
    ServiceWorkerContextWrapper* context,
    base::WeakPtr<storage::BlobStorageContext> blob_storage_context,
    FetchRequestMode request_mode,
    FetchCredentialsMode credentials_mode,
    FetchRedirectMode redirect_mode,
    const std::string& integrity,
    ResourceType resource_type,
    RequestContextType request_context_type,
    RequestContextFrameType frame_type,
    scoped_refptr<ResourceRequestBody> body,
    const base::Optional<base::TimeDelta>& timeout)
    : context_(context),
      blob_storage_context_(blob_storage_context),
      resource_type_(resource_type),
      request_mode_(request_mode),
      credentials_mode_(credentials_mode),
      redirect_mode_(redirect_mode),
      integrity_(integrity),
      request_context_type_(request_context_type),
      frame_type_(frame_type),
      body_(body),
      timeout_(timeout),
      weak_factory_(this) {}

void ForeignFetchRequestHandler::DidFindRegistration(
    const base::WeakPtr<ServiceWorkerURLRequestJob>& job,
    ServiceWorkerStatusCode status,
    scoped_refptr<ServiceWorkerRegistration> registration) {
  if (!job || job.get() != job_.get()) {
    // No more job to handle, or job changed somehow, so just return.
    return;
  }

  if (status != SERVICE_WORKER_OK || !job->request()) {
    job->FallbackToNetwork();
    return;
  }

  ServiceWorkerVersion* active_version = registration->active_version();
  DCHECK(active_version);

  const GURL& request_url = job->request()->url();
  bool scope_matches = false;
  for (const GURL& scope : active_version->foreign_fetch_scopes()) {
    if (ServiceWorkerUtils::ScopeMatches(scope, request_url)) {
      scope_matches = true;
      break;
    }
  }

  const url::Origin& request_origin = job->request()->initiator().value();
  bool origin_matches = active_version->foreign_fetch_origins().empty();
  for (const url::Origin& origin : active_version->foreign_fetch_origins()) {
    if (request_origin.IsSameOriginWith(origin))
      origin_matches = true;
  }

  if (!scope_matches || !origin_matches) {
    job->FallbackToNetwork();
    return;
  }

  if (!IsForeignFetchEnabled() && !CheckOriginTrialToken(active_version)) {
    job->FallbackToNetwork();
    return;
  }

  auto* request_info = ResourceRequestInfo::ForRequest(job->request());
  base::Callback<WebContents*(void)> web_contents_getter;
  if (request_info)
    web_contents_getter = request_info->GetWebContentsGetterForRequest();

  if (!GetContentClient()->browser()->AllowServiceWorker(
          registration->pattern(), job->request()->site_for_cookies(),
          resource_context_, web_contents_getter)) {
    job->FallbackToNetwork();
    return;
  }

  target_worker_ = active_version;
  job->ForwardToServiceWorker();
}

void ForeignFetchRequestHandler::OnPrepareToRestart() {
  use_network_ = true;
  ClearJob();
}

ServiceWorkerVersion* ForeignFetchRequestHandler::GetServiceWorkerVersion(
    ServiceWorkerMetrics::URLRequestJobResult* result) {
  // TODO(mek): Figure out what should happen if the active worker changes or
  // gets uninstalled before this point is reached.
  if (!target_worker_) {
    *result = ServiceWorkerMetrics::REQUEST_JOB_ERROR_NO_ACTIVE_VERSION;
    return nullptr;
  }
  return target_worker_.get();
}

void ForeignFetchRequestHandler::ClearJob() {
  job_.reset();
  target_worker_ = nullptr;
  resource_context_ = nullptr;
}

// static
bool ForeignFetchRequestHandler::CheckOriginTrialToken(
    const ServiceWorkerVersion* const active_version) {
  // The worker entry in the database was written by old version Chrome (< M56)
  // and the main script was not loaded yet. In this case, we can't check the
  // origin trial token.
  if (!active_version->origin_trial_tokens())
    return true;
  const auto& token_map = *active_version->origin_trial_tokens();
  return base::ContainsKey(token_map, "ForeignFetch");
}

}  // namespace content
