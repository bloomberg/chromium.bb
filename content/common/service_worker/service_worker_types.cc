// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/service_worker/service_worker_types.h"

namespace content {

ServiceWorkerFetchRequest::ServiceWorkerFetchRequest() : blob_size(0),
                                                         is_reload(false) {}

ServiceWorkerFetchRequest::ServiceWorkerFetchRequest(
    const GURL& url,
    const std::string& method,
    const ServiceWorkerHeaderMap& headers,
    const GURL& referrer,
    bool is_reload)
    : url(url),
      method(method),
      headers(headers),
      blob_size(0),
      referrer(referrer),
      is_reload(is_reload) {}

ServiceWorkerFetchRequest::~ServiceWorkerFetchRequest() {}

ServiceWorkerResponse::ServiceWorkerResponse() : status_code(0) {}

ServiceWorkerResponse::ServiceWorkerResponse(
    const GURL& url,
    int status_code,
    const std::string& status_text,
    const ServiceWorkerHeaderMap& headers,
    const std::string& blob_uuid)
    : url(url),
      status_code(status_code),
      status_text(status_text),
      headers(headers),
      blob_uuid(blob_uuid) {}

ServiceWorkerResponse::~ServiceWorkerResponse() {}

ServiceWorkerCacheQueryParams::ServiceWorkerCacheQueryParams()
    : ignore_search(false),
      ignore_method(false),
      ignore_vary(false),
      prefix_match(false) {}

ServiceWorkerBatchOperation::ServiceWorkerBatchOperation() {}

ServiceWorkerObjectInfo::ServiceWorkerObjectInfo()
    : handle_id(kInvalidServiceWorkerHandleId),
      state(blink::WebServiceWorkerStateUnknown) {}

ServiceWorkerRegistrationObjectInfo::ServiceWorkerRegistrationObjectInfo()
    : handle_id(kInvalidServiceWorkerRegistrationHandleId) {}

}  // namespace content
