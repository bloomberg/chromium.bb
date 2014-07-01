// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_COMMON_DATA_REDUCTION_PROXY_HEADERS_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_COMMON_DATA_REDUCTION_PROXY_HEADERS_H_

#include <string>

#include "base/macros.h"
#include "base/time/time.h"
#include "net/http/http_response_headers.h"
#include "net/proxy/proxy_service.h"

using net::ProxyService;

namespace data_reduction_proxy {

// Contains instructions contained in the Chrome-Proxy header.
struct DataReductionProxyInfo {
  DataReductionProxyInfo() : bypass_all(false) {}

  // True if Chrome should bypass all available data reduction proxies. False
  // if only the currently connected data reduction proxy should be bypassed.
  bool bypass_all;

  // Amount of time to bypass the data reduction proxy or proxies.
  base::TimeDelta bypass_duration;
};

// Returns true if the Chrome-Proxy header is present and contains a bypass
// delay. Sets |proxy_info->bypass_duration| to the specified delay if greater
// than 0, and to 0 otherwise to indicate that the default proxy delay
// (as specified in |ProxyList::UpdateRetryInfoOnFallback|) should be used.
// If all available data reduction proxies should by bypassed, |bypass_all| is
// set to true. |proxy_info| must be non-NULL.
bool GetDataReductionProxyInfo(
    const net::HttpResponseHeaders* headers,
    DataReductionProxyInfo* proxy_info);

// Returns true if the response contain the data reduction proxy Via header
// value. Used to check the integrity of data reduction proxy responses.
bool HasDataReductionProxyViaHeader(const net::HttpResponseHeaders* headers);

// Returns the reason why the Chrome proxy should be bypassed or not, and
// populates |proxy_info| with information on how long to bypass if
// applicable.
ProxyService::DataReductionProxyBypassType GetDataReductionProxyBypassType(
    const net::HttpResponseHeaders* headers,
    DataReductionProxyInfo* proxy_info);

// Searches for the specified Chrome-Proxy action, and if present interprets
// its value as a duration in seconds.
bool GetDataReductionProxyBypassDuration(
    const net::HttpResponseHeaders* headers,
    const std::string& action_prefix,
    base::TimeDelta* duration);

}  // namespace data_reduction_proxy
#endif  // COMPONENTS_DATA_REDUCTION_PROXY_COMMON_DATA_REDUCTION_PROXY_HEADERS_H_
