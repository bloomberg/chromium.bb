// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// See http://dev.chromium.org/developers/design-documents/multi-process-resource-loading

#ifndef CHROME_COMMON_RESOURCE_RESPONSE_H_
#define CHROME_COMMON_RESOURCE_RESPONSE_H_

#include <string>

#include "base/ref_counted.h"
#include "chrome/common/filter_policy.h"
#include "googleurl/src/gurl.h"
#include "net/url_request/url_request_status.h"
#include "webkit/glue/resource_loader_bridge.h"

// Parameters for a resource response header.
struct ResourceResponseHead
    : webkit_glue::ResourceLoaderBridge::ResponseInfo {
  ResourceResponseHead() : filter_policy(FilterPolicy::DONT_FILTER) {}

  // The response status.
  URLRequestStatus status;

  // Specifies if the resource should be filtered before being displayed
  // (insecure resources can be filtered to keep the page secure).
  FilterPolicy::Type filter_policy;
};

// Parameters for a synchronous resource response.
struct SyncLoadResult : ResourceResponseHead {
  // The final URL after any redirects.
  GURL final_url;

  // The response data.
  std::string data;
};

// Simple wrapper that refcounts ResourceResponseHead.
struct ResourceResponse : public base::RefCounted<ResourceResponse> {
  ResourceResponseHead response_head;

 private:
  friend class base::RefCounted<ResourceResponse>;

  ~ResourceResponse() {}
};

#endif  // CHROME_COMMON_RESOURCE_RESPONSE_H_
