// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTROLLEE_REQUEST_HANDLER_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTROLLEE_REQUEST_HANDLER_H_

#include "base/gtest_prod_util.h"
#include "content/browser/service_worker/service_worker_request_handler.h"

namespace net {
class NetworkDelegate;
class URLRequest;
}

namespace content {

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
      base::WeakPtr<webkit_blob::BlobStorageContext> blob_storage_context,
      ResourceType resource_type);
  virtual ~ServiceWorkerControlleeRequestHandler();

  // Called via custom URLRequestJobFactory.
  virtual net::URLRequestJob* MaybeCreateJob(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) OVERRIDE;

  virtual void GetExtraResponseInfo(
      bool* was_fetched_via_service_worker,
      GURL* original_url_via_service_worker) const OVERRIDE;

 private:
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerControlleeRequestHandlerTest,
                           ActivateWaitingVersion);
  typedef ServiceWorkerControlleeRequestHandler self;

  // For main resource case.
  void PrepareForMainResource(const GURL& url);
  void DidLookupRegistrationForMainResource(
      ServiceWorkerStatusCode status,
      const scoped_refptr<ServiceWorkerRegistration>& registration);
  void OnVersionStatusChanged(
      ServiceWorkerRegistration* registration,
      ServiceWorkerVersion* version);

  // For sub resource case.
  void PrepareForSubResource();

  bool is_main_resource_load_;
  scoped_refptr<ServiceWorkerURLRequestJob> job_;
  base::WeakPtrFactory<ServiceWorkerControlleeRequestHandler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerControlleeRequestHandler);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTROLLEE_REQUEST_HANDLER_H_
