// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/resource_type.h"

#include "base/logging.h"

using blink::WebURLRequest;

namespace content {

// static
ResourceType::Type ResourceType::FromWebURLRequest(
    const WebURLRequest& request) {
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
      return ResourceType::MAIN_FRAME;
    }
    if (request.frameType() == WebURLRequest::FrameTypeNested)
      return ResourceType::SUB_FRAME;
    NOTREACHED();
    return ResourceType::SUB_RESOURCE;
  }

  switch (requestContext) {
    // Favicon
    case WebURLRequest::RequestContextFavicon:
      return ResourceType::FAVICON;

    // Font
    case WebURLRequest::RequestContextFont:
      return ResourceType::FONT_RESOURCE;

    // Image
    case WebURLRequest::RequestContextImage:
      return ResourceType::IMAGE;

    // Media
    case WebURLRequest::RequestContextAudio:
    case WebURLRequest::RequestContextVideo:
      return ResourceType::MEDIA;

    // Object
    case WebURLRequest::RequestContextEmbed:
    case WebURLRequest::RequestContextObject:
      return ResourceType::OBJECT;

    // Ping
    case WebURLRequest::RequestContextBeacon:
    case WebURLRequest::RequestContextCSPReport:
    case WebURLRequest::RequestContextPing:
      return ResourceType::PING;

    // Prefetch
    case WebURLRequest::RequestContextPrefetch:
      return ResourceType::PREFETCH;

    // Script
    case WebURLRequest::RequestContextScript:
      return ResourceType::SCRIPT;

    // Style
    case WebURLRequest::RequestContextXSLT:
    case WebURLRequest::RequestContextStyle:
      return ResourceType::STYLESHEET;

    // Subresource
    case WebURLRequest::RequestContextDownload:
    case WebURLRequest::RequestContextManifest:
    case WebURLRequest::RequestContextSubresource:
    case WebURLRequest::RequestContextPlugin:
      return ResourceType::SUB_RESOURCE;

    // TextTrack
    case WebURLRequest::RequestContextTrack:
      return ResourceType::MEDIA;

    // Workers
    case WebURLRequest::RequestContextServiceWorker:
      return ResourceType::SERVICE_WORKER;
    case WebURLRequest::RequestContextSharedWorker:
      return ResourceType::SHARED_WORKER;
    case WebURLRequest::RequestContextWorker:
      return ResourceType::WORKER;

    // Unspecified
    case WebURLRequest::RequestContextInternal:
    case WebURLRequest::RequestContextUnspecified:
      return ResourceType::SUB_RESOURCE;

    // XHR
    case WebURLRequest::RequestContextEventSource:
    case WebURLRequest::RequestContextFetch:
    case WebURLRequest::RequestContextXMLHttpRequest:
      return ResourceType::XHR;

    // These should be handled by the FrameType checks at the top of the
    // function.
    // Main Frame
    case WebURLRequest::RequestContextForm:
    case WebURLRequest::RequestContextHyperlink:
    case WebURLRequest::RequestContextLocation:
    case WebURLRequest::RequestContextFrame:
    case WebURLRequest::RequestContextIframe:
      NOTREACHED();
      return ResourceType::SUB_RESOURCE;
  }
  NOTREACHED();
  return ResourceType::SUB_RESOURCE;
}

}  // namespace content
