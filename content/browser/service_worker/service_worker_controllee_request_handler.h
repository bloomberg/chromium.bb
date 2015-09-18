// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTROLLEE_REQUEST_HANDLER_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTROLLEE_REQUEST_HANDLER_H_

#include "base/gtest_prod_util.h"
#include "content/browser/service_worker/service_worker_request_handler.h"
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
class ServiceWorkerURLRequestJob;
class ServiceWorkerVersion;

// A request handler derivative used to handle requests from
// controlled documents.
class CONTENT_EXPORT ServiceWorkerControlleeRequestHandler
    : public ServiceWorkerRequestHandler {
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
      int64 registration_id);
  void OnUpdatedVersionStatusChanged(
      const scoped_refptr<ServiceWorkerRegistration>& registration,
      const scoped_refptr<ServiceWorkerVersion>& version);

  // For sub resource case.
  void PrepareForSubResource();

  void FallbackToNetwork();

  bool is_main_resource_load_;
  scoped_refptr<ServiceWorkerURLRequestJob> job_;
  FetchRequestMode request_mode_;
  FetchCredentialsMode credentials_mode_;
  FetchRedirectMode redirect_mode_;
  RequestContextType request_context_type_;
  RequestContextFrameType frame_type_;
  scoped_refptr<ResourceRequestBody> body_;
  ResourceContext* resource_context_;
  GURL stripped_url_;
  base::TimeTicks worker_start_time_;
  base::TimeTicks worker_ready_time_;
  bool skip_service_worker_;
  bool force_update_started_;
  base::WeakPtrFactory<ServiceWorkerControlleeRequestHandler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerControlleeRequestHandler);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTROLLEE_REQUEST_HANDLER_H_
