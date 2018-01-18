// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_JOB_WRAPPER_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_JOB_WRAPPER_H_

#include "base/macros.h"
#include "content/browser/loader/url_loader_request_handler.h"
#include "content/browser/service_worker/service_worker_metrics.h"
#include "content/common/content_export.h"

namespace content {

class ServiceWorkerURLRequestJob;
class ServiceWorkerURLLoaderJob;
class ServiceWorkerVersion;

// This class is a helper to support having
// ServiceWorkerControlleeRequestHandler work with both URLRequestJobs and
// network::mojom::URLLoaders (that is, both with and without
// --enable-network-service). It wraps either a ServiceWorkerURLRequestJob or a
// callback for URLLoader and forwards to the underlying implementation.
class ServiceWorkerURLJobWrapper {
 public:
  // A helper used by the ServiceWorkerURLLoaderJob or
  // ServiceWorkerURLRequestJob.
  class CONTENT_EXPORT Delegate {
   public:
    virtual ~Delegate() {}

    // Will be invoked before the request is restarted. The caller
    // can use this opportunity to grab state from the
    // ServiceWorkerURLLoaderJob or ServiceWorkerURLRequestJob to determine
    // how it should behave when the request is restarted.
    virtual void OnPrepareToRestart() = 0;

    // Returns the ServiceWorkerVersion fetch events for this request job should
    // be dispatched to. If no appropriate worker can be determined, returns
    // nullptr and sets |*result| to an appropriate error.
    virtual ServiceWorkerVersion* GetServiceWorkerVersion(
        ServiceWorkerMetrics::URLRequestJobResult* result) = 0;

    // Called after dispatching the fetch event to determine if processing of
    // the request should still continue, or if processing should be aborted.
    // When false is returned, this sets |*result| to an appropriate error.
    virtual bool RequestStillValid(
        ServiceWorkerMetrics::URLRequestJobResult* result);

    // Called to signal that loading failed, and that the resource being loaded
    // was a main resource.
    virtual void MainResourceLoadFailed() {}
  };

  // Non-network service case.
  explicit ServiceWorkerURLJobWrapper(
      base::WeakPtr<ServiceWorkerURLRequestJob> url_request_job);

  // With --enable-network-service.
  explicit ServiceWorkerURLJobWrapper(
      std::unique_ptr<ServiceWorkerURLLoaderJob> url_loader_job);

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
  base::WeakPtr<ServiceWorkerURLRequestJob> url_request_job_;
  std::unique_ptr<ServiceWorkerURLLoaderJob> url_loader_job_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerURLJobWrapper);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_JOB_WRAPPER_H_
