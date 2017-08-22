// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_LOADER_UTIL_H_
#define CONTENT_COMMON_LOADER_UTIL_H_

#include "base/memory/ref_counted.h"

namespace net {
class HttpRawRequestHeaders;
class URLRequest;
}

namespace content {
struct ResourceDevToolsInfo;
struct ResourceResponse;

// Helper utilities shared between network service and ResourceDispatcherHost
// code paths.

// Whether the response body should be sniffed in order to determine the MIME
// type of the response.
bool ShouldSniffContent(net::URLRequest* url_request,
                        ResourceResponse* response);

// Fill ResourceDevToolsInfo based on raw headers.
scoped_refptr<ResourceDevToolsInfo> BuildDevToolsInfo(
    const net::URLRequest& request,
    const net::HttpRawRequestHeaders& raw_request_headers);

}  // namespace content

#endif  // CONTENT_COMMON_LOADER_UTIL_H_
