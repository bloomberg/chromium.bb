// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_CACHE_STORAGE_DISPATCHER_H_
#define CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_CACHE_STORAGE_DISPATCHER_H_

#include <vector>

#include "base/id_map.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "content/public/renderer/render_process_observer.h"
#include "third_party/WebKit/public/platform/WebServiceWorkerCache.h"
#include "third_party/WebKit/public/platform/WebServiceWorkerCacheError.h"
#include "third_party/WebKit/public/platform/WebServiceWorkerCacheStorage.h"

namespace content {

struct ServiceWorkerFetchRequest;
class ServiceWorkerScriptContext;
struct ServiceWorkerResponse;

// There is one ServiceWorkerCacheStorageDispatcher per
// ServiceWorkerScriptContext. It handles CacheStorage operations with messages
// to/from the browser, creating Cache objects (for which it also handles
// messages) as it goes.

class ServiceWorkerCacheStorageDispatcher
    : public blink::WebServiceWorkerCacheStorage {
 public:
  explicit ServiceWorkerCacheStorageDispatcher(
      ServiceWorkerScriptContext* script_context);
  virtual ~ServiceWorkerCacheStorageDispatcher();

  // ServiceWorkerScriptContext calls our OnMessageReceived directly.
  bool OnMessageReceived(const IPC::Message& message);

  // Message handlers for CacheStorage messages from the browser process.
  void OnCacheStorageHasSuccess(int request_id);
  void OnCacheStorageOpenSuccess(int request_id, int cache_id);
  void OnCacheStorageDeleteSuccess(int request_id);
  void OnCacheStorageKeysSuccess(int request_id,
                                 const std::vector<base::string16>& keys);
  void OnCacheStorageMatchSuccess(int request_id,
                                  const ServiceWorkerResponse& response);

  void OnCacheStorageHasError(int request_id,
                              blink::WebServiceWorkerCacheError reason);
  void OnCacheStorageOpenError(int request_id,
                               blink::WebServiceWorkerCacheError reason);
  void OnCacheStorageDeleteError(int request_id,
                                 blink::WebServiceWorkerCacheError reason);
  void OnCacheStorageKeysError(int request_id,
                               blink::WebServiceWorkerCacheError reason);
  void OnCacheStorageMatchError(int request_id,
                                blink::WebServiceWorkerCacheError reason);

  // Message handlers for Cache messages from the browser process.
  void OnCacheMatchSuccess(int request_id,
                           const ServiceWorkerResponse& response);
  void OnCacheMatchAllSuccess(
      int request_id,
      const std::vector<ServiceWorkerResponse>& response);
  void OnCacheKeysSuccess(
      int request_id,
      const std::vector<ServiceWorkerFetchRequest>& response);
  void OnCacheBatchSuccess(int request_id,
                           const std::vector<ServiceWorkerResponse>& response);

  void OnCacheMatchError(int request_id,
                         blink::WebServiceWorkerCacheError reason);
  void OnCacheMatchAllError(int request_id,
                            blink::WebServiceWorkerCacheError reason);
  void OnCacheKeysError(int request_id,
                        blink::WebServiceWorkerCacheError reason);
  void OnCacheBatchError(int request_id,
                         blink::WebServiceWorkerCacheError reason);

  // From WebServiceWorkerCacheStorage:
  virtual void dispatchHas(CacheStorageCallbacks* callbacks,
                           const blink::WebString& cacheName);
  virtual void dispatchOpen(CacheStorageWithCacheCallbacks* callbacks,
                            const blink::WebString& cacheName);
  virtual void dispatchDelete(CacheStorageCallbacks* callbacks,
                              const blink::WebString& cacheName);
  virtual void dispatchKeys(CacheStorageKeysCallbacks* callbacks);
  virtual void dispatchMatch(
      CacheStorageMatchCallbacks* callbacks,
      const blink::WebServiceWorkerRequest& request,
      const blink::WebServiceWorkerCache::QueryParams& query_params);

  // These methods are used by WebCache to forward events to the browser
  // process.
  void dispatchMatchForCache(
      int cache_id,
      blink::WebServiceWorkerCache::CacheMatchCallbacks* callbacks,
      const blink::WebServiceWorkerRequest& request,
      const blink::WebServiceWorkerCache::QueryParams& query_params);
  void dispatchMatchAllForCache(
      int cache_id,
      blink::WebServiceWorkerCache::CacheWithResponsesCallbacks* callbacks,
      const blink::WebServiceWorkerRequest& request,
      const blink::WebServiceWorkerCache::QueryParams& query_params);
  void dispatchKeysForCache(
      int cache_id,
      blink::WebServiceWorkerCache::CacheWithRequestsCallbacks* callbacks,
      const blink::WebServiceWorkerRequest* request,
      const blink::WebServiceWorkerCache::QueryParams& query_params);
  void dispatchBatchForCache(
      int cache_id,
      blink::WebServiceWorkerCache::CacheWithResponsesCallbacks* callbacks,
      const blink::WebVector<blink::WebServiceWorkerCache::BatchOperation>&
          batch_operations);

  void OnWebCacheDestruction(int cache_id);

 private:
  class WebCache;

  typedef IDMap<CacheStorageCallbacks, IDMapOwnPointer> CallbacksMap;
  typedef IDMap<CacheStorageWithCacheCallbacks, IDMapOwnPointer>
      WithCacheCallbacksMap;
  typedef IDMap<CacheStorageKeysCallbacks, IDMapOwnPointer>
      KeysCallbacksMap;
  typedef IDMap<CacheStorageMatchCallbacks, IDMapOwnPointer>
      StorageMatchCallbacksMap;

  typedef base::hash_map<int32, base::TimeTicks> TimeMap;

  typedef IDMap<blink::WebServiceWorkerCache::CacheMatchCallbacks,
                IDMapOwnPointer> MatchCallbacksMap;
  typedef IDMap<blink::WebServiceWorkerCache::CacheWithResponsesCallbacks,
                IDMapOwnPointer> WithResponsesCallbacksMap;
  typedef IDMap<blink::WebServiceWorkerCache::CacheWithRequestsCallbacks,
                IDMapOwnPointer> WithRequestsCallbacksMap;

  void PopulateWebResponseFromResponse(
      const ServiceWorkerResponse& response,
      blink::WebServiceWorkerResponse* web_response);

  blink::WebVector<blink::WebServiceWorkerResponse> WebResponsesFromResponses(
      const std::vector<ServiceWorkerResponse>& responses);

  // Not owned. The script context containing this object.
  ServiceWorkerScriptContext* script_context_;

  CallbacksMap has_callbacks_;
  WithCacheCallbacksMap open_callbacks_;
  CallbacksMap delete_callbacks_;
  KeysCallbacksMap keys_callbacks_;
  StorageMatchCallbacksMap match_callbacks_;

  TimeMap has_times_;
  TimeMap open_times_;
  TimeMap delete_times_;
  TimeMap keys_times_;
  TimeMap match_times_;

  // The individual caches created under this CacheStorage object.
  IDMap<WebCache, IDMapExternalPointer> web_caches_;

  // These ID maps are held in the CacheStorage object rather than the Cache
  // object to ensure that the IDs are unique.
  MatchCallbacksMap cache_match_callbacks_;
  WithResponsesCallbacksMap cache_match_all_callbacks_;
  WithRequestsCallbacksMap cache_keys_callbacks_;
  WithResponsesCallbacksMap cache_batch_callbacks_;

  TimeMap cache_match_times_;
  TimeMap cache_match_all_times_;
  TimeMap cache_keys_times_;
  TimeMap cache_batch_times_;

  base::WeakPtrFactory<ServiceWorkerCacheStorageDispatcher> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerCacheStorageDispatcher);
};

}  // namespace content

#endif  // CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_CACHE_STORAGE_DISPATCHER_H_
