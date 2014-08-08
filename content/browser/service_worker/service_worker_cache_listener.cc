// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_cache_listener.h"

#include "content/browser/service_worker/service_worker_version.h"
#include "content/common/service_worker/service_worker_messages.h"
#include "third_party/WebKit/public/platform/WebServiceWorkerCacheError.h"

namespace content {

ServiceWorkerStoresListener::ServiceWorkerStoresListener(
    ServiceWorkerVersion* version) : version_(version) {}

ServiceWorkerStoresListener::~ServiceWorkerStoresListener() {}

bool ServiceWorkerStoresListener::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ServiceWorkerStoresListener, message)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_CacheStorageGet,
                        OnCacheStorageGet)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_CacheStorageHas,
                        OnCacheStorageGet)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_CacheStorageCreate,
                        OnCacheStorageCreate)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_CacheStorageDelete,
                        OnCacheStorageDelete)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_CacheStorageKeys,
                        OnCacheStorageKeys)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void ServiceWorkerStoresListener::OnCacheStorageGet(
    int request_id,
    const base::string16& fetch_store_name) {
  // TODO(gavinp,jkarlin): Implement this method.
  Send(ServiceWorkerMsg_CacheStorageGetError(
      request_id,
      blink::WebServiceWorkerCacheErrorNotImplemented));
}

void ServiceWorkerStoresListener::OnCacheStorageHas(
    int request_id,
    const base::string16& fetch_store_name) {
  // TODO(gavinp,jkarlin): Implement this method.
  Send(ServiceWorkerMsg_CacheStorageHasError(
      request_id,
      blink::WebServiceWorkerCacheErrorNotImplemented));
}

void ServiceWorkerStoresListener::OnCacheStorageCreate(
    int request_id,
    const base::string16& fetch_store_name) {
  // TODO(gavinp,jkarlin): Implement this method.
  Send(ServiceWorkerMsg_CacheStorageCreateError(
      request_id,
      blink::WebServiceWorkerCacheErrorNotImplemented));
}

void ServiceWorkerStoresListener::OnCacheStorageDelete(
    int request_id,
    const base::string16& fetch_store_name) {
  // TODO(gavinp,jkarlin): Implement this method.
  Send(ServiceWorkerMsg_CacheStorageDeleteError(
      request_id, blink::WebServiceWorkerCacheErrorNotImplemented));
}

void ServiceWorkerStoresListener::OnCacheStorageKeys(
    int request_id) {
  // TODO(gavinp,jkarlin): Implement this method.
  Send(ServiceWorkerMsg_CacheStorageKeysError(
      request_id, blink::WebServiceWorkerCacheErrorNotImplemented));
}

void ServiceWorkerStoresListener::Send(const IPC::Message& message) {
  version_->embedded_worker()->SendMessage(message);
}

}  // namespace content
