// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/cache_storage/webserviceworkercachestorage_impl.h"

#include <memory>
#include <utility>

#include "content/child/thread_safe_sender.h"
#include "content/renderer/cache_storage/cache_storage_dispatcher.h"
#include "third_party/WebKit/public/platform/WebHTTPHeaderVisitor.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerCache.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerRequest.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerResponse.h"

using base::TimeTicks;

namespace content {

WebServiceWorkerCacheStorageImpl::WebServiceWorkerCacheStorageImpl(
    ThreadSafeSender* thread_safe_sender,
    const url::Origin& origin)
    : thread_safe_sender_(thread_safe_sender), origin_(origin) {}

WebServiceWorkerCacheStorageImpl::~WebServiceWorkerCacheStorageImpl() {
}

void WebServiceWorkerCacheStorageImpl::dispatchHas(
    std::unique_ptr<CacheStorageCallbacks> callbacks,
    const blink::WebString& cacheName) {
  GetDispatcher()->dispatchHas(std::move(callbacks), origin_, cacheName);
}

void WebServiceWorkerCacheStorageImpl::dispatchOpen(
    std::unique_ptr<CacheStorageWithCacheCallbacks> callbacks,
    const blink::WebString& cacheName) {
  GetDispatcher()->dispatchOpen(std::move(callbacks), origin_, cacheName);
}

void WebServiceWorkerCacheStorageImpl::dispatchDelete(
    std::unique_ptr<CacheStorageCallbacks> callbacks,
    const blink::WebString& cacheName) {
  GetDispatcher()->dispatchDelete(std::move(callbacks), origin_, cacheName);
}

void WebServiceWorkerCacheStorageImpl::dispatchKeys(
    std::unique_ptr<CacheStorageKeysCallbacks> callbacks) {
  GetDispatcher()->dispatchKeys(std::move(callbacks), origin_);
}

void WebServiceWorkerCacheStorageImpl::dispatchMatch(
    std::unique_ptr<CacheStorageMatchCallbacks> callbacks,
    const blink::WebServiceWorkerRequest& request,
    const blink::WebServiceWorkerCache::QueryParams& query_params) {
  GetDispatcher()->dispatchMatch(std::move(callbacks), origin_, request,
                                 query_params);
}

CacheStorageDispatcher* WebServiceWorkerCacheStorageImpl::GetDispatcher()
    const {
  return CacheStorageDispatcher::ThreadSpecificInstance(
      thread_safe_sender_.get());
}

}  // namespace content
