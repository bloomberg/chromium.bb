// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_PROXY_UTILS_H_
#define NET_HTTP_HTTP_PROXY_UTILS_H_
#pragma once

#include <string>

namespace net {

class BoundNetLog;
class HttpAuthController;
class HttpResponseInfo;
struct HttpRequestInfo;
class HttpRequestHeaders;
class HostPortPair;

// The HTTP CONNECT method for establishing a tunnel connection is documented
// in draft-luotonen-web-proxy-tunneling-01.txt and RFC 2817, Sections 5.2 and
// 5.3.
void BuildTunnelRequest(const HttpRequestInfo& request_info,
                        const HttpRequestHeaders& auth_headers,
                        const HostPortPair& endpoint,
                        std::string* request_line,
                        HttpRequestHeaders* request_headers);

// When an auth challenge (407 response) is received during tunnel construction
// this method should be called.
int HandleAuthChallenge(HttpAuthController* auth,
                        HttpResponseInfo* response,
                        const BoundNetLog& net_log);

}  // namespace net

#endif  // NET_HTTP_HTTP_PROXY_UTILS_H_
