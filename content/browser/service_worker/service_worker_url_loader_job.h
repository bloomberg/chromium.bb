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
#include "content/common/service_worker/service_worker_status_code.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/common/url_loader.mojom.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/service_worker_stream_handle.mojom.h"

namespace net {
class IOBuffer;
}

namespace storage {
class BlobReader;
class BlobStorageContext;
}

namespace content {

class ServiceWorkerFetchDispatcher;
class ServiceWorkerVersion;
struct ServiceWorkerResponse;

// ServiceWorkerURLLoaderJob works similar to ServiceWorkerURLRequestJob
// but with mojom::URLLoader instead of URLRequest, and used only when
// --enable-network-service and PlzNavigate is enabled.
class ServiceWorkerURLLoaderJob : public mojom::URLLoader {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}

    virtual ServiceWorkerVersion* GetServiceWorkerVersion(
        ServiceWorkerMetrics::URLRequestJobResult* result) = 0;
  };

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

  // mojom::URLLoader:
  void FollowRedirect() override;
  void SetPriority(net::RequestPriority priority,
                   int32_t intra_priority_value) override;

  ServiceWorkerResponseType response_type_ = NOT_DETERMINED;
  LoaderCallback loader_callback_;

  Delegate* delegate_;
  ResourceRequest resource_request_;
  base::WeakPtr<storage::BlobStorageContext> blob_storage_context_;
  std::unique_ptr<ServiceWorkerFetchDispatcher> fetch_dispatcher_;

  std::unique_ptr<storage::BlobReader> blob_reader_;
  mojo::DataPipe data_pipe_;

  mojom::URLLoaderClientPtr url_loader_client_;
  mojo::Binding<mojom::URLLoader> binding_;

  bool canceled_ = false;

  base::WeakPtrFactory<ServiceWorkerURLLoaderJob> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerURLLoaderJob);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_URL_LOADER_JOB_H_
