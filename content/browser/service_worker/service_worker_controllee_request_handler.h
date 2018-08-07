// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTROLLEE_REQUEST_HANDLER_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTROLLEE_REQUEST_HANDLER_H_

#include <stdint.h>
#include <memory>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "content/browser/service_worker/service_worker_navigation_loader.h"
#include "content/browser/service_worker/service_worker_request_handler.h"
#include "content/browser/service_worker/service_worker_url_job_wrapper.h"
#include "content/browser/service_worker/service_worker_url_request_job.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/public/common/request_context_type.h"
#include "content/public/common/resource_type.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/public/mojom/request_context_frame_type.mojom.h"
#include "url/gurl.h"

namespace net {
class NetworkDelegate;
class URLRequest;
}

namespace network {
class ResourceRequestBody;
}

namespace content {

class ServiceWorkerRegistration;
class ServiceWorkerVersion;

// A request handler derivative used to handle requests for,
// and requests from, controlled documents and shared workers.
//
// Note that in IsServicificationEnabled cases this is used only for
// main resource fetch during navigation or shared worker creation.
class CONTENT_EXPORT ServiceWorkerControlleeRequestHandler
    : public ServiceWorkerRequestHandler,
      public ServiceWorkerURLJobWrapper::Delegate {
 public:
  ServiceWorkerControlleeRequestHandler(
      base::WeakPtr<ServiceWorkerContextCore> context,
      base::WeakPtr<ServiceWorkerProviderHost> provider_host,
      base::WeakPtr<storage::BlobStorageContext> blob_storage_context,
      network::mojom::FetchRequestMode request_mode,
      network::mojom::FetchCredentialsMode credentials_mode,
      network::mojom::FetchRedirectMode redirect_mode,
      const std::string& integrity,
      bool keepalive,
      ResourceType resource_type,
      RequestContextType request_context_type,
      network::mojom::RequestContextFrameType frame_type,
      scoped_refptr<network::ResourceRequestBody> body);
  ~ServiceWorkerControlleeRequestHandler() override;

  // Non-S13nServiceWorker:
  // Called via custom URLRequestJobFactory.
  // Returning a nullptr indicates that the request is not handled by
  // this handler.
  // This could get called multiple times during the lifetime.
  net::URLRequestJob* MaybeCreateJob(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate,
      ResourceContext* resource_context) override;

  // S13nServiceWorker:
  // MaybeCreateLoader replaces MaybeCreateJob() when S13nServiceWorker is
  // enabled.
  // This could get called multiple times during the lifetime in redirect
  // cases. (In fallback-to-network cases we basically forward the request
  // to the request to the next request handler)
  // NavigationLoaderInterceptor overrides:
  void MaybeCreateLoader(const network::ResourceRequest& request,
                         ResourceContext* resource_context,
                         LoaderCallback callback) override;
  // Returns params with the ControllerServiceWorkerPtr if we have found
  // a matching controller service worker for the |request| that is given
  // to MaybeCreateLoader(). Otherwise this returns base::nullopt.
  base::Optional<SubresourceLoaderParams> MaybeCreateSubresourceLoaderParams()
      override;

  // Exposed for testing.
  ServiceWorkerURLJobWrapper* url_job() const { return url_job_.get(); }

 private:
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerControlleeRequestHandlerTest,
                           ActivateWaitingVersion);
  class ScopedDisallowSetControllerRegistration;
  class MainResourceRequestTracker;

  // For main resource case.
  void PrepareForMainResource(const GURL& url, const GURL& site_for_cookies);
  void DidLookupRegistrationForMainResource(
      std::unique_ptr<ScopedDisallowSetControllerRegistration>
          disallow_controller,
      blink::ServiceWorkerStatusCode status,
      scoped_refptr<ServiceWorkerRegistration> registration);
  void ContinueWithInScopeMainResourceRequest(
      scoped_refptr<ServiceWorkerRegistration> registration,
      scoped_refptr<ServiceWorkerVersion> version,
      std::unique_ptr<ScopedDisallowSetControllerRegistration>
          disallow_controller);

  // For forced update.
  void DidUpdateRegistration(
      scoped_refptr<ServiceWorkerRegistration> original_registration,
      std::unique_ptr<ScopedDisallowSetControllerRegistration>
          disallow_controller,
      blink::ServiceWorkerStatusCode status,
      const std::string& status_message,
      int64_t registration_id);
  void OnUpdatedVersionStatusChanged(
      scoped_refptr<ServiceWorkerRegistration> registration,
      scoped_refptr<ServiceWorkerVersion> version,
      std::unique_ptr<ScopedDisallowSetControllerRegistration>
          disallow_controller);

  // For sub resource case.
  void PrepareForSubResource();

  // ServiceWorkerURLJobWrapper::Delegate implementation:
  void OnPrepareToRestart() override;
  ServiceWorkerVersion* GetServiceWorkerVersion(
      ServiceWorkerMetrics::URLRequestJobResult* result) override;
  bool RequestStillValid(
      ServiceWorkerMetrics::URLRequestJobResult* result) override;
  void MainResourceLoadFailed() override;
  void ReportDestination(ServiceWorkerMetrics::MainResourceRequestDestination
                             destination) override;
  void WillDispatchFetchEventForMainResource() override;

  // Sets |job_| to nullptr, and clears all extra response info associated with
  // that job, except for timing information.
  void ClearJob();

  bool JobWasCanceled() const;

  // Schedules a service worker update to occur shortly after the page and its
  // initial subresources load, if this handler was for a navigation.
  void MaybeScheduleUpdate();

  const ResourceType resource_type_;
  const bool is_main_resource_load_;
  std::unique_ptr<ServiceWorkerURLJobWrapper> url_job_;
  network::mojom::FetchRequestMode request_mode_;
  network::mojom::FetchCredentialsMode credentials_mode_;
  network::mojom::FetchRedirectMode redirect_mode_;
  std::string integrity_;
  const bool keepalive_;
  RequestContextType request_context_type_;
  network::mojom::RequestContextFrameType frame_type_;
  scoped_refptr<network::ResourceRequestBody> body_;
  ResourceContext* resource_context_;
  GURL stripped_url_;
  bool force_update_started_;

  // True if the next time this request is started, the response should be
  // delivered from the network, bypassing the ServiceWorker. Cleared after the
  // next intercept opportunity, for main frame requests.
  bool use_network_;

  std::unique_ptr<MainResourceRequestTracker> tracker_;

  base::WeakPtrFactory<ServiceWorkerControlleeRequestHandler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerControlleeRequestHandler);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTROLLEE_REQUEST_HANDLER_H_
