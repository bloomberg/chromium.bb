// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_PREVIEWS_TYPE_H_
#define CONTENT_PUBLIC_COMMON_PREVIEWS_TYPE_H_

#include "content/common/content_export.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"

#define STATIC_ASSERT_PREVIEWS_ENUM(a, b)                   \
  static_assert(static_cast<int>(a) == static_cast<int>(b), \
                "mismatching enums: " #a)

namespace content {

// Chrome Proxy Previews header and directives.
const char kChromeProxyContentTransformHeader[] =
    "chrome-proxy-content-transform";
const char kChromeProxyEmptyImageDirective[] = "empty-image";
const char kChromeProxyLitePageDirective[] = "lite-page";

typedef int PreviewsState;

// The Previews types which determines whether to request a Preview version of
// the resource. Previews are optimizations that change the format and
// content of web pages to improve data savings and / or performance. This enum
// determines which Previews types to request.
enum PreviewsTypes {
  PREVIEWS_UNSPECIFIED = 0,        // Let the browser process decide whether or
                                   // not to request Preview types.
  SERVER_LOFI_ON = 1 << 0,         // Request a Lo-Fi version of the resource
                                   // from the server.
  CLIENT_LOFI_ON = 1 << 1,         // Request a Lo-Fi version of the resource
                                   // from the client.
  SERVER_LITE_PAGE_ON = 1 << 2,    // Request a Lite Page version of the
                                   // resource from the server.
  PREVIEWS_NO_TRANSFORM = 1 << 3,  // Explicitly forbid Previews
                                   // transformations.
  PREVIEWS_OFF = 1 << 4,           // Request a normal (non-Preview) version of
                                   // the resource. Server transformations may
                                   // still happen if the page is heavy.
  PREVIEWS_STATE_LAST = PREVIEWS_OFF
};

// Ensure that content::PreviewsState and blink::WebURLRequest::PreviewsState
// are kept in sync.
STATIC_ASSERT_PREVIEWS_ENUM(PREVIEWS_UNSPECIFIED,
                            blink::WebURLRequest::PreviewsUnspecified);
STATIC_ASSERT_PREVIEWS_ENUM(SERVER_LOFI_ON, blink::WebURLRequest::ServerLoFiOn);
STATIC_ASSERT_PREVIEWS_ENUM(CLIENT_LOFI_ON, blink::WebURLRequest::ClientLoFiOn);
STATIC_ASSERT_PREVIEWS_ENUM(SERVER_LITE_PAGE_ON,
                            blink::WebURLRequest::ServerLitePageOn);
STATIC_ASSERT_PREVIEWS_ENUM(PREVIEWS_NO_TRANSFORM,
                            blink::WebURLRequest::PreviewsNoTransform);
STATIC_ASSERT_PREVIEWS_ENUM(PREVIEWS_OFF, blink::WebURLRequest::PreviewsOff);
STATIC_ASSERT_PREVIEWS_ENUM(PREVIEWS_STATE_LAST,
                            blink::WebURLRequest::PreviewsStateLast);

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_PREVIEWS_TYPE_H_
