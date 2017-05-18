// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_JOB_WRAPPER_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_JOB_WRAPPER_H_

#include "base/macros.h"
#include "content/browser/service_worker/service_worker_url_request_job.h"

namespace content {

// TODO(scottmg): Not yet implemented. See https://crbug.com/715640.
class ServiceWorkerControlleeURLLoader;

// This class is a helper to support having
// ServiceWorkerControlleeRequestHandler work with both URLRequestJobs and
// mojom::URLLoaderFactorys (that is, both with and without
// --enable-network-service). It wraps either a ServiceWorkerURLRequestJob or a
// ServiceWorkerControlleeRequestHandler and forwards to the underlying
// implementation.
class ServiceWorkerURLJobWrapper {
 public:
  // Non-network service case.
  explicit ServiceWorkerURLJobWrapper(
      base::WeakPtr<ServiceWorkerURLRequestJob> url_request_job);

  // With --enable-network-service.
  explicit ServiceWorkerURLJobWrapper(
      ServiceWorkerControlleeURLLoader* url_loader);

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
  base::WeakPtr<ServiceWorkerURLRequestJob> url_request_job_;
  ServiceWorkerControlleeURLLoader* url_loader_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerURLJobWrapper);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_JOB_WRAPPER_H_
