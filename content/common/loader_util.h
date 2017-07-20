// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_LOADER_UTIL_H_
#define CONTENT_COMMON_LOADER_UTIL_H_

namespace net {
class URLRequest;
}

namespace content {
struct ResourceResponse;

// Helper utilities shared between network service and ResourceDispatcherHost
// code paths.

// Whether the response body should be sniffed in order to determine the MIME
// type of the response.
bool ShouldSniffContent(net::URLRequest* url_request,
                        ResourceResponse* response);

}  // namespace content

#endif  // CONTENT_COMMON_LOADER_UTIL_H_
