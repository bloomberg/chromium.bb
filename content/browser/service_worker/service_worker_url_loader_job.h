// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_URL_LOADER_JOB_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_URL_LOADER_JOB_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/loader/url_loader_request_handler.h"
#include "content/browser/service_worker/service_worker_metrics.h"
#include "content/browser/service_worker/service_worker_response_type.h"
#include "content/browser/service_worker/service_worker_url_job_wrapper.h"
#include "content/common/service_worker/service_worker_status_code.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/common/url_loader.mojom.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/service_worker_stream_handle.mojom.h"

namespace net {
class IOBuffer;
struct RedirectInfo;
}

namespace storage {
class BlobStorageContext;
}

namespace content {

class ServiceWorkerFetchDispatcher;
struct ServiceWorkerResponse;
class ServiceWorkerVersion;

// ServiceWorkerURLLoaderJob works similar to ServiceWorkerURLRequestJob
// but with mojom::URLLoader instead of URLRequest, and used only when
// --enable-network-service and PlzNavigate is enabled.
// This also works as a URLLoaderClient for BlobURLLoader while reading
// the blob content returned by SW.
class ServiceWorkerURLLoaderJob : public mojom::URLLoader,
                                  public mojom::URLLoaderClient {
 public:
  using Delegate = ServiceWorkerURLJobWrapper::Delegate;

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
  // 5. Then StartResponse() will be called with mojom::URLLoaderClientPtr
  //    that is connected to NavigationURLLoaderNetworkService (for resource
  //    loading for navigation).  This job may set up BlobURLLoader and
  //    act as URLLoaderClient for the BlobURLLoader if the response includes
  //    blob uuid as a response body, or may forward the stream data pipe to
  //    the NavigationURLLoader if response body was sent as a stream.
  ServiceWorkerURLLoaderJob(
      LoaderCallback loader_callback,
      Delegate* delegate,
      const ResourceRequest& resource_request,
      base::WeakPtr<storage::BlobStorageContext> blob_storage_context);

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

 private:
  // For FORWARD_TO_SERVICE_WORKER case.
  void StartRequest();
  void DidPrepareFetchEvent(scoped_refptr<ServiceWorkerVersion> version);
  void DidDispatchFetchEvent(
      ServiceWorkerStatusCode status,
      ServiceWorkerFetchEventResult fetch_result,
      const ServiceWorkerResponse& response,
      blink::mojom::ServiceWorkerStreamHandlePtr body_as_stream,
      const scoped_refptr<ServiceWorkerVersion>& version);

  void StartResponse(const ServiceWorkerResponse& response,
                     blink::mojom::ServiceWorkerStreamHandlePtr body_as_stream,
                     mojom::URLLoaderRequest request,
                     mojom::URLLoaderClientPtr client);
  void AfterRead(scoped_refptr<net::IOBuffer> buffer, int bytes);

  std::unique_ptr<ServiceWorkerFetchRequest> CreateFetchRequest(
      const ResourceRequest& request);

  // Populates |response_info_| (except for headers) with given |response|.
  void SaveResponseInfo(const ServiceWorkerResponse& response);
  // Generates and populates |response.headers|.
  void SaveResponseHeaders(int status_code,
                           const std::string& status_text,
                           const ServiceWorkerHeaderMap& headers);
  // Calls url_loader_client_->OnReceiveResopnse() with |response_head_|.
  // Expected to be called afer saving response info/headers.
  void CommitResponseHeaders();
  // Calls url_loader_client_->OnComplete(). Expected to be called after
  // CommitResponseHeaders (i.e. status_ == kSentHeader).
  void CommitCompleted(int error_code);
  // Calls CommitResponseHeaders() if we haven't sent headers yet,
  // and CommitCompleted() with error code.
  void DeliverErrorResponse();

  // mojom::URLLoader:
  void FollowRedirect() override;
  void SetPriority(net::RequestPriority priority,
                   int32_t intra_priority_value) override;

  // mojom::URLLoaderClient for Blob response reading (used only when
  // the SW response had valid blob UUID):
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

  ServiceWorkerResponseType response_type_ = NOT_DETERMINED;
  LoaderCallback loader_callback_;

  Delegate* delegate_;
  ResourceRequest resource_request_;
  base::WeakPtr<storage::BlobStorageContext> blob_storage_context_;
  std::unique_ptr<ServiceWorkerFetchDispatcher> fetch_dispatcher_;

  bool did_navigation_preload_ = false;
  ResourceResponseHead response_head_;
  base::Optional<net::SSLInfo> ssl_info_;

  // URLLoaderClient binding for loading a blob.
  mojo::Binding<mojom::URLLoaderClient> blob_client_binding_;

  // Pointer to the URLLoaderClient (i.e. NavigationURLLoader).
  mojom::URLLoaderClientPtr url_loader_client_;
  mojo::Binding<mojom::URLLoader> binding_;

  enum class Status {
    kNotStarted,
    kStarted,
    kSentHeader,
    kCompleted,
    kCancelled
  };
  Status status_ = Status::kNotStarted;

  base::WeakPtrFactory<ServiceWorkerURLLoaderJob> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerURLLoaderJob);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_URL_LOADER_JOB_H_
