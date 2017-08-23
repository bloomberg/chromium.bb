// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/appcache/appcache_request_handler.h"

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "content/browser/appcache/appcache.h"
#include "content/browser/appcache/appcache_backend_impl.h"
#include "content/browser/appcache/appcache_host.h"
#include "content/browser/appcache/appcache_navigation_handle_core.h"
#include "content/browser/appcache/appcache_policy.h"
#include "content/browser/appcache/appcache_request.h"
#include "content/browser/appcache/appcache_subresource_url_factory.h"
#include "content/browser/appcache/appcache_url_loader_job.h"
#include "content/browser/appcache/appcache_url_loader_request.h"
#include "content/browser/appcache/appcache_url_request_job.h"
#include "content/browser/service_worker/service_worker_request_handler.h"
#include "content/public/common/content_features.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_job.h"

namespace content {

namespace {

bool g_running_in_tests = false;

}  // namespace

AppCacheRequestHandler::AppCacheRequestHandler(
    AppCacheHost* host,
    ResourceType resource_type,
    bool should_reset_appcache,
    std::unique_ptr<AppCacheRequest> request)
    : host_(host),
      resource_type_(resource_type),
      should_reset_appcache_(should_reset_appcache),
      is_waiting_for_cache_selection_(false),
      found_group_id_(0),
      found_cache_id_(0),
      found_network_namespace_(false),
      cache_entry_not_found_(false),
      is_delivering_network_response_(false),
      maybe_load_resource_executed_(false),
      old_process_id_(0),
      old_host_id_(kAppCacheNoHostId),
      cache_id_(kAppCacheNoCacheId),
      service_(host_->service()),
      request_(std::move(request)) {
  DCHECK(host_);
  DCHECK(service_);
  host_->AddObserver(this);
  service_->AddObserver(this);
}

AppCacheRequestHandler::~AppCacheRequestHandler() {
  if (host_) {
    storage()->CancelDelegateCallbacks(this);
    host_->RemoveObserver(this);
  }
  if (service_)
    service_->RemoveObserver(this);
}

AppCacheStorage* AppCacheRequestHandler::storage() const {
  DCHECK(host_);
  return host_->storage();
}

AppCacheJob* AppCacheRequestHandler::MaybeLoadResource(
    net::NetworkDelegate* network_delegate) {
  maybe_load_resource_executed_ = true;
  if (!host_ ||
      !AppCacheRequest::IsSchemeAndMethodSupportedForAppCache(request_.get()) ||
      cache_entry_not_found_) {
    return NULL;
  }

  // This method can get called multiple times over the life
  // of a request. The case we detect here is having scheduled
  // delivery of a "network response" using a job set up on an
  // earlier call through this method. To send the request through
  // to the network involves restarting the request altogether,
  // which will call through to our interception layer again.
  // This time through, we return NULL so the request hits the wire.
  if (is_delivering_network_response_) {
    is_delivering_network_response_ = false;
    return NULL;
  }

  // Clear out our 'found' fields since we're starting a request for a
  // new resource, any values in those fields are no longer valid.
  found_entry_ = AppCacheEntry();
  found_fallback_entry_ = AppCacheEntry();
  found_cache_id_ = kAppCacheNoCacheId;
  found_manifest_url_ = GURL();
  found_network_namespace_ = false;

  std::unique_ptr<AppCacheJob> job;
  if (is_main_resource())
    job = MaybeLoadMainResource(network_delegate);
  else
    job = MaybeLoadSubResource(network_delegate);

  // If its been setup to deliver a network response, we can just delete
  // it now and return NULL instead to achieve that since it couldn't
  // have been started yet.
  if (job && job->IsDeliveringNetworkResponse() && !job->AsURLLoaderJob()) {
    DCHECK(!job->IsStarted());
    job.reset();
  }

  return job.release();
}

AppCacheJob* AppCacheRequestHandler::MaybeLoadFallbackForRedirect(
    net::NetworkDelegate* network_delegate,
    const GURL& location) {
  if (!host_ ||
      !AppCacheRequest::IsSchemeAndMethodSupportedForAppCache(request_.get()) ||
      cache_entry_not_found_)
    return NULL;
  if (is_main_resource())
    return NULL;
  // TODO(vabr) This is a temporary fix (see crbug/141114). We should get rid of
  // it once a more general solution to crbug/121325 is in place.
  if (!maybe_load_resource_executed_)
    return NULL;
  if (request_->GetURL().GetOrigin() == location.GetOrigin())
    return NULL;

  // In network service land, the existing job initiates a fallback request.
  if (!base::FeatureList::IsEnabled(features::kNetworkService)) {
    DCHECK(!job_.get());  // our jobs never generate redirects
  } else {
    DCHECK(job_.get());
  }

  std::unique_ptr<AppCacheJob> job;
  if (found_fallback_entry_.has_response_id()) {
    job = MaybeCreateJobForFallback(network_delegate);
    // 6.9.6, step 4: If this results in a redirect to another origin,
    // get the resource of the fallback entry.
    DeliverAppCachedResponse(found_fallback_entry_, found_cache_id_,
                             found_manifest_url_, true,
                             found_namespace_entry_url_);
  } else if (!found_network_namespace_) {
    // 6.9.6, step 6: Fail the resource load.
    job = MaybeCreateJobForFallback(network_delegate);
    DeliverErrorResponse();
  } else {
    // 6.9.6 step 3 and 5: Fetch the resource normally.
  }

  return job.release();
}

AppCacheJob* AppCacheRequestHandler::MaybeLoadFallbackForResponse(
    net::NetworkDelegate* network_delegate) {
  if (!host_ ||
      !AppCacheRequest::IsSchemeAndMethodSupportedForAppCache(request_.get()) ||
      cache_entry_not_found_)
    return NULL;
  if (!found_fallback_entry_.has_response_id())
    return NULL;

  if (request_->IsCancelled()) {
    // 6.9.6, step 4: But not if the user canceled the download.
    return NULL;
  }

  // We don't fallback for responses that we delivered.
  if (job_.get()) {
    DCHECK(!job_->IsDeliveringNetworkResponse());
    return NULL;
  }

  if (request_->IsSuccess()) {
    int code_major = request_->GetResponseCode() / 100;
    if (code_major !=4 && code_major != 5)
      return NULL;

    // Servers can override the fallback behavior with a response header.
    const std::string kFallbackOverrideHeader(
        "x-chromium-appcache-fallback-override");
    const std::string kFallbackOverrideValue(
        "disallow-fallback");
    std::string header_value;
    header_value = request_->GetResponseHeaderByName(kFallbackOverrideHeader);
    if (header_value == kFallbackOverrideValue)
      return NULL;
  }

  // 6.9.6, step 4: If this results in a 4xx or 5xx status code
  // or there were network errors, get the resource of the fallback entry.

  // In network service land, the job initiates a fallback request. We reuse
  // the existing job to deliver the fallback response.
  std::unique_ptr<AppCacheJob> job =
      MaybeCreateJobForFallback(network_delegate);

  DeliverAppCachedResponse(found_fallback_entry_, found_cache_id_,
                           found_manifest_url_, true,
                           found_namespace_entry_url_);
  return job.release();
}

void AppCacheRequestHandler::GetExtraResponseInfo(int64_t* cache_id,
                                                  GURL* manifest_url) {
  *cache_id = cache_id_;
  *manifest_url = manifest_url_;
}

void AppCacheRequestHandler::PrepareForCrossSiteTransfer(int old_process_id) {
  if (!host_)
    return;
  AppCacheBackendImpl* backend = host_->service()->GetBackend(old_process_id);
  DCHECK(backend) << "appcache detected likely storage partition mismatch";
  old_process_id_ = old_process_id;
  old_host_id_ = host_->host_id();
  host_for_cross_site_transfer_ = backend->TransferHostOut(host_->host_id());
  DCHECK_EQ(host_, host_for_cross_site_transfer_.get());
}

void AppCacheRequestHandler::CompleteCrossSiteTransfer(
    int new_process_id, int new_host_id) {
  if (!host_for_cross_site_transfer_.get())
    return;
  DCHECK_EQ(host_, host_for_cross_site_transfer_.get());
  AppCacheBackendImpl* backend = host_->service()->GetBackend(new_process_id);
  DCHECK(backend) << "appcache detected likely storage partition mismatch";
  backend->TransferHostIn(new_host_id,
                          std::move(host_for_cross_site_transfer_));
}

void AppCacheRequestHandler::MaybeCompleteCrossSiteTransferInOldProcess(
    int old_process_id) {
  if (!host_ || !host_for_cross_site_transfer_.get() ||
      old_process_id != old_process_id_) {
    return;
  }
  CompleteCrossSiteTransfer(old_process_id_, old_host_id_);
}

AppCacheJob* AppCacheRequestHandler::MaybeCreateSubresourceLoader(
    std::unique_ptr<SubresourceLoadInfo> subresource_load_info,
    URLLoaderFactoryGetter* loader_factory_getter) {
  DCHECK(!is_main_resource());
  DCHECK(base::FeatureList::IsEnabled(features::kNetworkService));

  subresource_load_info_ = std::move(subresource_load_info);
  network_url_loader_factory_getter_ = loader_factory_getter;

  AppCacheJob* job = MaybeLoadResource(nullptr);
  if (!job)
    return nullptr;

  AppCacheURLLoaderJob* loader_job = job->AsURLLoaderJob();
  // The job takes ownership of the handler.
  loader_job->set_request_handler(
      std::unique_ptr<AppCacheRequestHandler>(this));
  return job;
}

// static
std::unique_ptr<AppCacheRequestHandler>
AppCacheRequestHandler::InitializeForNavigationNetworkService(
    const ResourceRequest& request,
    AppCacheNavigationHandleCore* appcache_handle_core,
    URLLoaderFactoryGetter* url_loader_factory_getter) {
  std::unique_ptr<AppCacheRequestHandler> handler =
      appcache_handle_core->host()->CreateRequestHandler(
          AppCacheURLLoaderRequest::Create(request), request.resource_type,
          request.should_reset_appcache);
  handler->network_url_loader_factory_getter_ = url_loader_factory_getter;
  handler->appcache_host_ = appcache_handle_core->host()->GetWeakPtr();
  return handler;
}

void AppCacheRequestHandler::OnDestructionImminent(AppCacheHost* host) {
  storage()->CancelDelegateCallbacks(this);
  host_ = NULL;  // no need to RemoveObserver, the host is being deleted

  // Since the host is being deleted, we don't have to complete any job
  // that is current running. It's destined for the bit bucket anyway.
  if (job_.get()) {
    job_->Kill();
    job_.reset();
  }
}

void AppCacheRequestHandler::OnServiceDestructionImminent(
    AppCacheServiceImpl* service) {
  service_ = nullptr;
  if (!host_) {
    DCHECK(!host_for_cross_site_transfer_);
    DCHECK(!job_);
    return;
  }
  host_->RemoveObserver(this);
  OnDestructionImminent(host_);
  host_for_cross_site_transfer_.reset();
}

void AppCacheRequestHandler::DeliverAppCachedResponse(
    const AppCacheEntry& entry,
    int64_t cache_id,
    const GURL& manifest_url,
    bool is_fallback,
    const GURL& namespace_entry_url) {
  DCHECK(host_ && job_.get() && job_->IsWaiting());
  DCHECK(entry.has_response_id());

  // Cache information about the response, for use by GetExtraResponseInfo.
  cache_id_ = cache_id;
  manifest_url_ = manifest_url;

  if (IsResourceTypeFrame(resource_type_) && !namespace_entry_url.is_empty())
    host_->NotifyMainResourceIsNamespaceEntry(namespace_entry_url);

  job_->DeliverAppCachedResponse(manifest_url, cache_id, entry, is_fallback);
  // In the network service world, we need to release the AppCacheJob instance
  // created for handling navigation requests. These instances will get
  // destroyed when the client disconnects.
  navigation_request_job_.release();
}

void AppCacheRequestHandler::DeliverErrorResponse() {
  DCHECK(job_.get() && job_->IsWaiting());
  DCHECK_EQ(kAppCacheNoCacheId, cache_id_);
  DCHECK(manifest_url_.is_empty());
  job_->DeliverErrorResponse();
  // In the network service world, we need to release the AppCacheJob instance
  // created for handling navigation requests. These instances will get
  // destroyed when the client disconnects.
  navigation_request_job_.release();
}

void AppCacheRequestHandler::DeliverNetworkResponse() {
  DCHECK(job_.get() && job_->IsWaiting());
  DCHECK_EQ(kAppCacheNoCacheId, cache_id_);
  DCHECK(manifest_url_.is_empty());
  job_->DeliverNetworkResponse();
  // In the network service world, we need to destroy the AppCacheJob instance
  // created for handling navigation requests.
  navigation_request_job_.reset(nullptr);
}

void AppCacheRequestHandler::OnPrepareToRestart() {
  DCHECK(job_->IsDeliveringNetworkResponse() || job_->IsCacheEntryNotFound());

  // Any information about the source of the response is no longer relevant.
  cache_id_ = kAppCacheNoCacheId;
  manifest_url_ = GURL();

  cache_entry_not_found_ = job_->IsCacheEntryNotFound();
  is_delivering_network_response_ = job_->IsDeliveringNetworkResponse();

  storage()->CancelDelegateCallbacks(this);

  job_.reset();
}

std::unique_ptr<AppCacheJob> AppCacheRequestHandler::CreateJob(
    net::NetworkDelegate* network_delegate) {
  std::unique_ptr<AppCacheJob> job = AppCacheJob::Create(
      is_main_resource(), host_, storage(), request_.get(), network_delegate,
      base::BindOnce(&AppCacheRequestHandler::OnPrepareToRestart,
                     base::Unretained(this)),
      std::move(subresource_load_info_),
      network_url_loader_factory_getter_.get());
  job_ = job->GetWeakPtr();
  return job;
}

std::unique_ptr<AppCacheJob> AppCacheRequestHandler::MaybeCreateJobForFallback(
    net::NetworkDelegate* network_delegate) {
  if (!base::FeatureList::IsEnabled(features::kNetworkService) ||
      IsMainResourceType(resource_type_)) {
    return CreateJob(network_delegate);
  }
  // In network service land, the job initiates a fallback request. We reuse
  // the existing job to deliver the fallback response.
  DCHECK(job_.get());
  job_->set_delivery_type(AppCacheJob::AWAITING_DELIVERY_ORDERS);
  return std::unique_ptr<AppCacheJob>(job_.get());
}

// Main-resource handling ----------------------------------------------

std::unique_ptr<AppCacheJob> AppCacheRequestHandler::MaybeLoadMainResource(
    net::NetworkDelegate* network_delegate) {
  DCHECK(!job_.get());
  DCHECK(host_);

  // If a page falls into the scope of a ServiceWorker, any matching AppCaches
  // should be ignored. This depends on the ServiceWorker handler being invoked
  // prior to the AppCache handler.
  // TODO(ananta)
  // We need to handle this for AppCache requests initiated for the network
  // service
  if (request_->GetURLRequest() &&
      ServiceWorkerRequestHandler::IsControlledByServiceWorker(
          request_->GetURLRequest())) {
    host_->enable_cache_selection(false);
    return nullptr;
  }

  if (storage()->IsInitialized() &&
      service_->storage()->usage_map()->find(request_->GetURL().GetOrigin()) ==
          service_->storage()->usage_map()->end()) {
    return nullptr;
  }

  host_->enable_cache_selection(true);

  const AppCacheHost* spawning_host =
      (resource_type_ == RESOURCE_TYPE_SHARED_WORKER) ?
      host_ : host_->GetSpawningHost();
  GURL preferred_manifest_url = spawning_host ?
      spawning_host->preferred_manifest_url() : GURL();

  // We may have to wait for our storage query to complete, but
  // this query can also complete syncrhonously.
  std::unique_ptr<AppCacheJob> job = CreateJob(network_delegate);
  storage()->FindResponseForMainRequest(request_->GetURL(),
                                        preferred_manifest_url, this);
  return job;
}

void AppCacheRequestHandler::OnMainResponseFound(
    const GURL& url,
    const AppCacheEntry& entry,
    const GURL& namespace_entry_url,
    const AppCacheEntry& fallback_entry,
    int64_t cache_id,
    int64_t group_id,
    const GURL& manifest_url) {
  DCHECK(host_);
  DCHECK(is_main_resource());
  DCHECK(!entry.IsForeign());
  DCHECK(!fallback_entry.IsForeign());
  DCHECK(!(entry.has_response_id() && fallback_entry.has_response_id()));

  // Request may have been canceled, but not yet deleted, while waiting on
  // the cache.
  if (!job_.get())
    return;

  AppCachePolicy* policy = host_->service()->appcache_policy();
  bool was_blocked_by_policy = !manifest_url.is_empty() && policy &&
      !policy->CanLoadAppCache(manifest_url, host_->first_party_url());

  if (was_blocked_by_policy) {
    if (IsResourceTypeFrame(resource_type_)) {
      host_->NotifyMainResourceBlocked(manifest_url);
    } else {
      DCHECK_EQ(resource_type_, RESOURCE_TYPE_SHARED_WORKER);
      host_->frontend()->OnContentBlocked(host_->host_id(), manifest_url);
    }
    DeliverNetworkResponse();
    return;
  }

  if (should_reset_appcache_ && !manifest_url.is_empty()) {
    host_->service()->DeleteAppCacheGroup(
        manifest_url, net::CompletionCallback());
    DeliverNetworkResponse();
    return;
  }

  if (IsMainResourceType(resource_type_) && cache_id != kAppCacheNoCacheId) {
    // AppCacheHost loads and holds a reference to the main resource cache
    // for two reasons, firstly to preload the cache into the working set
    // in advance of subresource loads happening, secondly to prevent the
    // AppCache from falling out of the working set on frame navigations.
    host_->LoadMainResourceCache(cache_id);
    host_->set_preferred_manifest_url(manifest_url);
  }

  // 6.11.1 Navigating across documents, steps 10 and 14.

  found_entry_ = entry;
  found_namespace_entry_url_ = namespace_entry_url;
  found_fallback_entry_ = fallback_entry;
  found_cache_id_ = cache_id;
  found_group_id_ = group_id;
  found_manifest_url_ = manifest_url;
  found_network_namespace_ = false;  // not applicable to main requests

  if (found_entry_.has_response_id()) {
    DCHECK(!found_fallback_entry_.has_response_id());
    DeliverAppCachedResponse(found_entry_, found_cache_id_, found_manifest_url_,
                             false, found_namespace_entry_url_);
  } else {
    DeliverNetworkResponse();
  }
}

// Sub-resource handling ----------------------------------------------

std::unique_ptr<AppCacheJob> AppCacheRequestHandler::MaybeLoadSubResource(
    net::NetworkDelegate* network_delegate) {
  DCHECK(!job_.get());

  if (host_->is_selection_pending()) {
    // We have to wait until cache selection is complete and the
    // selected cache is loaded.
    is_waiting_for_cache_selection_ = true;
    return CreateJob(network_delegate);
  }

  if (!host_->associated_cache() ||
      !host_->associated_cache()->is_complete() ||
      host_->associated_cache()->owning_group()->is_being_deleted()) {
    return nullptr;
  }

  std::unique_ptr<AppCacheJob> job = CreateJob(network_delegate);
  ContinueMaybeLoadSubResource();
  return job;
}

void AppCacheRequestHandler::ContinueMaybeLoadSubResource() {
  // 6.9.6 Changes to the networking model
  // If the resource is not to be fetched using the HTTP GET mechanism or
  // equivalent ... then fetch the resource normally.
  DCHECK(job_.get());
  DCHECK(host_->associated_cache() && host_->associated_cache()->is_complete());

  const GURL& url = job_->GetURL();
  AppCache* cache = host_->associated_cache();
  storage()->FindResponseForSubRequest(
      host_->associated_cache(), url,
      &found_entry_, &found_fallback_entry_, &found_network_namespace_);

  if (found_entry_.has_response_id()) {
    // Step 2: If there's an entry, get it instead.
    DCHECK(!found_network_namespace_ &&
           !found_fallback_entry_.has_response_id());
    found_cache_id_ = cache->cache_id();
    found_group_id_ = cache->owning_group()->group_id();
    found_manifest_url_ = cache->owning_group()->manifest_url();
    DeliverAppCachedResponse(found_entry_, found_cache_id_, found_manifest_url_,
                             false, GURL());
    return;
  }

  if (found_fallback_entry_.has_response_id()) {
    // Step 4: Fetch the resource normally, if this results
    // in certain conditions, then use the fallback.
    DCHECK(!found_network_namespace_ &&
           !found_entry_.has_response_id());
    found_cache_id_ = cache->cache_id();
    found_manifest_url_ = cache->owning_group()->manifest_url();
    DeliverNetworkResponse();
    return;
  }

  if (found_network_namespace_) {
    // Step 3 and 5: Fetch the resource normally.
    DCHECK(!found_entry_.has_response_id() &&
           !found_fallback_entry_.has_response_id());
    DeliverNetworkResponse();
    return;
  }

  // Step 6: Fail the resource load.
  DeliverErrorResponse();
}

void AppCacheRequestHandler::OnCacheSelectionComplete(AppCacheHost* host) {
  DCHECK(host == host_);

  // Request may have been canceled, but not yet deleted, while waiting on
  // the cache.
  if (!job_.get())
    return;

  if (is_main_resource())
    return;
  if (!is_waiting_for_cache_selection_)
    return;

  is_waiting_for_cache_selection_ = false;

  if (!host_->associated_cache() ||
      !host_->associated_cache()->is_complete()) {
    DeliverNetworkResponse();
    return;
  }

  ContinueMaybeLoadSubResource();
}

void AppCacheRequestHandler::MaybeCreateLoader(
    const ResourceRequest& resource_request,
    ResourceContext* resource_context,
    LoaderCallback callback) {
  // MaybeLoadMainResource will invoke navigation_request_job's methods
  // asynchronously via AppCacheStorage::Delegate.
  navigation_request_job_ = MaybeLoadMainResource(nullptr);
  if (!navigation_request_job_.get()) {
    std::move(callback).Run(StartLoaderCallback());
    return;
  }
  navigation_request_job_->AsURLLoaderJob()->set_main_resource_loader_callback(
      std::move(callback));
}

mojom::URLLoaderFactoryPtr
AppCacheRequestHandler::MaybeCreateSubresourceFactory() {
  mojom::URLLoaderFactoryPtr factory_ptr = nullptr;

  // The factory is destroyed when the renderer drops the connection.
  AppCacheSubresourceURLFactory::CreateURLLoaderFactory(
      network_url_loader_factory_getter_.get(), appcache_host_, &factory_ptr);

  return factory_ptr;
}

bool AppCacheRequestHandler::MaybeCreateLoaderForResponse(
    const ResourceResponseHead& response,
    mojom::URLLoaderPtr* loader,
    mojom::URLLoaderClientRequest* client_request) {
  request_->AsURLLoaderRequest()->set_response(response);
  // The AppCacheJob will get destroyed when the client connection is
  // dropped.
  AppCacheJob* job = MaybeLoadFallbackForResponse(nullptr);
  if (job) {
    mojom::URLLoaderClientPtr client;
    *client_request = mojo::MakeRequest(&client);
    mojom::URLLoaderRequest loader_request = mojo::MakeRequest(loader);

    job->AsURLLoaderJob()->BindRequest(std::move(client),
                                       std::move(loader_request));
    return true;
  }
  return false;
}

// static
void AppCacheRequestHandler::SetRunningInTests(bool in_tests) {
  g_running_in_tests = in_tests;
}

// static
bool AppCacheRequestHandler::IsRunningInTests() {
  return g_running_in_tests;
}

}  // namespace content
