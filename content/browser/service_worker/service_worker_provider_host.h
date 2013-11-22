// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_PROVIDER_HOST_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_PROVIDER_HOST_H_

#include "base/memory/ref_counted.h"

namespace content {

class ServiceWorkerVersion;

// This class is the browser-process representation of a serice worker
// provider. There is a provider per document and the lifetime of this
// object is tied to the lifetime of its document in the renderer process.
// This class holds service worker state this is scoped to an individual
// document.
class ServiceWorkerProviderHost {
 public:
  ServiceWorkerProviderHost(int process_id,
                            int provider_id);
  ~ServiceWorkerProviderHost();

  int process_id() const { return process_id_; }
  int provider_id() const { return provider_id_; }

  // The service worker version that corresponds with navigator.serviceWorker
  // for our document.
  ServiceWorkerVersion* associated_version() const {
    return  associated_version_.get();
  }

 private:
  const int process_id_;
  const int provider_id_;
  scoped_refptr<ServiceWorkerVersion> associated_version_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_PROVIDER_HOST_H_
