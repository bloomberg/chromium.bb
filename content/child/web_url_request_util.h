// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_WEB_URL_REQUEST_UTIL_H_
#define CONTENT_CHILD_WEB_URL_REQUEST_UTIL_H_

#include <string>

#include "content/common/content_export.h"
#include "content/common/resource_request_body.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/public/common/request_context_frame_type.h"
#include "content/public/common/request_context_type.h"
#include "content/public/common/resource_type.h"

namespace blink {
class WebURL;
class WebURLRequest;
struct WebURLError;
}

namespace content {

CONTENT_EXPORT ResourceType WebURLRequestToResourceType(
    const blink::WebURLRequest& request);

std::string GetWebURLRequestHeaders(const blink::WebURLRequest& request);

int GetLoadFlagsForWebURLRequest(const blink::WebURLRequest& request);

// Takes a WebURLRequest and sets the appropriate information
// in a ResourceRequestBody structure. Returns an empty scoped_refptr
// if the request body is not present.
scoped_refptr<ResourceRequestBody> GetRequestBodyForWebURLRequest(
    const blink::WebURLRequest& request);

// Helper functions to convert enums from the blink type to the content
// type.
FetchRequestMode GetFetchRequestModeForWebURLRequest(
    const blink::WebURLRequest& request);
FetchCredentialsMode GetFetchCredentialsModeForWebURLRequest(
    const blink::WebURLRequest& request);
FetchRedirectMode GetFetchRedirectModeForWebURLRequest(
    const blink::WebURLRequest& request);
RequestContextFrameType GetRequestContextFrameTypeForWebURLRequest(
    const blink::WebURLRequest& request);
RequestContextType GetRequestContextTypeForWebURLRequest(
    const blink::WebURLRequest& request);

// Generates a WebURLError based on |reason|.
blink::WebURLError CreateWebURLError(const blink::WebURL& unreachable_url,
                                     bool stale_copy_in_cache,
                                     int reason);

// Generates a WebURLError based on |reason|.
blink::WebURLError CreateWebURLError(const blink::WebURL& unreachable_url,
                                     bool stale_copy_in_cache,
                                     int reason,
                                     bool was_ignored_by_handler);

}  // namespace content

#endif  // CONTENT_CHILD_WEB_URL_REQUEST_UTIL_H_
