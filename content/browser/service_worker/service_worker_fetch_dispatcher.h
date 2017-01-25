// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_FETCH_DISPATCHER_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_FETCH_DISPATCHER_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "content/browser/service_worker/service_worker_metrics.h"
#include "content/common/content_export.h"
#include "content/common/service_worker/service_worker_event_dispatcher.mojom.h"
#include "content/common/service_worker/service_worker_status_code.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/common/url_loader.mojom.h"
#include "content/common/url_loader_factory.mojom.h"
#include "content/public/common/resource_type.h"
#include "net/log/net_log_with_source.h"

namespace net {
class URLRequest;
}  // namespace net

namespace content {

class ServiceWorkerVersion;

// A helper class to dispatch fetch event to a service worker.
class CONTENT_EXPORT ServiceWorkerFetchDispatcher {
 public:
  using FetchCallback =
      base::Callback<void(ServiceWorkerStatusCode,
                          ServiceWorkerFetchEventResult,
                          const ServiceWorkerResponse&,
                          const scoped_refptr<ServiceWorkerVersion>&)>;

  ServiceWorkerFetchDispatcher(
      std::unique_ptr<ServiceWorkerFetchRequest> request,
      ServiceWorkerVersion* version,
      ResourceType resource_type,
      const base::Optional<base::TimeDelta>& timeout,
      const net::NetLogWithSource& net_log,
      const base::Closure& prepare_callback,
      const FetchCallback& fetch_callback);
  ~ServiceWorkerFetchDispatcher();

  // If appropriate, starts the navigation preload request and creates
  // |preload_handle_|. Returns true if it started navigation preload.
  // |on_response| is invoked in OnReceiveResponse().
  bool MaybeStartNavigationPreload(net::URLRequest* original_request,
                                   base::OnceClosure on_response);

  // Dispatches a fetch event to the |version| given in ctor, and fires
  // |fetch_callback| (also given in ctor) when finishes. It runs
  // |prepare_callback| as an intermediate step once the version is activated
  // and running.
  void Run();

 private:
  class ResponseCallback;
  class URLLoaderAssets;

  void DidWaitForActivation();
  void StartWorker();
  void DidStartWorker();
  void DidFailToStartWorker(ServiceWorkerStatusCode status);
  void DispatchFetchEvent();
  void DidFailToDispatch(ServiceWorkerStatusCode status);
  void DidFail(ServiceWorkerStatusCode status);
  void DidFinish(int request_id,
                 ServiceWorkerFetchEventResult fetch_result,
                 const ServiceWorkerResponse& response);
  void Complete(ServiceWorkerStatusCode status,
                ServiceWorkerFetchEventResult fetch_result,
                const ServiceWorkerResponse& response);

  static void OnFetchEventFinished(
      ServiceWorkerVersion* version,
      int event_finish_id,
      scoped_refptr<URLLoaderAssets> url_loader_assets,
      ServiceWorkerStatusCode status,
      base::Time dispatch_event_time);

  ServiceWorkerMetrics::EventType GetEventType() const;

  scoped_refptr<ServiceWorkerVersion> version_;
  net::NetLogWithSource net_log_;
  base::Closure prepare_callback_;
  FetchCallback fetch_callback_;
  std::unique_ptr<ServiceWorkerFetchRequest> request_;
  ResourceType resource_type_;
  base::Optional<base::TimeDelta> timeout_;
  bool did_complete_;

  scoped_refptr<URLLoaderAssets> url_loader_assets_;

  mojom::FetchEventPreloadHandlePtr preload_handle_;

  base::WeakPtrFactory<ServiceWorkerFetchDispatcher> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerFetchDispatcher);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_FETCH_DISPATCHER_H_
