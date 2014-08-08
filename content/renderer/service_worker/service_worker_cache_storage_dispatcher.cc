// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/service_worker/service_worker_cache_storage_dispatcher.h"

#include "base/logging.h"
#include "content/common/service_worker/service_worker_messages.h"
#include "content/public/renderer/render_thread.h"
#include "content/renderer/service_worker/service_worker_script_context.h"

namespace content {

namespace {

template<typename T>
void ClearCallbacksMapWithErrors(T* callbacks_map) {
  typename T::iterator iter(callbacks_map);
  while (!iter.IsAtEnd()) {
    blink::WebServiceWorkerCacheError reason =
        blink::WebServiceWorkerCacheErrorNotFound;
    iter.GetCurrentValue()->onError(&reason);
    callbacks_map->Remove(iter.GetCurrentKey());
    iter.Advance();
  }
}

}  // namespace

ServiceWorkerCacheStorageDispatcher::ServiceWorkerCacheStorageDispatcher(
    ServiceWorkerScriptContext* script_context)
  : script_context_(script_context) {}

ServiceWorkerCacheStorageDispatcher::~ServiceWorkerCacheStorageDispatcher() {
  ClearCallbacksMapWithErrors(&get_callbacks_);
  ClearCallbacksMapWithErrors(&has_callbacks_);
  ClearCallbacksMapWithErrors(&create_callbacks_);
  ClearCallbacksMapWithErrors(&delete_callbacks_);
  ClearCallbacksMapWithErrors(&keys_callbacks_);
}

bool ServiceWorkerCacheStorageDispatcher::OnMessageReceived(
    const IPC::Message& message)  {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ServiceWorkerCacheStorageDispatcher, message)
     IPC_MESSAGE_HANDLER(ServiceWorkerMsg_CacheStorageGetSuccess,
                         OnCacheStorageGetSuccess)
     IPC_MESSAGE_HANDLER(ServiceWorkerMsg_CacheStorageHasSuccess,
                         OnCacheStorageHasSuccess)
     IPC_MESSAGE_HANDLER(ServiceWorkerMsg_CacheStorageCreateSuccess,
                         OnCacheStorageCreateSuccess)
     IPC_MESSAGE_HANDLER(ServiceWorkerMsg_CacheStorageDeleteSuccess,
                         OnCacheStorageDeleteSuccess)
     IPC_MESSAGE_HANDLER(ServiceWorkerMsg_CacheStorageKeysSuccess,
                         OnCacheStorageKeysSuccess)
     IPC_MESSAGE_HANDLER(ServiceWorkerMsg_CacheStorageGetError,
                         OnCacheStorageGetError)
     IPC_MESSAGE_HANDLER(ServiceWorkerMsg_CacheStorageHasError,
                         OnCacheStorageHasError)
     IPC_MESSAGE_HANDLER(ServiceWorkerMsg_CacheStorageCreateError,
                         OnCacheStorageCreateError)
     IPC_MESSAGE_HANDLER(ServiceWorkerMsg_CacheStorageDeleteError,
                         OnCacheStorageDeleteError)
     IPC_MESSAGE_HANDLER(ServiceWorkerMsg_CacheStorageKeysError,
                         OnCacheStorageKeysError)
     IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void ServiceWorkerCacheStorageDispatcher::OnCacheStorageGetSuccess(
    int request_id,
    int cache_id) {
  CacheStorageWithCacheCallbacks* callbacks =
      get_callbacks_.Lookup(request_id);
  DCHECK(callbacks);
  callbacks->onSuccess(NULL);
  get_callbacks_.Remove(request_id);
}

void ServiceWorkerCacheStorageDispatcher::OnCacheStorageHasSuccess(
    int request_id) {
  CacheStorageCallbacks* callbacks = has_callbacks_.Lookup(request_id);
  DCHECK(callbacks);
  callbacks->onSuccess();
  has_callbacks_.Remove(request_id);
}

void ServiceWorkerCacheStorageDispatcher::OnCacheStorageCreateSuccess(
    int request_id,
    int cache_id) {
  CacheStorageWithCacheCallbacks* callbacks =
      create_callbacks_.Lookup(request_id);
  DCHECK(callbacks);
  callbacks->onSuccess(NULL);
  create_callbacks_.Remove(request_id);
}

void ServiceWorkerCacheStorageDispatcher::OnCacheStorageDeleteSuccess(
    int request_id) {
  CacheStorageCallbacks* callbacks = delete_callbacks_.Lookup(request_id);
  DCHECK(callbacks);
  callbacks->onSuccess();
  delete_callbacks_.Remove(request_id);
}

void ServiceWorkerCacheStorageDispatcher::OnCacheStorageKeysSuccess(
    int request_id,
    const std::vector<base::string16>& keys) {
  CacheStorageKeysCallbacks* callbacks = keys_callbacks_.Lookup(request_id);
  DCHECK(callbacks);
  blink::WebVector<blink::WebString> webKeys(keys.size());
  for (size_t i = 0; i < keys.size(); ++i)
    webKeys[i] = keys[i];

  callbacks->onSuccess(&webKeys);
  keys_callbacks_.Remove(request_id);
}

void ServiceWorkerCacheStorageDispatcher::OnCacheStorageGetError(
      int request_id,
      blink::WebServiceWorkerCacheError reason) {
  CacheStorageWithCacheCallbacks* callbacks =
      get_callbacks_.Lookup(request_id);
  DCHECK(callbacks);
  callbacks->onError(&reason);
  get_callbacks_.Remove(request_id);
}

void ServiceWorkerCacheStorageDispatcher::OnCacheStorageHasError(
      int request_id,
      blink::WebServiceWorkerCacheError reason) {
  CacheStorageCallbacks* callbacks = has_callbacks_.Lookup(request_id);
  DCHECK(callbacks);
  callbacks->onError(&reason);
  has_callbacks_.Remove(request_id);
}

void ServiceWorkerCacheStorageDispatcher::OnCacheStorageCreateError(
    int request_id,
    blink::WebServiceWorkerCacheError reason) {
  CacheStorageWithCacheCallbacks* callbacks =
      create_callbacks_.Lookup(request_id);
  DCHECK(callbacks);
  callbacks->onError(&reason);
  create_callbacks_.Remove(request_id);
}

void ServiceWorkerCacheStorageDispatcher::OnCacheStorageDeleteError(
      int request_id,
      blink::WebServiceWorkerCacheError reason) {
  CacheStorageCallbacks* callbacks = delete_callbacks_.Lookup(request_id);
  DCHECK(callbacks);
  callbacks->onError(&reason);
  delete_callbacks_.Remove(request_id);
}

void ServiceWorkerCacheStorageDispatcher::OnCacheStorageKeysError(
      int request_id,
      blink::WebServiceWorkerCacheError reason) {
  CacheStorageKeysCallbacks* callbacks = keys_callbacks_.Lookup(request_id);
  DCHECK(callbacks);
  callbacks->onError(&reason);
  keys_callbacks_.Remove(request_id);
}

void ServiceWorkerCacheStorageDispatcher::dispatchGet(
    CacheStorageWithCacheCallbacks* callbacks,
    const blink::WebString& cacheName) {
  int request_id = get_callbacks_.Add(callbacks);
  script_context_->Send(new ServiceWorkerHostMsg_CacheStorageGet(
      script_context_->GetRoutingID(), request_id, cacheName));
}

void ServiceWorkerCacheStorageDispatcher::dispatchHas(
    CacheStorageCallbacks* callbacks,
    const blink::WebString& cacheName) {
  int request_id = delete_callbacks_.Add(callbacks);
  script_context_->Send(new ServiceWorkerHostMsg_CacheStorageDelete(
      script_context_->GetRoutingID(), request_id, cacheName));
}

void ServiceWorkerCacheStorageDispatcher::dispatchCreate(
    CacheStorageWithCacheCallbacks* callbacks,
    const blink::WebString& cacheName) {
  int request_id = create_callbacks_.Add(callbacks);
  script_context_->Send(new ServiceWorkerHostMsg_CacheStorageCreate(
      script_context_->GetRoutingID(), request_id, cacheName));
}

void ServiceWorkerCacheStorageDispatcher::dispatchDelete(
    CacheStorageCallbacks* callbacks,
    const blink::WebString& cacheName) {
  int request_id = delete_callbacks_.Add(callbacks);
  script_context_->Send(new ServiceWorkerHostMsg_CacheStorageDelete(
      script_context_->GetRoutingID(), request_id, cacheName));
}

void ServiceWorkerCacheStorageDispatcher::dispatchKeys(
    CacheStorageKeysCallbacks* callbacks) {
  int request_id = keys_callbacks_.Add(callbacks);
  script_context_->Send(new ServiceWorkerHostMsg_CacheStorageKeys(
      script_context_->GetRoutingID(), request_id));
}

}  // namespace content
