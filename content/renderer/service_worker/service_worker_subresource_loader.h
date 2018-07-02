// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_SUBRESOURCE_LOADER_H_
#define CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_SUBRESOURCE_LOADER_H_

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/scoped_observer.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "content/renderer/service_worker/controller_service_worker_connector.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"
#include "third_party/blink/public/mojom/blob/blob.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_event_status.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_stream_handle.mojom.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace content {

class ControllerServiceWorkerConnector;

// S13nServiceWorker:
// A custom URLLoader implementation used by Service Worker controllees
// for loading subresources via the controller Service Worker.
// Currently an instance of this class is created and used only on
// the main thread (while the implementation itself is thread agnostic).
class CONTENT_EXPORT ServiceWorkerSubresourceLoader
    : public network::mojom::URLLoader,
      public mojom::ServiceWorkerFetchResponseCallback,
      public ControllerServiceWorkerConnector::Observer {
 public:
  // See the comments for ServiceWorkerSubresourceLoaderFactory's ctor (below)
  // to see how each parameter is used.
  ServiceWorkerSubresourceLoader(
      network::mojom::URLLoaderRequest request,
      int32_t routing_id,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& resource_request,
      network::mojom::URLLoaderClientPtr client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
      scoped_refptr<ControllerServiceWorkerConnector> controller_connector,
      scoped_refptr<network::SharedURLLoaderFactory> fallback_factory,
      scoped_refptr<base::SequencedTaskRunner> task_runner);

  ~ServiceWorkerSubresourceLoader() override;

  // ControllerServiceWorkerConnector::Observer overrides:
  void OnConnectionClosed() override;

 private:
  class StreamWaiter;

  void OnConnectionError();

  void StartRequest(const network::ResourceRequest& resource_request);
  void DispatchFetchEvent();
  void OnFetchEventFinished(base::Time request_dispatch_time,
                            blink::mojom::ServiceWorkerEventStatus status,
                            base::Time dispatch_event_time);
  // Called when this loader no longer needs to restart dispatching the fetch
  // event on failure. Null |status| means the event dispatch was not attempted.
  void SettleFetchEventDispatch(
      base::Optional<blink::ServiceWorkerStatusCode> status);

  // mojom::ServiceWorkerFetchResponseCallback overrides:
  void OnResponse(const ServiceWorkerResponse& response,
                  base::Time dispatch_event_time) override;
  void OnResponseBlob(const ServiceWorkerResponse& response,
                      blink::mojom::BlobPtr blob,
                      base::Time dispatch_event_time) override;
  void OnResponseStream(
      const ServiceWorkerResponse& response,
      blink::mojom::ServiceWorkerStreamHandlePtr body_as_stream,
      base::Time dispatch_event_time) override;
  void OnFallback(base::Time dispatch_event_time) override;

  void StartResponse(const ServiceWorkerResponse& response,
                     blink::mojom::BlobPtr body_as_blob,
                     blink::mojom::ServiceWorkerStreamHandlePtr body_as_stream);

  // network::mojom::URLLoader overrides:
  void FollowRedirect(const base::Optional<std::vector<std::string>>&
                          to_be_removed_request_headers,
                      const base::Optional<net::HttpRequestHeaders>&
                          modified_request_headers) override;
  void ProceedWithResponse() override;
  void SetPriority(net::RequestPriority priority,
                   int intra_priority_value) override;
  void PauseReadingBodyFromNet() override;
  void ResumeReadingBodyFromNet() override;

  void OnBlobSideDataReadingComplete(
      const base::Optional<std::vector<uint8_t>>& metadata);
  void OnBlobReadingComplete(int net_error);

  // Calls url_loader_client_->OnReceiveResponse() with |response_head_|.
  void CommitResponseHeaders();
  // Calls url_loader_client_->OnComplete(). Expected to be called after
  // CommitResponseHeaders (i.e. status_ == kSentHeader).
  void CommitCompleted(int error_code);

  network::ResourceResponseHead response_head_;
  base::Optional<net::RedirectInfo> redirect_info_;
  int redirect_limit_;

  network::mojom::URLLoaderClientPtr url_loader_client_;
  mojo::Binding<network::mojom::URLLoader> url_loader_binding_;

  // For handling FetchEvent response.
  mojo::Binding<ServiceWorkerFetchResponseCallback> response_callback_binding_;
  // The blob needs to be held while it's read to keep it alive.
  blink::mojom::BlobPtr body_as_blob_;

  scoped_refptr<ControllerServiceWorkerConnector> controller_connector_;

  // Observes |controller_connector_| while this loader dispatches a fetch event
  // to the controller. If a broken connection is observed, this loader attempts
  // to restart the controller and dispatch the event again.
  ScopedObserver<ControllerServiceWorkerConnector,
                 ServiceWorkerSubresourceLoader>
      controller_connector_observer_;
  bool fetch_request_restarted_;

  // These are given by the constructor (as the params for
  // URLLoaderFactory::CreateLoaderAndStart).
  const int routing_id_;
  const int request_id_;
  const uint32_t options_;
  net::MutableNetworkTrafficAnnotationTag traffic_annotation_;

  // |resource_request_| is initialized in the constructor, and may change
  // over the lifetime of this loader due to redirects.
  network::ResourceRequest resource_request_;

  std::unique_ptr<StreamWaiter> stream_waiter_;

  // For network fallback.
  scoped_refptr<network::SharedURLLoaderFactory> fallback_factory_;

  enum class Status {
    kNotStarted,
    kStarted,
    kSentHeader,
    kCompleted,
  };
  Status status_ = Status::kNotStarted;

  // The task runner where this loader is running.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  base::WeakPtrFactory<ServiceWorkerSubresourceLoader> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerSubresourceLoader);
};

// S13nServiceWorker:
// A custom URLLoaderFactory implementation used by Service Worker controllees
// for loading subresources via the controller Service Worker.
// Self destroys when no more bindings exist.
class CONTENT_EXPORT ServiceWorkerSubresourceLoaderFactory
    : public network::mojom::URLLoaderFactory {
 public:
  // |controller_connector| is used to get a connection to the controller
  // ServiceWorker.
  // |network_loader_factory| is used to get the associated loading context's
  // default URLLoaderFactory for network fallback. This should be the
  // URLLoaderFactory that directly goes to network without going through
  // any custom URLLoader factories.
  // |task_runner| is the runner where this loader runs. (We need to pass
  // this around because calling base::SequencedTaskRunnerHandle is
  // prohibited in the renderer :()
  static void Create(
      scoped_refptr<ControllerServiceWorkerConnector> controller_connector,
      scoped_refptr<network::SharedURLLoaderFactory> fallback_factory,
      network::mojom::URLLoaderFactoryRequest request,
      scoped_refptr<base::SequencedTaskRunner> task_runner);

  ~ServiceWorkerSubresourceLoaderFactory() override;

  // network::mojom::URLLoaderFactory overrides:
  void CreateLoaderAndStart(network::mojom::URLLoaderRequest request,
                            int32_t routing_id,
                            int32_t request_id,
                            uint32_t options,
                            const network::ResourceRequest& resource_request,
                            network::mojom::URLLoaderClientPtr client,
                            const net::MutableNetworkTrafficAnnotationTag&
                                traffic_annotation) override;
  void Clone(network::mojom::URLLoaderFactoryRequest request) override;

 private:
  ServiceWorkerSubresourceLoaderFactory(
      scoped_refptr<ControllerServiceWorkerConnector> controller_connector,
      scoped_refptr<network::SharedURLLoaderFactory> fallback_factory,
      network::mojom::URLLoaderFactoryRequest request,
      scoped_refptr<base::SequencedTaskRunner> task_runner);

  void OnConnectionError();

  scoped_refptr<ControllerServiceWorkerConnector> controller_connector_;

  // Used when a request falls back to network.
  scoped_refptr<network::SharedURLLoaderFactory> fallback_factory_;

  mojo::BindingSet<network::mojom::URLLoaderFactory> bindings_;

  // The task runner where this factory is running.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerSubresourceLoaderFactory);
};

}  // namespace content

#endif  // CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_SUBRESOURCE_LOADER_H_
