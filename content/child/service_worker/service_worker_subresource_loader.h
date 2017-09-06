// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_SERVICE_WORKER_SERVICE_WORKER_SUBRESOURCE_LOADER_H_
#define CONTENT_CHILD_SERVICE_WORKER_SERVICE_WORKER_SUBRESOURCE_LOADER_H_

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "content/common/service_worker/service_worker_event_dispatcher.mojom.h"
#include "content/common/service_worker/service_worker_status_code.h"
#include "content/public/common/url_loader_factory.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/service_worker_stream_handle.mojom.h"

namespace content {

class ChildURLLoaderFactoryGetter;

// S13nServiceWorker:
// A custom URLLoader implementation used by Service Worker controllees
// for loading subresources via the controller Service Worker.
// Currently an instance of this class is created and used only on
// the main thread (while the implementation itself is thread agnostic).
// TODO(kinuko): Consider factoring out the common part with
// ServiceWorkerURLLoaderJob into WebKit/common.
class CONTENT_EXPORT ServiceWorkerSubresourceLoader
    : public mojom::URLLoader,
      public mojom::URLLoaderClient,
      public mojom::ServiceWorkerFetchResponseCallback {
 public:
  ServiceWorkerSubresourceLoader(
      mojom::URLLoaderRequest request,
      int32_t routing_id,
      int32_t request_id,
      uint32_t options,
      const ResourceRequest& resource_request,
      mojom::URLLoaderClientPtr client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
      scoped_refptr<
          base::RefCountedData<mojom::ServiceWorkerEventDispatcherPtr>>
          event_dispatcher,
      scoped_refptr<ChildURLLoaderFactoryGetter> default_loader_factory_getter,
      const GURL& controller_origin);

  ~ServiceWorkerSubresourceLoader() override;

 private:
  void DeleteSoon();

  void StartRequest(const ResourceRequest& resource_request);
  void OnFetchEventFinished(ServiceWorkerStatusCode status,
                            base::Time dispatch_event_time);

  // mojom::ServiceWorkerFetchResponseCallback overrides:
  void OnResponse(const ServiceWorkerResponse& response,
                  base::Time dispatch_event_time) override;
  void OnResponseBlob(const ServiceWorkerResponse& response,
                      storage::mojom::BlobPtr blob,
                      base::Time dispatch_event_time) override;
  void OnResponseStream(
      const ServiceWorkerResponse& response,
      blink::mojom::ServiceWorkerStreamHandlePtr body_as_stream,
      base::Time dispatch_event_time) override;
  void OnFallback(base::Time dispatch_event_time) override;

  void StartResponse(const ServiceWorkerResponse& response,
                     storage::mojom::BlobPtr blob,
                     blink::mojom::ServiceWorkerStreamHandlePtr body_as_stream);

  // mojom::URLLoader overrides:
  void FollowRedirect() override;
  void SetPriority(net::RequestPriority priority,
                   int intra_priority_value) override;

  // Calls url_loader_client_->OnReceiveResponse() with |response_head_|.
  void CommitResponseHeaders();
  // Calls url_loader_client_->OnComplete(). Expected to be called after
  // CommitResponseHeaders (i.e. status_ == kSentHeader).
  void CommitCompleted(int error_code);
  // Calls CommitResponseHeaders() if we haven't sent headers yet,
  // and CommitCompleted() with error code.
  void DeliverErrorResponse();

  // mojom::URLLoaderClient for Blob response reading (used only when
  // the service worker response had valid Blob UUID):
  void OnReceiveResponse(const ResourceResponseHead& response_head,
                         const base::Optional<net::SSLInfo>& ssl_info,
                         mojom::DownloadedTempFilePtr downloaded_file) override;
  void OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                         const ResourceResponseHead& response_head) override;
  void OnDataDownloaded(int64_t data_len, int64_t encoded_data_len) override;
  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        OnUploadProgressCallback ack_callback) override;
  void OnReceiveCachedMetadata(const std::vector<uint8_t>& data) override;
  void OnTransferSizeUpdated(int32_t transfer_size_diff) override;
  void OnStartLoadingResponseBody(
      mojo::ScopedDataPipeConsumerHandle body) override;
  void OnComplete(const ResourceRequestCompletionStatus& status) override;

  ResourceResponseHead response_head_;

  mojom::URLLoaderClientPtr url_loader_client_;
  mojo::Binding<mojom::URLLoader> url_loader_binding_;

  // For handling FetchEvent response.
  mojo::Binding<ServiceWorkerFetchResponseCallback> response_callback_binding_;

  scoped_refptr<base::RefCountedData<mojom::ServiceWorkerEventDispatcherPtr>>
      event_dispatcher_;

  // These are given by the constructor (as the params for
  // URLLoaderFactory::CreateLoaderAndStart).
  const int routing_id_;
  const int request_id_;
  const uint32_t options_;
  ResourceRequest resource_request_;
  net::MutableNetworkTrafficAnnotationTag traffic_annotation_;

  // To load a blob.
  GURL blob_url_;
  GURL controller_origin_;
  mojom::URLLoaderPtr blob_loader_;
  mojo::Binding<mojom::URLLoaderClient> blob_client_binding_;

  // For Blob loading and network fallback loading.
  scoped_refptr<ChildURLLoaderFactoryGetter> default_loader_factory_getter_;

  enum class Status {
    kNotStarted,
    kStarted,
    kSentHeader,
    kCompleted,
    kCancelled
  };
  Status status_ = Status::kNotStarted;

  base::WeakPtrFactory<ServiceWorkerSubresourceLoader> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerSubresourceLoader);
};

// S13nServiceWorker:
// A custom URLLoaderFactory implementation used by Service Worker controllees
// for loading subresources via the controller Service Worker.
class CONTENT_EXPORT ServiceWorkerSubresourceLoaderFactory
    : public mojom::URLLoaderFactory {
 public:
  // |event_dispatcher| is used to dispatch FetchEvent to the controller
  // ServiceWorker.
  // |default_loader_factory_getter| contains a set of default loader
  // factories for the associated loading context, used to get
  // URLLoaderFactory's for Blob and Network for loading a blob response and for
  // network fallback. |controller_origin| is used to create a new Blob public
  // URL (this will become unnecessary once we switch over to MojoBlobs).
  ServiceWorkerSubresourceLoaderFactory(
      scoped_refptr<base::RefCountedData<
          mojom::ServiceWorkerEventDispatcherPtr>> event_dispatcher,
      scoped_refptr<ChildURLLoaderFactoryGetter> default_loader_factory_getter,
      const GURL& controller_origin);

  ~ServiceWorkerSubresourceLoaderFactory() override;

  // mojom::URLLoaderFactory overrides:
  void CreateLoaderAndStart(mojom::URLLoaderRequest request,
                            int32_t routing_id,
                            int32_t request_id,
                            uint32_t options,
                            const ResourceRequest& resource_request,
                            mojom::URLLoaderClientPtr client,
                            const net::MutableNetworkTrafficAnnotationTag&
                                traffic_annotation) override;
  void Clone(mojom::URLLoaderFactoryRequest request) override;

 private:
  scoped_refptr<base::RefCountedData<mojom::ServiceWorkerEventDispatcherPtr>>
      event_dispatcher_;

  // Contains a set of default loader factories for the associated loading
  // context. Used to load a blob, and for network fallback.
  scoped_refptr<ChildURLLoaderFactoryGetter> default_loader_factory_getter_;

  GURL controller_origin_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerSubresourceLoaderFactory);
};

}  // namespace content

#endif  // CONTENT_CHILD_SERVICE_WORKER_SERVICE_WORKER_SUBRESOURCE_LOADER_H_
