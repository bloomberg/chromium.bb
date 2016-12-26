// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTEXT_REQUEST_HANDLER_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTEXT_REQUEST_HANDLER_H_

#include <stdint.h>

#include "base/macros.h"
#include "content/browser/service_worker/service_worker_request_handler.h"

namespace content {

class ServiceWorkerVersion;

// A request handler derivative used to handle requests from
// service workers.
class CONTENT_EXPORT ServiceWorkerContextRequestHandler
    : public ServiceWorkerRequestHandler {
 public:
  enum class CreateJobStatus {
    UNINITIALIZED,
    CREATED_WRITE_JOB_FOR_REGISTER,
    CREATED_WRITE_JOB_FOR_UPDATE,
    CREATED_READ_JOB,
    NO_PROVIDER,
    NO_VERSION,
    NO_CONTEXT,
    IGNORE_REDIRECT,
    IGNORE_NON_SCRIPT,
    OUT_OF_RESOURCE_IDS,
    IGNORE_UNKNOWN,
    // This is just to distinguish from UNINITIALIZED in case the function that
    // is supposed to set status didn't do so.
    DID_NOT_SET_STATUS
  };

  ServiceWorkerContextRequestHandler(
      base::WeakPtr<ServiceWorkerContextCore> context,
      base::WeakPtr<ServiceWorkerProviderHost> provider_host,
      base::WeakPtr<storage::BlobStorageContext> blob_storage_context,
      ResourceType resource_type);
  ~ServiceWorkerContextRequestHandler() override;

  // Called via custom URLRequestJobFactory.
  net::URLRequestJob* MaybeCreateJob(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate,
      ResourceContext* resource_context) override;

 private:
  net::URLRequestJob* MaybeCreateJobImpl(CreateJobStatus* out_status,
                                         net::URLRequest* request,
                                         net::NetworkDelegate* network_delegate,
                                         ResourceContext* resource_context);
  bool ShouldAddToScriptCache(const GURL& url);
  bool ShouldReadFromScriptCache(const GURL& url, int64_t* resource_id_out);

  scoped_refptr<ServiceWorkerVersion> version_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerContextRequestHandler);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTEXT_REQUEST_HANDLER_H_
