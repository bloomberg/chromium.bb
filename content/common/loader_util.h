// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_LOADER_UTIL_H_
#define CONTENT_COMMON_LOADER_UTIL_H_

#include "base/memory/ref_counted.h"
#include "content/public/common/resource_type.h"

namespace net {
class HttpRawRequestHeaders;
class HttpResponseHeaders;
class URLRequest;
}

namespace network {
struct HttpRawRequestResponseInfo;
}

namespace content {
struct ResourceRequest;
struct ResourceResponse;

// Helper utilities shared between network service and ResourceDispatcherHost
// code paths.

// Whether the response body should be sniffed in order to determine the MIME
// type of the response.
bool ShouldSniffContent(net::URLRequest* url_request,
                        ResourceResponse* response);

// Fill HttpRawRequestResponseInfo based on raw headers.
scoped_refptr<network::HttpRawRequestResponseInfo> BuildRawRequestResponseInfo(
    const net::URLRequest& request,
    const net::HttpRawRequestHeaders& raw_request_headers,
    const net::HttpResponseHeaders* raw_response_headers);

void AttachAcceptHeader(ResourceType type, net::URLRequest* request);

int BuildLoadFlagsForRequest(const ResourceRequest& request_data);

}  // namespace content

#endif  // CONTENT_COMMON_LOADER_UTIL_H_
