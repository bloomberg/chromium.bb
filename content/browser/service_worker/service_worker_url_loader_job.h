// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_URL_LOADER_JOB_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_URL_LOADER_JOB_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/loader/url_loader_request_handler.h"
#include "content/browser/service_worker/service_worker_fetch_dispatcher.h"
#include "content/browser/service_worker/service_worker_metrics.h"
#include "content/browser/service_worker/service_worker_response_type.h"
#include "content/browser/service_worker/service_worker_url_job_wrapper.h"
#include "content/browser/url_loader_factory_getter.h"
#include "content/common/service_worker/service_worker_status_code.h"
#include "content/common/service_worker/service_worker_types.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "services/network/public/interfaces/url_loader.mojom.h"
#include "third_party/WebKit/common/blob/blob.mojom.h"
#include "third_party/WebKit/common/service_worker/service_worker_stream_handle.mojom.h"

namespace content {

struct ServiceWorkerResponse;
class ServiceWorkerVersion;

// S13nServiceWorker:
// ServiceWorkerURLLoaderJob works similar to ServiceWorkerURLRequestJob
// but with network::mojom::URLLoader instead of URLRequest.
// This class is owned by the job wrapper until it is bound to a URLLoader
// request. After it is bound |this| is kept alive until the Mojo connection
// to this URLLoader is dropped.
class CONTENT_EXPORT ServiceWorkerURLLoaderJob
    : public network::mojom::URLLoader {
 public:
  using Delegate = ServiceWorkerURLJobWrapper::Delegate;
  using ResponseType = ServiceWorkerResponseType;

  // Created by ServiceWorkerControlleeRequestHandler::MaybeCreateLoader
  // when starting to load a page for navigation.
  //
  // This job typically works in the following order:
  // 1. One of the FallbackTo* or ForwardTo* methods are called via
  //    URLJobWrapper by ServiceWorkerControlleeRequestHandler, which
  //    determines how the request should be served (e.g. should fallback
  //    to network or should be sent to the SW). If it decides to fallback
  //    to the network this will call |loader_callback| with null
  //    StartLoaderCallback (which will be then handled by
  //    NavigationURLLoaderNetworkService).
  // 2. If it is decided that the request should be sent to the SW,
  //    this job dispatches a FetchEvent in StartRequest.
  // 3. In DidDispatchFetchEvent() this job determines the request's
  //    final destination, and may still call |loader_callback| with null
  //    StartLoaderCallback if it turns out that we need to fallback to
  //    the network.
  // 4. Otherwise if the SW returned a stream or blob as a response
  //    this job calls |loader_callback| with a bound method for
  //    StartResponse().
  // 5. Then StartResponse() will be called with
  // network::mojom::URLLoaderClientPtr
  //    that is connected to NavigationURLLoaderNetworkService (for resource
  //    loading for navigation). This forwards the blob/stream data pipe to the
  //    NavigationURLLoader if the response body was sent as a blob/stream.
  ServiceWorkerURLLoaderJob(
      LoaderCallback loader_callback,
      Delegate* delegate,
      const network::ResourceRequest& resource_request,
      scoped_refptr<URLLoaderFactoryGetter> url_loader_factory_getter);

  ~ServiceWorkerURLLoaderJob() override;

  // Called via URLJobWrapper.
  void FallbackToNetwork();
  void FallbackToNetworkOrRenderer();
  void ForwardToServiceWorker();
  bool ShouldFallbackToNetwork();
  void FailDueToLostController();
  ui::PageTransition GetPageTransition();
  size_t GetURLChainSize() const;
  void Cancel();
  bool WasCanceled() const;

  // The navigation request that was holding this job is
  // going away. Calling this internally calls |DeleteIfNeeded()|
  // and may delete |this| if it is not bound to a endpoint.
  // Otherwise |this| will be kept around as far as the loader
  // endpoint is held by the client.
  void DetachedFromRequest();

 private:
  class StreamWaiter;

  // For FORWARD_TO_SERVICE_WORKER case.
  void StartRequest();
  void DidPrepareFetchEvent(scoped_refptr<ServiceWorkerVersion> version);
  void DidDispatchFetchEvent(
      ServiceWorkerStatusCode status,
      ServiceWorkerFetchDispatcher::FetchEventResult fetch_result,
      const ServiceWorkerResponse& response,
      blink::mojom::ServiceWorkerStreamHandlePtr body_as_stream,
      blink::mojom::BlobPtr body_as_blob,
      scoped_refptr<ServiceWorkerVersion> version);

  // Used as the StartLoaderCallback passed to |loader_callback_| when the
  // service worker provided a response. Returns the response to |client|.
  // |body_as_blob| is kept around until BlobDataHandle is created from
  // blob_uuid just to make sure the blob is kept alive.
  void StartResponse(const ServiceWorkerResponse& response,
                     scoped_refptr<ServiceWorkerVersion> version,
                     blink::mojom::ServiceWorkerStreamHandlePtr body_as_stream,
                     blink::mojom::BlobPtr body_as_blob,
                     network::mojom::URLLoaderRequest request,
                     network::mojom::URLLoaderClientPtr client);

  // Used as the StartLoaderCallback passed to |loader_callback_| on error.
  // Returns a network error to |client|.
  void StartErrorResponse(network::mojom::URLLoaderRequest request,
                          network::mojom::URLLoaderClientPtr client);

  // Calls url_loader_client_->OnReceiveResopnse() with |response_head_|.
  void CommitResponseHeaders();
  // Calls url_loader_client_->OnComplete(). Expected to be called after
  // CommitResponseHeaders (i.e. status_ == kSentHeader).
  void CommitCompleted(int error_code);

  // Calls |loader_callback_| with StartErrorResponse callback. Must not be
  // called once either StartResponse or StartErrorResponse is called.
  void ReturnNetworkError();

  // network::mojom::URLLoader:
  void FollowRedirect() override;
  void ProceedWithResponse() override;
  void SetPriority(net::RequestPriority priority,
                   int32_t intra_priority_value) override;
  void PauseReadingBodyFromNet() override;
  void ResumeReadingBodyFromNet() override;

  void OnBlobReadingComplete(int net_error);

  void DeleteIfNeeded();

  ResponseType response_type_ = ResponseType::NOT_DETERMINED;
  LoaderCallback loader_callback_;

  Delegate* delegate_;
  network::ResourceRequest resource_request_;
  scoped_refptr<URLLoaderFactoryGetter> url_loader_factory_getter_;
  std::unique_ptr<ServiceWorkerFetchDispatcher> fetch_dispatcher_;
  std::unique_ptr<StreamWaiter> stream_waiter_;
  // The blob needs to be held while it's read to keep it alive.
  blink::mojom::BlobPtr body_as_blob_;

  bool did_navigation_preload_ = false;
  network::ResourceResponseHead response_head_;
  base::Optional<net::SSLInfo> ssl_info_;

  // Pointer to the URLLoaderClient (i.e. NavigationURLLoader).
  network::mojom::URLLoaderClientPtr url_loader_client_;
  mojo::Binding<network::mojom::URLLoader> binding_;

  enum class Status {
    kNotStarted,
    kStarted,
    kSentHeader,
    kCompleted,
    kCancelled
  };
  Status status_ = Status::kNotStarted;

  bool detached_from_request_ = false;

  base::WeakPtrFactory<ServiceWorkerURLLoaderJob> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerURLLoaderJob);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_URL_LOADER_JOB_H_
