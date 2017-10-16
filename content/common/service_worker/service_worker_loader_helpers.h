// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_SERVICE_WORKER_SERVICE_WORKER_LOADER_HELPERS_H_
#define CONTENT_COMMON_SERVICE_WORKER_SERVICE_WORKER_LOADER_HELPERS_H_

#include "base/optional.h"
#include "content/common/service_worker/service_worker_types.h"
#include "net/url_request/redirect_info.h"

namespace content {

struct ResourceRequest;
struct ResourceResponseHead;

// Helper functions for service worker classes that use URLLoader
//(e.g., ServiceWorkerURLLoaderJob and ServiceWorkerSubresourceLoader).
class ServiceWorkerLoaderHelpers {
 public:
  static std::unique_ptr<ServiceWorkerFetchRequest> CreateFetchRequest(
      const ResourceRequest& request);

  // Populates |out_head->headers| with the given |status_code|, |status_text|,
  // and |headers|.
  static void SaveResponseHeaders(const int status_code,
                                  const std::string& status_text,
                                  const ServiceWorkerHeaderMap& headers,
                                  ResourceResponseHead* out_head);
  // Populates |out_head| (except for headers) with given |response|.
  static void SaveResponseInfo(const ServiceWorkerResponse& response,
                               ResourceResponseHead* out_head);

  // Returns a redirect info if |response_head| is an redirect response.
  // Otherwise returns base::nullopt.
  static base::Optional<net::RedirectInfo> ComputeRedirectInfo(
      const ResourceRequest& original_request,
      const ResourceResponseHead& response_head,
      bool token_binding_negotiated);
};

}  // namespace content

#endif  // CONTENT_COMMON_SERVICE_WORKER_SERVICE_WORKER_LOADER_HELPERS_H_
