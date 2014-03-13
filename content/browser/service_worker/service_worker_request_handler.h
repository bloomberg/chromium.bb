// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_REQUEST_HANDLER_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_REQUEST_HANDLER_H_

#include "base/basictypes.h"

#include "base/memory/weak_ptr.h"
#include "base/supports_user_data.h"
#include "content/common/content_export.h"
#include "content/common/service_worker/service_worker_status_code.h"
#include "net/url_request/url_request_job_factory.h"
#include "webkit/common/resource_type.h"

namespace net {
class NetworkDelegate;
class URLRequest;
}

namespace content {

class ServiceWorkerContextCore;
class ServiceWorkerContextWrapper;
class ServiceWorkerProviderHost;
class ServiceWorkerRegistration;
class ServiceWorkerURLRequestJob;

// Created one per URLRequest and attached to each request.
class CONTENT_EXPORT ServiceWorkerRequestHandler
    : public base::SupportsUserData::Data {
 public:
  // Attaches a newly created handler if the given |request| needs to
  // be handled by ServiceWorker.
  // TODO(kinuko): While utilizing UserData to attach data to URLRequest
  // has some precedence, it might be better to attach this handler in a more
  // explicit way within content layer, e.g. have ResourceRequestInfoImpl
  // own it.
  static void InitializeHandler(
      net::URLRequest* request,
      ServiceWorkerContextWrapper* context_wrapper,
      int process_id,
      int provider_id,
      ResourceType::Type resource_type);

  // Returns the handler attached to |request|. This may return NULL
  // if no handler is attached.
  static ServiceWorkerRequestHandler* GetHandler(
      net::URLRequest* request);

  // Creates a protocol interceptor for ServiceWorker.
  static scoped_ptr<net::URLRequestJobFactory::ProtocolHandler>
      CreateInterceptor();

  virtual ~ServiceWorkerRequestHandler();

  // Called via custom URLRequestJobFactory.
  net::URLRequestJob* MaybeCreateJob(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate);

 private:
  typedef ServiceWorkerRequestHandler self;

  ServiceWorkerRequestHandler(
      base::WeakPtr<ServiceWorkerContextCore> context,
      base::WeakPtr<ServiceWorkerProviderHost> provider_host,
      ResourceType::Type resource_type);

  // For main resource case.
  void PrepareForMainResource(const GURL& url);
  void DidLookupRegistrationForMainResource(
      ServiceWorkerStatusCode status,
      const scoped_refptr<ServiceWorkerRegistration>& registration);

  // For sub resource case.
  void PrepareForSubResource();

  base::WeakPtr<ServiceWorkerContextCore> context_;
  base::WeakPtr<ServiceWorkerProviderHost> provider_host_;
  ResourceType::Type resource_type_;
  scoped_refptr<ServiceWorkerURLRequestJob> job_;

  base::WeakPtrFactory<ServiceWorkerRequestHandler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerRequestHandler);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_REQUEST_HANDLER_H_
