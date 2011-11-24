// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// See http://dev.chromium.org/developers/design-documents/multi-process-resource-loading

#ifndef CONTENT_PUBLIC_COMMON_RESOURCE_RESPONSE_H_
#define CONTENT_PUBLIC_COMMON_RESOURCE_RESPONSE_H_
#pragma once

#include <string>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "googleurl/src/gurl.h"
#include "net/url_request/url_request_status.h"
#include "webkit/glue/resource_loader_bridge.h"

namespace content {

// Parameters for a resource response header.
struct ResourceResponseHead : webkit_glue::ResourceResponseInfo {
  // The response status.
  net::URLRequestStatus status;
};

// Parameters for a synchronous resource response.
struct SyncLoadResult : ResourceResponseHead {
  // The final URL after any redirects.
  GURL final_url;

  // The response data.
  std::string data;
};

// Simple wrapper that refcounts ResourceResponseHead.
struct CONTENT_EXPORT ResourceResponse
    : public NON_EXPORTED_BASE(ResourceResponseHead),
      public base::RefCounted<ResourceResponse> {

 private:
  friend class base::RefCounted<ResourceResponse>;

  ~ResourceResponse() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_RESOURCE_RESPONSE_H_
