// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_SPDY_HTTP_UTILS_H_
#define NET_SPDY_SPDY_HTTP_UTILS_H_

#include "net/base/net_export.h"
#include "net/base/request_priority.h"
#include "net/third_party/quiche/src/spdy/core/spdy_framer.h"
#include "net/third_party/quiche/src/spdy/core/spdy_header_block.h"
#include "net/third_party/quiche/src/spdy/core/spdy_protocol.h"
#include "url/gurl.h"

namespace net {

class HttpResponseInfo;
struct HttpRequestInfo;
class HttpRequestHeaders;

// Convert a spdy::SpdyHeaderBlock into an HttpResponseInfo.
// |headers| input parameter with the spdy::SpdyHeaderBlock.
// |response| output parameter for the HttpResponseInfo.
// Returns true if successfully converted.  False if the spdy::SpdyHeaderBlock
// is incomplete (e.g. missing 'status' or 'version').
NET_EXPORT bool SpdyHeadersToHttpResponse(const spdy::SpdyHeaderBlock& headers,
                                          HttpResponseInfo* response);

// Create a spdy::SpdyHeaderBlock from HttpRequestInfo and HttpRequestHeaders.
NET_EXPORT void CreateSpdyHeadersFromHttpRequest(
    const HttpRequestInfo& info,
    const HttpRequestHeaders& request_headers,
    spdy::SpdyHeaderBlock* headers);

// Create a spdy::SpdyHeaderBlock from HttpRequestInfo and HttpRequestHeaders
// for a WebSockets over HTTP/2 request.
NET_EXPORT void CreateSpdyHeadersFromHttpRequestForWebSocket(
    const GURL& url,
    const HttpRequestHeaders& request_headers,
    spdy::SpdyHeaderBlock* headers);

// Create HttpRequestHeaders from spdy::SpdyHeaderBlock.
NET_EXPORT void ConvertHeaderBlockToHttpRequestHeaders(
    const spdy::SpdyHeaderBlock& spdy_headers,
    HttpRequestHeaders* http_headers);

NET_EXPORT spdy::SpdyPriority ConvertRequestPriorityToSpdyPriority(
    RequestPriority priority);

NET_EXPORT RequestPriority
ConvertSpdyPriorityToRequestPriority(spdy::SpdyPriority priority);

}  // namespace net

#endif  // NET_SPDY_SPDY_HTTP_UTILS_H_
