// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// See http://dev.chromium.org/developers/design-documents/multi-process-resource-loading

#ifndef CHROME_COMMON_RESOURCE_RESPONSE_H_
#define CHROME_COMMON_RESOURCE_RESPONSE_H_
#pragma once

#include <string>

#include "base/ref_counted.h"
#include "googleurl/src/gurl.h"
#include "net/url_request/url_request_status.h"
#include "webkit/glue/resource_loader_bridge.h"

// Parameters for a resource response header.
struct ResourceResponseHead
    : webkit_glue::ResourceLoaderBridge::ResponseInfo {
  ResourceResponseHead();
  ~ResourceResponseHead();

  // The response status.
  URLRequestStatus status;

  // Whether we should apply a filter to this resource that replaces
  // localization templates with the appropriate localized strings.  This is set
  // for CSS resources used by extensions.
  bool replace_extension_localization_templates;
};

// Parameters for a synchronous resource response.
struct SyncLoadResult : ResourceResponseHead {
  SyncLoadResult();
  ~SyncLoadResult();

  // The final URL after any redirects.
  GURL final_url;

  // The response data.
  std::string data;
};

// Simple wrapper that refcounts ResourceResponseHead.
struct ResourceResponse : public base::RefCounted<ResourceResponse> {
  ResourceResponseHead response_head;

  ResourceResponse();
 private:
  friend class base::RefCounted<ResourceResponse>;

  virtual ~ResourceResponse();
};

#endif  // CHROME_COMMON_RESOURCE_RESPONSE_H_
