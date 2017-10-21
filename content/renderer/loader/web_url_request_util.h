// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_LOADER_WEB_URL_REQUEST_UTIL_H_
#define CONTENT_RENDERER_LOADER_WEB_URL_REQUEST_UTIL_H_

#include <string>

#include "content/common/content_export.h"
#include "content/public/common/request_context_frame_type.h"
#include "content/public/common/request_context_type.h"
#include "content/public/common/resource_request_body.h"
#include "content/public/common/resource_type.h"
#include "content/public/common/service_worker_modes.h"
#include "net/http/http_request_headers.h"
#include "third_party/WebKit/public/platform/WebMixedContentContextType.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"

namespace blink {
class WebHTTPBody;
}

namespace content {

ResourceType WebURLRequestContextToResourceType(
    blink::WebURLRequest::RequestContext request_context);

CONTENT_EXPORT ResourceType WebURLRequestToResourceType(
    const blink::WebURLRequest& request);

net::HttpRequestHeaders GetWebURLRequestHeaders(
    const blink::WebURLRequest& request);

std::string GetWebURLRequestHeadersAsString(
    const blink::WebURLRequest& request);

int GetLoadFlagsForWebURLRequest(const blink::WebURLRequest& request);

// Takes a ResourceRequestBody and converts into WebHTTPBody.
blink::WebHTTPBody GetWebHTTPBodyForRequestBody(
    const scoped_refptr<ResourceRequestBody>& input);

// Takes a WebHTTPBody and converts into a ResourceRequestBody.
scoped_refptr<ResourceRequestBody> GetRequestBodyForWebHTTPBody(
    const blink::WebHTTPBody& httpBody);

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
std::string GetFetchIntegrityForWebURLRequest(
    const blink::WebURLRequest& request);
RequestContextFrameType GetRequestContextFrameTypeForWebURLRequest(
    const blink::WebURLRequest& request);
RequestContextType GetRequestContextTypeForWebURLRequest(
    const blink::WebURLRequest& request);
blink::WebMixedContentContextType GetMixedContentContextTypeForWebURLRequest(
    const blink::WebURLRequest& request);
ServiceWorkerMode GetServiceWorkerModeForWebURLRequest(
    const blink::WebURLRequest& request);

}  // namespace content

#endif  // CONTENT_RENDERER_LOADER_WEB_URL_REQUEST_UTIL_H_
