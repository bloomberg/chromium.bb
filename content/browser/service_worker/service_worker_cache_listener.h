// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_STORES_LISTENER_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_STORES_LISTENER_H_

#include "base/strings/string16.h"
#include "content/browser/service_worker/embedded_worker_instance.h"

namespace content {

class ServiceWorkerVersion;

// This class listens for requests on the Store APIs, and sends response
// messages to the renderer process. There is one instance per
// ServiceWorkerVersion instance.
class ServiceWorkerStoresListener : public EmbeddedWorkerInstance::Listener {
 public:
  explicit ServiceWorkerStoresListener(ServiceWorkerVersion* version);
  virtual ~ServiceWorkerStoresListener();

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

  // The ServiceWorkerVersion to use for messaging back to the renderer thread.
  ServiceWorkerVersion* version_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_STORES_LISTENER_H_
