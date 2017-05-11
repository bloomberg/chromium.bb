// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_APPCACHE_APPCACHE_NETWORK_SERVICE_HANDLER_
#define CONTENT_BROWSER_APPCACHE_APPCACHE_NETWORK_SERVICE_HANDLER_

#include <memory>

#include "base/macros.h"
#include "content/browser/appcache/appcache_service_impl.h"
#include "content/browser/appcache/appcache_storage.h"
#include "content/common/content_export.h"
#include "content/common/url_loader_factory.mojom.h"
#include "content/public/common/resource_type.h"

namespace content {
class AppCacheEntry;
class AppCacheHost;
class AppCacheNavigationHandleCore;
class AppCacheStorage;
class ResourceContext;
struct ResourceRequest;

// This class is instantiated during navigation, to check if the URL being
// navigated to can be served out of the AppCache.
// The AppCacheRequestHandler class provides this functionality as well.
// However it is tightly coupled with the underlying job and the lifetime
// of that class gets a touch complicated. The AppCacheRequestHandler is
// generally associated with a request and it dies when the request goes away.
// For this case, we are just checking if the URL can be served out of the
// cache. If yes, then the plan is to create a URLLoaderFactory which can serve
// URL requests from the cache.
// TODO(ananta)
// Look into whether we can get rid of this class when the overall picture of
// how AppCache interacts with the network service gets clearer.
class CONTENT_EXPORT AppCacheNetworkServiceHandler
    : public AppCacheStorage::Delegate {
 public:
  AppCacheNetworkServiceHandler(
      std::unique_ptr<ResourceRequest> resource_request,
      AppCacheNavigationHandleCore* navigation_handle_core,
      base::Callback<void(mojom::URLLoaderFactoryPtrInfo,
                          std::unique_ptr<ResourceRequest>)> callback);

  ~AppCacheNetworkServiceHandler() override;

  // Called to start the process of looking up the URL in the AppCache
  // database,
  void Start();

  // AppCacheStorage::Delegate methods
  // The AppCacheNetworkServiceHandler instance is deleted on return from this
  // function.
  void OnMainResponseFound(const GURL& url,
                           const AppCacheEntry& entry,
                           const GURL& fallback_url,
                           const AppCacheEntry& fallback_entry,
                           int64_t cache_id,
                           int64_t group_id,
                           const GURL& mainfest_url) override;

 private:
  std::unique_ptr<ResourceRequest> resource_request_;

  // Callback to invoke when we make a determination on whether the request is
  // to be served from the cache or network.
  base::Callback<void(mojom::URLLoaderFactoryPtrInfo,
                      std::unique_ptr<ResourceRequest>)> callback_;

  AppCacheStorage* storage_;

  // The precreated host pointer from the AppCacheNavigationHandleCore class.
  // The ownership of this pointer stays with the AppCacheNavigationHandleCore
  // class.
  AppCacheHost* host_;

  DISALLOW_COPY_AND_ASSIGN(AppCacheNetworkServiceHandler);
};

}  // namespace content

#endif  // CONTENT_BROWSER_APPCACHE_APPCACHE_NETWORK_SERVICE_HANDLER_
