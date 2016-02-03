// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_FETCH_DISPATCHER_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_FETCH_DISPATCHER_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "content/common/service_worker/service_worker_status_code.h"
#include "content/common/service_worker/service_worker_types.h"

namespace content {

class ServiceWorkerVersion;

// A helper class to dispatch fetch event to a service worker.
class CONTENT_EXPORT ServiceWorkerFetchDispatcher {
 public:
  typedef base::Callback<void(ServiceWorkerStatusCode,
                              ServiceWorkerFetchEventResult,
                              const ServiceWorkerResponse&,
                              const scoped_refptr<ServiceWorkerVersion>&)>
      FetchCallback;

  ServiceWorkerFetchDispatcher(scoped_ptr<ServiceWorkerFetchRequest> request,
                               ServiceWorkerVersion* version,
                               const base::Closure& prepare_callback,
                               const FetchCallback& fetch_callback);
  ~ServiceWorkerFetchDispatcher();

  // Dispatches a fetch event to the |version| given in ctor, and fires
  // |callback| (also given in ctor) when finishes.
  void Run();

 private:
  void DidWaitActivation();
  void DidFailActivation();
  void DispatchFetchEvent();
  void DidPrepare();
  void DidFail(ServiceWorkerStatusCode status);
  void DidFinish(int request_id,
                 ServiceWorkerFetchEventResult fetch_result,
                 const ServiceWorkerResponse& response);

  scoped_refptr<ServiceWorkerVersion> version_;
  base::Closure prepare_callback_;
  FetchCallback fetch_callback_;
  scoped_ptr<ServiceWorkerFetchRequest> request_;
  base::WeakPtrFactory<ServiceWorkerFetchDispatcher> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerFetchDispatcher);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_FETCH_DISPATCHER_H_
