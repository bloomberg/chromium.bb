// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTROLLEE_REQUEST_HANDLER_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTROLLEE_REQUEST_HANDLER_H_

#include <stdint.h>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "content/browser/service_worker/service_worker_request_handler.h"
#include "content/browser/service_worker/service_worker_url_request_job.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/public/common/request_context_frame_type.h"
#include "content/public/common/request_context_type.h"
#include "content/public/common/resource_type.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerResponseType.h"
#include "url/gurl.h"

namespace net {
class NetworkDelegate;
class URLRequest;
}

namespace content {

class ResourceRequestBody;
class ServiceWorkerRegistration;
class ServiceWorkerVersion;

// A request handler derivative used to handle requests from
// controlled documents.
class CONTENT_EXPORT ServiceWorkerControlleeRequestHandler
    : public ServiceWorkerRequestHandler,
      public ServiceWorkerURLRequestJob::Delegate {
 public:
  ServiceWorkerControlleeRequestHandler(
      base::WeakPtr<ServiceWorkerContextCore> context,
      base::WeakPtr<ServiceWorkerProviderHost> provider_host,
      base::WeakPtr<storage::BlobStorageContext> blob_storage_context,
      FetchRequestMode request_mode,
      FetchCredentialsMode credentials_mode,
      FetchRedirectMode redirect_mode,
      ResourceType resource_type,
      RequestContextType request_context_type,
      RequestContextFrameType frame_type,
      scoped_refptr<ResourceRequestBody> body);
  ~ServiceWorkerControlleeRequestHandler() override;

  // Called via custom URLRequestJobFactory.
  net::URLRequestJob* MaybeCreateJob(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate,
      ResourceContext* resource_context) override;

  void GetExtraResponseInfo(ResourceResponseInfo* response_info) const override;

 private:
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerControlleeRequestHandlerTest,
                           ActivateWaitingVersion);
  typedef ServiceWorkerControlleeRequestHandler self;

  // For main resource case.
  void PrepareForMainResource(const net::URLRequest* request);
  void DidLookupRegistrationForMainResource(
      ServiceWorkerStatusCode status,
      const scoped_refptr<ServiceWorkerRegistration>& registration);
  void OnVersionStatusChanged(
      ServiceWorkerRegistration* registration,
      ServiceWorkerVersion* version);

  void DidUpdateRegistration(
      const scoped_refptr<ServiceWorkerRegistration>& original_registration,
      ServiceWorkerStatusCode status,
      const std::string& status_message,
      int64_t registration_id);
  void OnUpdatedVersionStatusChanged(
      const scoped_refptr<ServiceWorkerRegistration>& registration,
      const scoped_refptr<ServiceWorkerVersion>& version);

  // For sub resource case.
  void PrepareForSubResource();

  // ServiceWorkerURLRequestJob::Delegate implementation:

  // Called just before the request is restarted. Makes sure the next request
  // goes over the network.
  void OnPrepareToRestart(base::TimeTicks service_worker_start_time,
                          base::TimeTicks service_worker_ready_time) override;

  // Called when the request's start phase completes. Caches response info for
  // GetExtraResponseInfo.
  void OnStartCompleted(
      bool was_fetched_via_service_worker,
      bool was_fallback_required,
      const GURL& original_url_via_service_worker,
      blink::WebServiceWorkerResponseType response_type_via_service_worker,
      base::TimeTicks worker_start_time,
      base::TimeTicks service_worker_ready_time) override;

  ServiceWorkerVersion* GetServiceWorkerVersion(
      ServiceWorkerMetrics::URLRequestJobResult* result) override;
  bool RequestStillValid(
      ServiceWorkerMetrics::URLRequestJobResult* result) override;
  void MainResourceLoadFailed() override;
  GURL GetRequestingOrigin() override;

  // Sets |job_| to nullptr, and clears all extra response info associated with
  // that job, except for timing information.
  void ClearJob();

  bool is_main_resource_load_;
  base::WeakPtr<ServiceWorkerURLRequestJob> job_;
  FetchRequestMode request_mode_;
  FetchCredentialsMode credentials_mode_;
  FetchRedirectMode redirect_mode_;
  RequestContextType request_context_type_;
  RequestContextFrameType frame_type_;
  scoped_refptr<ResourceRequestBody> body_;
  ResourceContext* resource_context_;
  GURL stripped_url_;
  bool force_update_started_;

  // True if the next time this request is started, the response should be
  // delivered from the network, bypassing the ServiceWorker. Cleared after the
  // next intercept opportunity, for main frame requests.
  bool use_network_;

  // Cached metadata for GetExtraResponseInfo.
  bool was_fetched_via_service_worker_;
  bool was_fallback_required_;
  GURL original_url_via_service_worker_;
  blink::WebServiceWorkerResponseType response_type_via_service_worker_;
  base::TimeTicks service_worker_start_time_;
  base::TimeTicks service_worker_ready_time_;

  base::WeakPtrFactory<ServiceWorkerControlleeRequestHandler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerControlleeRequestHandler);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTROLLEE_REQUEST_HANDLER_H_
