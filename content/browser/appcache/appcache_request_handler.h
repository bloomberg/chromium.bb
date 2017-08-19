// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_APPCACHE_APPCACHE_REQUEST_HANDLER_H_
#define CONTENT_BROWSER_APPCACHE_APPCACHE_REQUEST_HANDLER_H_

#include <stdint.h>

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/supports_user_data.h"
#include "content/browser/appcache/appcache_entry.h"
#include "content/browser/appcache/appcache_host.h"
#include "content/browser/appcache/appcache_request_handler.h"
#include "content/browser/appcache/appcache_service_impl.h"
#include "content/browser/loader/url_loader_request_handler.h"
#include "content/browser/url_loader_factory_getter.h"
#include "content/common/content_export.h"
#include "content/public/common/resource_type.h"

namespace net {
class NetworkDelegate;
class URLRequest;
}  // namespace net

namespace content {
class AppCacheJob;
class AppCacheNavigationHandleCore;
class AppCacheRequest;
class AppCacheRequestHandlerTest;
class AppCacheURLRequestJob;
class AppCacheHost;
struct SubresourceLoadInfo;

// An instance is created for each net::URLRequest. The instance survives all
// http transactions involved in the processing of its net::URLRequest, and is
// given the opportunity to hijack the request along the way. Callers
// should use AppCacheHost::CreateRequestHandler to manufacture instances
// that can retrieve resources for a particular host.
class CONTENT_EXPORT AppCacheRequestHandler
    : public base::SupportsUserData::Data,
      public AppCacheHost::Observer,
      public AppCacheServiceImpl::Observer,
      public AppCacheStorage::Delegate,
      public URLLoaderRequestHandler {
 public:
  ~AppCacheRequestHandler() override;

  // These are called on each request intercept opportunity.
  AppCacheJob* MaybeLoadResource(net::NetworkDelegate* network_delegate);
  AppCacheJob* MaybeLoadFallbackForRedirect(
      net::NetworkDelegate* network_delegate,
      const GURL& location);
  AppCacheJob* MaybeLoadFallbackForResponse(
      net::NetworkDelegate* network_delegate);

  void GetExtraResponseInfo(int64_t* cache_id, GURL* manifest_url);

  // Methods to support cross site navigations.
  void PrepareForCrossSiteTransfer(int old_process_id);
  void CompleteCrossSiteTransfer(int new_process_id, int new_host_id);
  void MaybeCompleteCrossSiteTransferInOldProcess(int old_process_id);

  // Useful for detecting storage partition mismatches in the context
  // of cross site transfer navigations.
  bool SanityCheckIsSameService(AppCacheService* service) {
    return !host_ || (host_->service() == service);
  }

  // This method is called in the network service code path for creating a job
  // for handling subresource load requests.
  // The |subresource_load_info| parameter contains the information required to
  // service the load request.
  // The |loader_factory_getter| parameter points to the URLLoaderFactoryGetter
  // instance which provides functionality to return the default network
  // URLLoader interface.
  AppCacheJob* MaybeCreateSubresourceLoader(
      std::unique_ptr<SubresourceLoadInfo> subresource_load_info,
      URLLoaderFactoryGetter* loader_factory_getter);

  static bool IsMainResourceType(ResourceType type) {
    return IsResourceTypeFrame(type) ||
           type == RESOURCE_TYPE_SHARED_WORKER;
  }

  static std::unique_ptr<AppCacheRequestHandler>
  InitializeForNavigationNetworkService(
      const ResourceRequest& request,
      AppCacheNavigationHandleCore* appcache_handle_core,
      URLLoaderFactoryGetter* url_loader_factory_getter);

  // Called by unittests to indicate that we are in test mode.
  static void SetRunningInTests(bool in_tests);

  // Returns true if we are running in tests.
  static bool IsRunningInTests();

 private:
  friend class AppCacheHost;

  // Callers should use AppCacheHost::CreateRequestHandler.
  AppCacheRequestHandler(AppCacheHost* host,
                         ResourceType resource_type,
                         bool should_reset_appcache,
                         std::unique_ptr<AppCacheRequest> request);

  // AppCacheHost::Observer override
  void OnDestructionImminent(AppCacheHost* host) override;

  // AppCacheServiceImpl::Observer override
  void OnServiceDestructionImminent(AppCacheServiceImpl* service) override;

  // Helpers to instruct a waiting job with what response to
  // deliver for the request we're handling.
  void DeliverAppCachedResponse(const AppCacheEntry& entry,
                                int64_t cache_id,
                                const GURL& manifest_url,
                                bool is_fallback,
                                const GURL& namespace_entry_url);
  void DeliverNetworkResponse();
  void DeliverErrorResponse();

  // Called just before the request is restarted. Grabs the reason for
  // restarting, so can correctly continue to handle the request.
  void OnPrepareToRestart();

  // Helper method to create an AppCacheJob and populate job_.
  // Caller takes ownership of returned value.
  std::unique_ptr<AppCacheJob> CreateJob(
      net::NetworkDelegate* network_delegate);

  // Helper method to create an AppCacheJob for fallback responses.
  std::unique_ptr<AppCacheJob> MaybeCreateJobForFallback(
      net::NetworkDelegate* network_delegate);

  // Helper to retrieve a pointer to the storage object.
  AppCacheStorage* storage() const;

  bool is_main_resource() const {
    return IsMainResourceType(resource_type_);
  }

  // Main-resource loading -------------------------------------
  // Frame and SharedWorker main resources are handled here.

  std::unique_ptr<AppCacheJob> MaybeLoadMainResource(
      net::NetworkDelegate* network_delegate);

  // AppCacheStorage::Delegate methods
  void OnMainResponseFound(const GURL& url,
                           const AppCacheEntry& entry,
                           const GURL& fallback_url,
                           const AppCacheEntry& fallback_entry,
                           int64_t cache_id,
                           int64_t group_id,
                           const GURL& mainfest_url) override;

  // Sub-resource loading -------------------------------------
  // Dedicated worker and all manner of sub-resources are handled here.

  std::unique_ptr<AppCacheJob> MaybeLoadSubResource(
      net::NetworkDelegate* network_delegate);
  void ContinueMaybeLoadSubResource();

  // AppCacheHost::Observer override
  void OnCacheSelectionComplete(AppCacheHost* host) override;

  // Network service loading

  // URLLoaderRequestHandler overrides
  // These functions are invoked for loading AppCache content for the frame and
  // for subresources.
  void MaybeCreateLoader(const ResourceRequest& resource_request,
                         ResourceContext* resource_context,
                         LoaderCallback callback) override;
  mojom::URLLoaderFactoryPtr MaybeCreateSubresourceFactory() override;
  bool MaybeCreateLoaderForResponse(
      const ResourceResponseHead& response,
      mojom::URLLoaderPtr* loader,
      mojom::URLLoaderClientRequest* client_request) override;

  // Data members -----------------------------------------------

  // What host we're servicing a request for.
  AppCacheHost* host_;

  // Frame vs subresource vs sharedworker loads are somewhat different.
  ResourceType resource_type_;

  // True if corresponding AppCache group should be resetted before load.
  bool should_reset_appcache_;

  // Subresource requests wait until after cache selection completes.
  bool is_waiting_for_cache_selection_;

  // Info about the type of response we found for delivery.
  // These are relevant for both main and subresource requests.
  int64_t found_group_id_;
  int64_t found_cache_id_;
  AppCacheEntry found_entry_;
  AppCacheEntry found_fallback_entry_;
  GURL found_namespace_entry_url_;
  GURL found_manifest_url_;
  bool found_network_namespace_;

  // True if a cache entry this handler attempted to return was
  // not found in the disk cache. Once set, the handler will take
  // no action on all subsequent intercept opportunities, so the
  // request and any redirects will be handled by the network library.
  bool cache_entry_not_found_;

  // True if the next time this request is started, the response should be
  // delivered from the network, bypassing the AppCache. Cleared after the next
  // intercept opportunity.
  bool is_delivering_network_response_;

  // True if this->MaybeLoadResource(...) has been called in the past.
  bool maybe_load_resource_executed_;

  // The job we use to deliver a response. Only NULL during the following times:
  // 1) Before request has started a job.
  // 2) Request is not being handled by appcache.
  // 3) Request has been cancelled, and the job killed.
  base::WeakPtr<AppCacheJob> job_;

  // During a cross site navigation, we transfer ownership the AppcacheHost
  // from the old processes structures over to the new structures.
  std::unique_ptr<AppCacheHost> host_for_cross_site_transfer_;
  int old_process_id_;
  int old_host_id_;

  // Cached information about the response being currently served by the
  // AppCache, if there is one.
  int cache_id_;
  GURL manifest_url_;

  // Backptr to the central service object.
  AppCacheServiceImpl* service_;

  std::unique_ptr<AppCacheRequest> request_;

  // Network service related members.

  // In the network service world we are queried via the URLLoaderRequestHandler
  // interface to see if the navigation request can be handled via the
  // AppCache. We hold onto the AppCache job created here until the client
  // binds to it (Serviced via AppCache). If the request cannot be handled via
  // the AppCache, we delete the job.
  std::unique_ptr<AppCacheJob> navigation_request_job_;

  // Points to the getter for the network URL loader.
  scoped_refptr<URLLoaderFactoryGetter> network_url_loader_factory_getter_;

  friend class content::AppCacheRequestHandlerTest;

  // Subresource load information.
  std::unique_ptr<SubresourceLoadInfo> subresource_load_info_;

  // The AppCache host instance. We pass this to the
  // AppCacheSubresourceURLFactory instance on creation.
  base::WeakPtr<AppCacheHost> appcache_host_;

  DISALLOW_COPY_AND_ASSIGN(AppCacheRequestHandler);
};

}  // namespace content

#endif  // CONTENT_BROWSER_APPCACHE_APPCACHE_REQUEST_HANDLER_H_
