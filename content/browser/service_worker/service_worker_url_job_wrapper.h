// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_JOB_WRAPPER_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_JOB_WRAPPER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/loader/url_loader_request_handler.h"
#include "content/browser/service_worker/service_worker_fetch_dispatcher.h"
#include "content/browser/service_worker/service_worker_response_type.h"
#include "content/browser/service_worker/service_worker_url_request_job.h"
#include "storage/browser/blob/blob_reader.h"

namespace content {

// This class is a helper to support having
// ServiceWorkerControlleeRequestHandler work with both URLRequestJobs and
// mojom::URLLoaderFactorys (that is, both with and without
// --enable-network-service). It wraps either a ServiceWorkerURLRequestJob or a
// callback for URLLoaderFactory and forwards to the underlying implementation.
class ServiceWorkerURLJobWrapper {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}

    virtual ServiceWorkerVersion* GetServiceWorkerVersion(
        ServiceWorkerMetrics::URLRequestJobResult* result) = 0;
  };

  // Non-network service case.
  explicit ServiceWorkerURLJobWrapper(
      base::WeakPtr<ServiceWorkerURLRequestJob> url_request_job);

  // With --enable-network-service.
  // TODO(kinuko): Implement this as a separate job class rather
  // than in a wrapper.
  ServiceWorkerURLJobWrapper(
      LoaderFactoryCallback loader_factory_callback,
      Delegate* delegate,
      const ResourceRequest& resource_request,
      base::WeakPtr<storage::BlobStorageContext> blob_storage_context);

  ~ServiceWorkerURLJobWrapper();

  // Sets the response type.
  void FallbackToNetwork();
  void FallbackToNetworkOrRenderer();
  void ForwardToServiceWorker();

  // Returns true if this job should not be handled by a service worker, but
  // instead should fallback to the network.
  bool ShouldFallbackToNetwork();

  // Tells the job to abort with a start error. Currently this is only called
  // because the controller was lost. This function could be made more generic
  // if needed later.
  void FailDueToLostController();

  // Determines from the ResourceRequestInfo (or similar) the type of page
  // transition used (for metrics purposes).
  ui::PageTransition GetPageTransition();

  // Determines the number of redirects used to handle the job (for metrics
  // purposes).
  size_t GetURLChainSize() const;

  // Returns true if the underlying job has been canceled or destroyed.
  bool WasCanceled() const;

 private:
  enum class JobType { kURLRequest, kURLLoader };

  // Used only for URLLoader case.
  // For FORWARD_TO_SERVICE_WORKER case.
  class Factory;
  void StartRequest();

  void DidPrepareFetchEvent(scoped_refptr<ServiceWorkerVersion> version);
  void DidDispatchFetchEvent(
      ServiceWorkerStatusCode status,
      ServiceWorkerFetchEventResult fetch_result,
      const ServiceWorkerResponse& response,
      blink::mojom::ServiceWorkerStreamHandlePtr body_as_stream,
      const scoped_refptr<ServiceWorkerVersion>& version);

  std::unique_ptr<ServiceWorkerFetchRequest> CreateFetchRequest(
      const ResourceRequest& request);

  void AfterRead(scoped_refptr<net::IOBuffer> buffer, int bytes);

  JobType job_type_;

  ServiceWorkerResponseType response_type_ = NOT_DETERMINED;
  LoaderFactoryCallback loader_factory_callback_;

  base::WeakPtr<ServiceWorkerURLRequestJob> url_request_job_;

  Delegate* delegate_;
  std::unique_ptr<Factory> factory_;
  ResourceRequest resource_request_;
  base::WeakPtr<storage::BlobStorageContext> blob_storage_context_;
  std::unique_ptr<ServiceWorkerFetchDispatcher> fetch_dispatcher_;

  base::WeakPtrFactory<ServiceWorkerURLJobWrapper> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerURLJobWrapper);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_JOB_WRAPPER_H_
