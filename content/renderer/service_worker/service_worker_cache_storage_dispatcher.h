// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_FETCH_STORES_DISPATCHER_H_
#define CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_FETCH_STORES_DISPATCHER_H_

#include <vector>

#include "base/id_map.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "content/public/renderer/render_process_observer.h"
#include "third_party/WebKit/public/platform/WebServiceWorkerCacheError.h"
#include "third_party/WebKit/public/platform/WebServiceWorkerCacheStorage.h"

namespace content {

class ServiceWorkerScriptContext;

// There is one ServiceWorkerCacheStorageDispatcher per
// ServiceWorkerScriptContext. It starts CacheStorage operations with messages
// to the browser process and runs callbacks at operation completion.

class ServiceWorkerCacheStorageDispatcher
    : public blink::WebServiceWorkerCacheStorage {
 public:
  explicit ServiceWorkerCacheStorageDispatcher(
      ServiceWorkerScriptContext* script_context);
  virtual ~ServiceWorkerCacheStorageDispatcher();

  bool OnMessageReceived(const IPC::Message& message);

  // Message handlers for messages from the browser process.
  void OnCacheStorageGetSuccess(int request_id, int cache_id);
  void OnCacheStorageHasSuccess(int request_id);
  void OnCacheStorageCreateSuccess(int request_id, int cache_id);
  void OnCacheStorageDeleteSuccess(int request_id);
  void OnCacheStorageKeysSuccess(int request_id,
                                const std::vector<base::string16>& keys);

  void OnCacheStorageGetError(int request_id,
                             blink::WebServiceWorkerCacheError reason);
  void OnCacheStorageHasError(int request_id,
                             blink::WebServiceWorkerCacheError reason);
  void OnCacheStorageCreateError(int request_id,
                                blink::WebServiceWorkerCacheError reason);
  void OnCacheStorageDeleteError(int request_id,
                                blink::WebServiceWorkerCacheError reason);
  void OnCacheStorageKeysError(int request_id,
                              blink::WebServiceWorkerCacheError reason);

  // From WebServiceWorkerCacheStorage:
  virtual void dispatchGet(CacheStorageWithCacheCallbacks* callbacks,
                           const blink::WebString& cacheName);
  virtual void dispatchHas(CacheStorageCallbacks* callbacks,
                           const blink::WebString& cacheName);
  virtual void dispatchCreate(CacheStorageWithCacheCallbacks* callbacks,
                              const blink::WebString& cacheName);
  virtual void dispatchDelete(CacheStorageCallbacks* callbacks,
                              const blink::WebString& cacheName);
  virtual void dispatchKeys(CacheStorageKeysCallbacks* callbacks);

 private:
  typedef IDMap<CacheStorageCallbacks, IDMapOwnPointer> CallbacksMap;
  typedef IDMap<CacheStorageWithCacheCallbacks, IDMapOwnPointer>
      WithCacheCallbacksMap;
  typedef IDMap<CacheStorageKeysCallbacks, IDMapOwnPointer>
      KeysCallbacksMap;

  // Not owned. The script context containing this object.
  ServiceWorkerScriptContext* script_context_;

  WithCacheCallbacksMap get_callbacks_;
  CallbacksMap has_callbacks_;
  WithCacheCallbacksMap create_callbacks_;
  CallbacksMap delete_callbacks_;
  KeysCallbacksMap keys_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerCacheStorageDispatcher);
};

}  // namespace content

#endif  // CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_CACHE_STORAGE_DISPATCHER_H_
