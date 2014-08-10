// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_BROWSER_DATA_REDUCTION_PROXY_PROTOCOL_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_BROWSER_DATA_REDUCTION_PROXY_PROTOCOL_H_

#include "base/memory/ref_counted.h"
#include "components/data_reduction_proxy/common/data_reduction_proxy_headers.h"

namespace base {
class TimeDelta;
}

namespace net {
class HttpResponseHeaders;
class ProxyInfo;
class ProxyServer;
class URLRequest;
}

class GURL;

namespace data_reduction_proxy {

class DataReductionProxyParams;

// Decides whether to mark the data reduction proxy as temporarily bad and
// put it on the proxy retry list. Returns true if the request should be
// retried. Sets |override_response_headers| to redirect if so. Returns
// the DataReductionProxyBypassType (if not NULL).
bool MaybeBypassProxyAndPrepareToRetry(
    const DataReductionProxyParams* params,
    net::URLRequest* request,
    const net::HttpResponseHeaders* original_response_headers,
    scoped_refptr<net::HttpResponseHeaders>* override_response_headers,
    DataReductionProxyBypassType* proxy_bypass_type);

// Configure |result| to proceed directly to the origin if |result|'s current
// proxy is the data reduction proxy, the
// |net::LOAD_BYPASS_DATA_REDUCTION_PROXY| |load_flag| is set, and the
// DataCompressionProxyCriticalBypass Finch trial is set.
// This handler is intended to be invoked only by
// |ChromeNetworkDelegate.NotifyResolveProxy|.
void OnResolveProxyHandler(const GURL& url,
                           int load_flags,
                           const DataReductionProxyParams* params,
                           net::ProxyInfo* result);

// Returns true if the request method is idempotent. Only idempotent requests
// are retried on a bypass. Visible as part of the public API for testing.
bool IsRequestIdempotent(const net::URLRequest* request);

// Sets the override headers to contain a status line that indicates a
// redirect (a 302), a "Location:" header that points to the request url,
// and sets load flags to bypass proxies.
// Visible as part of the public API for testing.
void OverrideResponseAsRedirect(
    net::URLRequest* request,
    const net::HttpResponseHeaders* original_response_headers,
    scoped_refptr<net::HttpResponseHeaders>* override_response_headers);

// Adds non-empty entries in |data_reduction_proxies| to the retry map
// maintained by the proxy service of the request. Adds
// |data_reduction_proxies.second| to the retry list only if |bypass_all| is
// true. Visible as part of the public API for testing.
void MarkProxiesAsBadUntil(net::URLRequest* request,
                           base::TimeDelta& bypass_duration,
                           bool bypass_all,
                           const std::pair<GURL, GURL>& data_reduction_proxies);

}  // namespace data_reduction_proxy
#endif  // COMPONENTS_DATA_REDUCTION_PROXY_BROWSER_DATA_REDUCTION_PROXY_PROTOCOL_H_
