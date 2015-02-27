// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_WEB_URL_REQUEST_UTIL_H_
#define CONTENT_CHILD_WEB_URL_REQUEST_UTIL_H_

#include <string>

#include "content/common/content_export.h"
#include "content/common/resource_request_body.h"
#include "content/public/common/resource_type.h"

namespace blink {
class WebURLRequest;
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

}  // namespace content

#endif  // CONTENT_CHILD_WEB_URL_REQUEST_UTIL_H_
