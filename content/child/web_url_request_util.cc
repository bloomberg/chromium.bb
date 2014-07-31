// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/web_url_request_util.h"

#include "base/logging.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"

using blink::WebURLRequest;

namespace content {

ResourceType WebURLRequestToResourceType(const WebURLRequest& request) {
  WebURLRequest::RequestContext requestContext = request.requestContext();
  if (request.frameType() != WebURLRequest::FrameTypeNone) {
    DCHECK(requestContext == WebURLRequest::RequestContextForm ||
           requestContext == WebURLRequest::RequestContextFrame ||
           requestContext == WebURLRequest::RequestContextHyperlink ||
           requestContext == WebURLRequest::RequestContextIframe ||
           requestContext == WebURLRequest::RequestContextInternal ||
           requestContext == WebURLRequest::RequestContextLocation);
    if (request.frameType() == WebURLRequest::FrameTypeTopLevel ||
        request.frameType() == WebURLRequest::FrameTypeAuxiliary) {
      return RESOURCE_TYPE_MAIN_FRAME;
    }
    if (request.frameType() == WebURLRequest::FrameTypeNested)
      return RESOURCE_TYPE_SUB_FRAME;
    NOTREACHED();
    return RESOURCE_TYPE_SUB_RESOURCE;
  }

  switch (requestContext) {
    // Favicon
    case WebURLRequest::RequestContextFavicon:
      return RESOURCE_TYPE_FAVICON;

    // Font
    case WebURLRequest::RequestContextFont:
      return RESOURCE_TYPE_FONT_RESOURCE;

    // Image
    case WebURLRequest::RequestContextImage:
      return RESOURCE_TYPE_IMAGE;

    // Media
    case WebURLRequest::RequestContextAudio:
    case WebURLRequest::RequestContextVideo:
      return RESOURCE_TYPE_MEDIA;

    // Object
    case WebURLRequest::RequestContextEmbed:
    case WebURLRequest::RequestContextObject:
      return RESOURCE_TYPE_OBJECT;

    // Ping
    case WebURLRequest::RequestContextBeacon:
    case WebURLRequest::RequestContextCSPReport:
    case WebURLRequest::RequestContextPing:
      return RESOURCE_TYPE_PING;

    // Prefetch
    case WebURLRequest::RequestContextPrefetch:
      return RESOURCE_TYPE_PREFETCH;

    // Script
    case WebURLRequest::RequestContextScript:
      return RESOURCE_TYPE_SCRIPT;

    // Style
    case WebURLRequest::RequestContextXSLT:
    case WebURLRequest::RequestContextStyle:
      return RESOURCE_TYPE_STYLESHEET;

    // Subresource
    case WebURLRequest::RequestContextDownload:
    case WebURLRequest::RequestContextManifest:
    case WebURLRequest::RequestContextSubresource:
    case WebURLRequest::RequestContextPlugin:
      return RESOURCE_TYPE_SUB_RESOURCE;

    // TextTrack
    case WebURLRequest::RequestContextTrack:
      return RESOURCE_TYPE_MEDIA;

    // Workers
    case WebURLRequest::RequestContextServiceWorker:
      return RESOURCE_TYPE_SERVICE_WORKER;
    case WebURLRequest::RequestContextSharedWorker:
      return RESOURCE_TYPE_SHARED_WORKER;
    case WebURLRequest::RequestContextWorker:
      return RESOURCE_TYPE_WORKER;

    // Unspecified
    case WebURLRequest::RequestContextInternal:
    case WebURLRequest::RequestContextUnspecified:
      return RESOURCE_TYPE_SUB_RESOURCE;

    // XHR
    case WebURLRequest::RequestContextEventSource:
    case WebURLRequest::RequestContextFetch:
    case WebURLRequest::RequestContextXMLHttpRequest:
      return RESOURCE_TYPE_XHR;

    // These should be handled by the FrameType checks at the top of the
    // function.
    case WebURLRequest::RequestContextForm:
    case WebURLRequest::RequestContextHyperlink:
    case WebURLRequest::RequestContextLocation:
    case WebURLRequest::RequestContextFrame:
    case WebURLRequest::RequestContextIframe:
      NOTREACHED();
      return RESOURCE_TYPE_SUB_RESOURCE;

    default:
      NOTREACHED();
      return RESOURCE_TYPE_SUB_RESOURCE;
  }
}

}  // namespace content
