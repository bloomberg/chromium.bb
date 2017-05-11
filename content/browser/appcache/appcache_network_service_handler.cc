// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/appcache/appcache_network_service_handler.h"
#include "base/bind.h"
#include "base/logging.h"
#include "content/browser/appcache/appcache_entry.h"
#include "content/browser/appcache/appcache_host.h"
#include "content/browser/appcache/appcache_navigation_handle_core.h"
#include "content/browser/appcache/appcache_policy.h"
#include "content/browser/appcache/appcache_request.h"
#include "content/public/browser/browser_thread.h"

namespace content {

AppCacheNetworkServiceHandler::AppCacheNetworkServiceHandler(
    std::unique_ptr<ResourceRequest> resource_request,
    AppCacheNavigationHandleCore* navigation_handle_core,
    base::Callback<void(mojom::URLLoaderFactoryPtrInfo,
                        std::unique_ptr<ResourceRequest>)> callback)
    : resource_request_(std::move(resource_request)),
      callback_(callback),
      storage_(navigation_handle_core->host()->storage()),
      host_(navigation_handle_core->host()) {}

AppCacheNetworkServiceHandler::~AppCacheNetworkServiceHandler() {}

void AppCacheNetworkServiceHandler::Start() {
  storage_->FindResponseForMainRequest(resource_request_->url, GURL(), this);
}

void AppCacheNetworkServiceHandler::OnMainResponseFound(
    const GURL& url,
    const AppCacheEntry& entry,
    const GURL& fallback_url,
    const AppCacheEntry& fallback_entry,
    int64_t cache_id,
    int64_t group_id,
    const GURL& manifest_url) {
  AppCachePolicy* policy = host_->service()->appcache_policy();
  bool was_blocked_by_policy =
      !manifest_url.is_empty() && policy &&
      !policy->CanLoadAppCache(manifest_url, host_->first_party_url());

  if (was_blocked_by_policy || !entry.has_response_id() ||
      cache_id == kAppCacheNoCacheId) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(callback_,
                   base::Passed(mojom::URLLoaderFactoryPtrInfo()),
                   base::Passed(std::move(resource_request_))));
  } else {
    DLOG(WARNING) << "AppCache found for url " << url
                  << " falling back to network for now\n";
    // TODO(ananta)
    // Pass a URLLoaderFactory pointer which supports serving URL requests from
    // the cache. The other option is to create the loader directly here.
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(callback_,
                   base::Passed(mojom::URLLoaderFactoryPtrInfo()),
                   base::Passed(std::move(resource_request_))));
  }
  delete this;
}

}  // namespace content