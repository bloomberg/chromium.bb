// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SERVICE_WORKER_WEB_SERVICE_WORKER_CACHE_STORAGE_IMPL_H_
#define CONTENT_RENDERER_SERVICE_WORKER_WEB_SERVICE_WORKER_CACHE_STORAGE_IMPL_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/containers/id_map.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/modules/cache_storage/cache_storage.mojom.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerCache.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerCacheStorage.h"

namespace content {
struct ServiceWorkerResponse;

// This corresponds to an instance of the script-facing CacheStorage object.
// One exists per script execution context (window, worker) and has an origin
// which provides a namespace for the caches. Script calls to the CacheStorage
// object are routed through here via Mojo interface then on to the browser,
// with the origin managed at binding time on browser side. Cache instances
// returned by open() calls return a WebCache to script-facing caller which can
// dispatch calls to browser via CacheRef. WebCache is owned by script-facing
// caller and CacheRef is owned by WebServiceWorkerCacheStorageImpl. CacheRef is
// destroyed once WebCache has been destroyed and there are no pending
// callbacks.
class WebServiceWorkerCacheStorageImpl
    : public blink::WebServiceWorkerCacheStorage {
 public:
  explicit WebServiceWorkerCacheStorageImpl(
      service_manager::InterfaceProvider* provider);

  ~WebServiceWorkerCacheStorageImpl() override;

  // From WebServiceWorkerCacheStorage interface to script-facing:
  void DispatchHas(std::unique_ptr<CacheStorageCallbacks> callbacks,
                   const blink::WebString& cacheName) override;
  void DispatchOpen(std::unique_ptr<CacheStorageWithCacheCallbacks> callbacks,
                    const blink::WebString& cacheName) override;
  void DispatchDelete(std::unique_ptr<CacheStorageCallbacks> callbacks,
                      const blink::WebString& cacheName) override;
  void DispatchKeys(
      std::unique_ptr<CacheStorageKeysCallbacks> callbacks) override;
  void DispatchMatch(
      std::unique_ptr<CacheStorageMatchCallbacks> callbacks,
      const blink::WebServiceWorkerRequest& request,
      const blink::WebServiceWorkerCache::QueryParams& query_params) override;

  // Message handlers for CacheStorage messages from the browser process.
  // And callbacks called by Mojo implementation on browser process.
  void OnCacheStorageHasCallback(
      std::unique_ptr<CacheStorageCallbacks> callbacks,
      base::TimeTicks start_time,
      blink::mojom::CacheStorageError result);
  void OnCacheStorageOpenCallback(
      std::unique_ptr<CacheStorageWithCacheCallbacks> callbacks,
      base::TimeTicks start_time,
      blink::mojom::OpenResultPtr result);
  void CacheStorageDeleteCallback(
      std::unique_ptr<CacheStorageCallbacks> callbacks,
      base::TimeTicks start_time,
      blink::mojom::CacheStorageError result);
  void KeysCallback(std::unique_ptr<CacheStorageKeysCallbacks> callbacks,
                    base::TimeTicks start_time,
                    const std::vector<base::string16>& keys);
  void OnCacheStorageMatchCallback(
      std::unique_ptr<CacheStorageMatchCallbacks> callbacks,
      base::TimeTicks start_time,
      blink::mojom::MatchResultPtr result);

 private:
  class WebCache;
  class CacheRef;

  void PopulateWebResponseFromResponse(
      const ServiceWorkerResponse& response,
      blink::WebServiceWorkerResponse* web_response);

  blink::WebVector<blink::WebServiceWorkerResponse> WebResponsesFromResponses(
      const std::vector<ServiceWorkerResponse>& responses);

  blink::mojom::CacheStorage& GetCacheStorage();

  blink::mojom::CacheStoragePtr cache_storage_ptr_;

  base::WeakPtrFactory<WebServiceWorkerCacheStorageImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(WebServiceWorkerCacheStorageImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_SERVICE_WORKER_WEB_SERVICE_WORKER_CACHE_STORAGE_IMPL_H_
