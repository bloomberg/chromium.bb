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
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"
#include "third_party/blink/public/mojom/blob/blob.mojom-forward.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_event_status.mojom-forward.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_stream_handle.mojom-forward.h"
#include "third_party/blink/public/mojom/timing/worker_timing_container.mojom-forward.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace content {

class ControllerServiceWorkerConnector;
class ServiceWorkerSubresourceLoaderFactory;

// A custom URLLoader implementation used by Service Worker controllees
// for loading subresources via the controller Service Worker.
// Currently an instance of this class is created and used only on
// the main thread (while the implementation itself is thread agnostic).
class CONTENT_EXPORT ServiceWorkerSubresourceLoader
    : public network::mojom::URLLoader,
      public blink::mojom::ServiceWorkerFetchResponseCallback,
      public ControllerServiceWorkerConnector::Observer {
 public:
  // See the comments for ServiceWorkerSubresourceLoaderFactory's ctor (below)
  // to see how each parameter is used.
  ServiceWorkerSubresourceLoader(
      mojo::PendingReceiver<network::mojom::URLLoader>,
      int32_t routing_id,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& resource_request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
      scoped_refptr<ControllerServiceWorkerConnector> controller_connector,
      scoped_refptr<network::SharedURLLoaderFactory> fallback_factory,
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      base::WeakPtr<ServiceWorkerSubresourceLoaderFactory>
          service_worker_subresource_loader_factory);

  ~ServiceWorkerSubresourceLoader() override;

  // ControllerServiceWorkerConnector::Observer overrides:
  void OnConnectionClosed() override;

 private:
  class StreamWaiter;
  enum class Status {
    kNotStarted,
    // |binding_| is bound and the fetch event is being dispatched to the
    // service worker.
    kStarted,
    // A redirect happened, waiting for FollowRedirect().
    kSentRedirect,
    // The response head has been sent to |url_loader_client_|.
    kSentHeader,
    // The data pipe for the response body has been sent to
    // |url_loader_client_|. The body is being written to the pipe.
    kSentBody,
    // OnComplete() was called on |url_loader_client_|, or fallback to network
    // occurred so the request was not handled.
    kCompleted,
  };

  void OnMojoDisconnect();

  void StartRequest(const network::ResourceRequest& resource_request);
  void DispatchFetchEvent();
  void DispatchFetchEventForSubresource();
  void OnFetchEventFinished(blink::mojom::ServiceWorkerEventStatus status);
  // Called when this loader no longer needs to restart dispatching the fetch
  // event on failure. Null |status| means the event dispatch was not attempted.
  void SettleFetchEventDispatch(
      base::Optional<blink::ServiceWorkerStatusCode> status);

  // blink::mojom::ServiceWorkerFetchResponseCallback overrides:
  void OnResponse(
      blink::mojom::FetchAPIResponsePtr response,
      blink::mojom::ServiceWorkerFetchEventTimingPtr timing) override;
  void OnResponseStream(
      blink::mojom::FetchAPIResponsePtr response,
      blink::mojom::ServiceWorkerStreamHandlePtr body_as_stream,
      blink::mojom::ServiceWorkerFetchEventTimingPtr timing) override;
  void OnFallback(
      blink::mojom::ServiceWorkerFetchEventTimingPtr timing) override;

  void UpdateResponseTiming(
      blink::mojom::ServiceWorkerFetchEventTimingPtr timing);

  void StartResponse(blink::mojom::FetchAPIResponsePtr response,
                     blink::mojom::ServiceWorkerStreamHandlePtr body_as_stream);

  // network::mojom::URLLoader overrides:
  void FollowRedirect(
      const std::vector<std::string>& removed_headers,
      const net::HttpRequestHeaders& modified_headers,
      const net::HttpRequestHeaders& modified_cors_exempt_headers,
      const base::Optional<GURL>& new_url) override;
  void SetPriority(net::RequestPriority priority,
                   int intra_priority_value) override;
  void PauseReadingBodyFromNet() override;
  void ResumeReadingBodyFromNet() override;

  int StartBlobReading(mojo::ScopedDataPipeConsumerHandle* body_pipe);
  void OnSideDataReadingComplete(mojo::ScopedDataPipeConsumerHandle data_pipe,
                                 base::Optional<mojo_base::BigBuffer> metadata);
  void OnBodyReadingComplete(int net_error);

  // Calls url_loader_client_->OnReceiveResponse() with |response_head_|.
  void CommitResponseHeaders();

  // Calls url_loader_client_->OnStartLoadingResponseBody() with
  // |response_body|.
  void CommitResponseBody(mojo::ScopedDataPipeConsumerHandle response_body);

  // Creates and sends an empty response's body with the net::OK status.
  // Sends net::ERR_INSUFFICIENT_RESOURCES when it can't be created.
  void CommitEmptyResponseAndComplete();

  // Calls url_loader_client_->OnComplete(). Expected to be called after
  // CommitResponseHeaders (i.e. status_ == kSentHeader).
  void CommitCompleted(int error_code);

  // Record loading milestones. Called after a response is completed or
  // a request is fall back to network. Never called when an error is
  // occurred. |handled| is true when a fetch handler handled a request.
  void RecordTimingMetrics(bool handled);

  void TransitionToStatus(Status new_status);

  network::mojom::URLResponseHeadPtr response_head_ =
      network::mojom::URLResponseHead::New();
  base::Optional<net::RedirectInfo> redirect_info_;
  int redirect_limit_;

  mojo::Remote<network::mojom::URLLoaderClient> url_loader_client_;
  mojo::Receiver<network::mojom::URLLoader> url_loader_receiver_;

  // For handling FetchEvent response.
  mojo::Receiver<blink::mojom::ServiceWorkerFetchResponseCallback>
      response_callback_receiver_{this};
  // The blob needs to be held while it's read to keep it alive.
  mojo::Remote<blink::mojom::Blob> body_as_blob_;
  uint64_t body_as_blob_size_;
  // The blob needs to be held while it's read to keep it alive.
  mojo::Remote<blink::mojom::Blob> side_data_as_blob_;

  scoped_refptr<ControllerServiceWorkerConnector> controller_connector_;

  // Observes |controller_connector_| while this loader dispatches a fetch event
  // to the controller. If a broken connection is observed, this loader attempts
  // to restart the controller and dispatch the event again.
  ScopedObserver<ControllerServiceWorkerConnector,
                 ControllerServiceWorkerConnector::Observer>
      controller_connector_observer_{this};
  bool fetch_request_restarted_;
  bool body_reading_complete_;
  bool side_data_reading_complete_;

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

  Status status_ = Status::kNotStarted;

  // The task runner where this loader is running.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  base::WeakPtr<ServiceWorkerSubresourceLoaderFactory>
      service_worker_subresource_loader_factory_;

  blink::mojom::ServiceWorkerFetchEventTimingPtr fetch_event_timing_;
  network::mojom::FetchResponseSource response_source_;

  base::WeakPtrFactory<ServiceWorkerSubresourceLoader> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerSubresourceLoader);
};

// A custom URLLoaderFactory implementation used by Service Worker controllees
// for loading subresources via the controller Service Worker.
// Self destroys when no more bindings exist.
class CONTENT_EXPORT ServiceWorkerSubresourceLoaderFactory
    : public network::mojom::URLLoaderFactory {
 public:
  using WorkerTimingCallback = base::RepeatingCallback<void(
      int /* request_id */,
      mojo::PendingReceiver<blink::mojom::WorkerTimingContainer>)>;

  // |controller_connector| is used to get a connection to the controller
  // ServiceWorker.
  // |network_loader_factory| is used to get the associated loading context's
  // default URLLoaderFactory for network fallback. This should be the
  // URLLoaderFactory that directly goes to network without going through
  // any custom URLLoader factories.
  // |task_runner| is the runner where this loader runs. In production it runs,
  // on a background thread.
  // |worker_timing_callback| is passed the WorkerTimingContainer for the given
  // request_id. It is called on |parent_task_runner|.
  static void Create(
      scoped_refptr<ControllerServiceWorkerConnector> controller_connector,
      scoped_refptr<network::SharedURLLoaderFactory> fallback_factory,
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver,
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      scoped_refptr<base::SequencedTaskRunner> parent_task_runner,
      WorkerTimingCallback worker_timing_callback);

  ~ServiceWorkerSubresourceLoaderFactory() override;

  void AddPendingWorkerTimingReceiver(
      int request_id,
      mojo::PendingReceiver<blink::mojom::WorkerTimingContainer> receiver);

  // network::mojom::URLLoaderFactory overrides:
  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> receiver,
      int32_t routing_id,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& resource_request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override;
  void Clone(mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver)
      override;

 private:
  ServiceWorkerSubresourceLoaderFactory(
      scoped_refptr<ControllerServiceWorkerConnector> controller_connector,
      scoped_refptr<network::SharedURLLoaderFactory> fallback_factory,
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver,
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      scoped_refptr<base::SequencedTaskRunner> parent_task_runner,
      WorkerTimingCallback worker_timing_callback);

  void OnMojoDisconnect();

  scoped_refptr<ControllerServiceWorkerConnector> controller_connector_;

  // Used when a request falls back to network.
  scoped_refptr<network::SharedURLLoaderFactory> fallback_factory_;

  mojo::ReceiverSet<network::mojom::URLLoaderFactory> receivers_;

  // The task runner where this factory is running.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // The task runner of the context (the main thread for frame or worker thread
  // for worker) that is using this factory.
  scoped_refptr<base::SequencedTaskRunner> parent_task_runner_;
  // The callback used to pass the WorkerTimingContainer pending receiver.
  // Called on |parent_task_runner_|.
  WorkerTimingCallback worker_timing_callback_;

  base::WeakPtrFactory<ServiceWorkerSubresourceLoaderFactory> weak_factory_{
      this};

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerSubresourceLoaderFactory);
};

}  // namespace content

#endif  // CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_SUBRESOURCE_LOADER_H_
