// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CACHE_LISTENER_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CACHE_LISTENER_H_

#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "content/browser/service_worker/embedded_worker_instance.h"
#include "content/browser/service_worker/service_worker_cache_storage.h"

namespace content {

class ServiceWorkerVersion;

// This class listens for requests on the Cache APIs, and sends response
// messages to the renderer process. There is one instance per
// ServiceWorkerVersion instance.
class ServiceWorkerCacheListener : public EmbeddedWorkerInstance::Listener {
 public:
  ServiceWorkerCacheListener(ServiceWorkerVersion* version,
                             base::WeakPtr<ServiceWorkerContextCore> context);
  virtual ~ServiceWorkerCacheListener();

  // From EmbeddedWorkerInstance::Listener:
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // Message receiver functions for CacheStorage API.
  void OnCacheStorageGet(int request_id, const base::string16& cache_name);
  void OnCacheStorageHas(int request_id, const base::string16& cache_name);
  void OnCacheStorageCreate(int request_id,
                          const base::string16& cache_name);
  void OnCacheStorageDelete(int request_id,
                           const base::string16& cache_name);
  void OnCacheStorageKeys(int request_id);

 private:
  void Send(const IPC::Message& message);

  void OnCacheStorageGetCallback(
      int request_id,
      int cache_id,
      ServiceWorkerCacheStorage::CacheStorageError error);
  void OnCacheStorageHasCallback(
      int request_id,
      bool has_cache,
      ServiceWorkerCacheStorage::CacheStorageError error);
  void OnCacheStorageCreateCallback(
      int request_id,
      int cache_id,
      ServiceWorkerCacheStorage::CacheStorageError error);
  void OnCacheStorageDeleteCallback(
      int request_id,
      bool deleted,
      ServiceWorkerCacheStorage::CacheStorageError error);
  void OnCacheStorageKeysCallback(
      int request_id,
      const std::vector<std::string>& strings,
      ServiceWorkerCacheStorage::CacheStorageError error);

  // The ServiceWorkerVersion to use for messaging back to the renderer thread.
  ServiceWorkerVersion* version_;

  // The ServiceWorkerContextCore should always outlive this.
  base::WeakPtr<ServiceWorkerContextCore> context_;

  base::WeakPtrFactory<ServiceWorkerCacheListener> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerCacheListener);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CACHE_LISTENER_H_
