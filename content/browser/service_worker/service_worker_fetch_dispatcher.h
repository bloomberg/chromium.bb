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
#include "content/public/common/resource_type.h"
#include "content/public/common/url_loader.mojom.h"
#include "content/public/common/url_loader_factory.mojom.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/log/net_log_with_source.h"
#include "third_party/WebKit/common/blob/blob.mojom.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/service_worker_event_status.mojom.h"

namespace net {
class URLRequest;
}  // namespace net

namespace content {

class ServiceWorkerVersion;
class URLLoaderFactoryGetter;

// A helper class to dispatch fetch event to a service worker.
class CONTENT_EXPORT ServiceWorkerFetchDispatcher {
 public:
  using FetchCallback =
      base::Callback<void(ServiceWorkerStatusCode,
                          ServiceWorkerFetchEventResult,
                          const ServiceWorkerResponse&,
                          blink::mojom::ServiceWorkerStreamHandlePtr,
                          blink::mojom::BlobPtr,
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
  // S13nServiceWorker
  // Same as above but for S13N.
  bool MaybeStartNavigationPreloadWithURLLoader(
      const ResourceRequest& original_request,
      URLLoaderFactoryGetter* url_loader_factory_getter,
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
  void DidFailToDispatch(std::unique_ptr<ResponseCallback> callback,
                         ServiceWorkerStatusCode status);
  void DidFail(ServiceWorkerStatusCode status);
  void DidFinish(int request_id,
                 ServiceWorkerFetchEventResult fetch_result,
                 const ServiceWorkerResponse& response,
                 blink::mojom::ServiceWorkerStreamHandlePtr body_as_stream,
                 blink::mojom::BlobPtr body_as_blob);
  void Complete(ServiceWorkerStatusCode status,
                ServiceWorkerFetchEventResult fetch_result,
                const ServiceWorkerResponse& response,
                blink::mojom::ServiceWorkerStreamHandlePtr body_as_stream,
                blink::mojom::BlobPtr body_as_blob);

  static void OnFetchEventFinished(
      ServiceWorkerVersion* version,
      int event_finish_id,
      scoped_refptr<URLLoaderAssets> url_loader_assets,
      blink::mojom::ServiceWorkerEventStatus status,
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

  // |preload_handle_| holds the URLLoader and URLLoaderClient for the service
  // worker to receive the navigation preload response. It's passed to the
  // service worker along with the fetch event.
  mojom::FetchEventPreloadHandlePtr preload_handle_;

  base::WeakPtrFactory<ServiceWorkerFetchDispatcher> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerFetchDispatcher);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_FETCH_DISPATCHER_H_
