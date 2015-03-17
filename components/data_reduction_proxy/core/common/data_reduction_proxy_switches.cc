// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"

namespace data_reduction_proxy {
namespace switches {

// The origin of the data reduction proxy.
const char kDataReductionProxy[]         = "spdy-proxy-auth-origin";

// The origin of an alternative data reduction proxy.
const char kDataReductionProxyAlt[]      = "data-reduction-proxy-alternative";

// The origin of an alternative data reduction proxy fallback.
const char kDataReductionProxyAltFallback[] =
    "data-reduction-proxy-alternative-fallback";

// The origin of the data reduction proxy dev.
const char kDataReductionProxyDev[]      = "spdy-proxy-dev-auth-origin";

// The name of a Data Reduction Proxy experiment to run. These experiments are
// defined by the proxy server. Use --force-fieldtrials for Data Reduction
// Proxy field trials.
const char kDataReductionProxyExperiment[] = "data-reduction-proxy-experiment";

// The origin of the data reduction proxy fallback.
const char kDataReductionProxyFallback[] = "spdy-proxy-auth-fallback";

// A test key for data reduction proxy authentication.
const char kDataReductionProxyKey[] = "spdy-proxy-auth-value";

// Sets a secure proxy check URL to test before committing to using the Data
// Reduction Proxy. Note this check does not go through the Data Reduction
// Proxy.
const char kDataReductionProxySecureProxyCheckURL[] =
    "data-reduction-proxy-secure-proxy-check-url";

// Sets a URL to fetch to warm up the data reduction proxy on startup and
// network changes.
const char kDataReductionProxyWarmupURL[] = "data-reduction-proxy-warmup-url";

// The origin of the data reduction SSL proxy.
const char kDataReductionSSLProxy[] = "data-reduction-ssl-proxy";

// Disables the origin of the data reduction proxy dev.
const char kDisableDataReductionProxyDev[] =
    "disable-spdy-proxy-dev-auth-origin";

// Enables the origin of the data reduction proxy dev.
const char kEnableDataReductionProxyDev[] =
    "enable-spdy-proxy-dev-auth-origin";

// Enable the data reduction proxy.
const char kEnableDataReductionProxy[] = "enable-spdy-proxy-auth";

// Enable the alternative data reduction proxy.
const char kEnableDataReductionProxyAlt[] = "enable-data-reduction-proxy-alt";

// Enable Data Reduction Proxy Lo-Fi mode.
const char kEnableDataReductionProxyLoFi[] =
    "enable-data-reduction-proxy-lo-fi";

// Enable the data reduction proxy bypass warning.
const char kEnableDataReductionProxyBypassWarning[] =
    "enable-data-reduction-proxy-bypass-warning";

// Clear data savings on Chrome startup.
const char kClearDataReductionProxyDataSavings[] =
    "clear-data-reduction-proxy-data-savings";

}  // namespace switches
}  // namespace data_reduction_proxy
