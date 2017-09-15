// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"

namespace data_reduction_proxy {
namespace switches {

// The origin of the data reduction proxy.
const char kDataReductionProxy[]         = "spdy-proxy-auth-origin";

// The URL from which to retrieve the Data Reduction Proxy configuration.
const char kDataReductionProxyConfigURL[] = "data-reduction-proxy-config-url";

// The name of a Data Reduction Proxy experiment to run. These experiments are
// defined by the proxy server. Use --force-fieldtrials for Data Reduction
// Proxy field trials.
const char kDataReductionProxyExperiment[] = "data-reduction-proxy-experiment";

// The Chrome-Proxy "exp" directive value used by data reduction proxy to
// receive an alternative back end implementation.
const char kDataReductionProxyServerAlternative[] = "alternative";

// The origin of the data reduction proxy fallback.
const char kDataReductionProxyFallback[] = "spdy-proxy-auth-fallback";

// The semicolon-separated list of proxy server URIs to override the list of
// HTTP proxies returned by the Data Saver API. It is illegal to use
// |kDataReductionProxy| or |kDataReductionProxyFallback| switch in conjunction
// with |kDataReductionProxyHttpProxies|. If the URI omits a scheme, then the
// proxy server scheme defaults to HTTP, and if the port is omitted then the
// default port for that scheme is used. E.g. "http://foo.net:80",
// "http://foo.net", "foo.net:80", and "foo.net" are all equivalent.
const char kDataReductionProxyHttpProxies[] =
    "data-reduction-proxy-http-proxies";

// A test key for data reduction proxy authentication.
const char kDataReductionProxyKey[] = "spdy-proxy-auth-value";

// The mode for Data Reduction Proxy Lo-Fi. The various modes are always-on,
// cellular-only, slow connections only and disabled.
const char kDataReductionProxyLoFi[] = "data-reduction-proxy-lo-fi";
const char kDataReductionProxyLoFiValueAlwaysOn[] = "always-on";
const char kDataReductionProxyLoFiValueCellularOnly[] = "cellular-only";
const char kDataReductionProxyLoFiValueDisabled[] = "disabled";
const char kDataReductionProxyLoFiValueSlowConnectionsOnly[] =
    "slow-connections-only";

const char kDataReductionPingbackURL[] = "data-reduction-proxy-pingback-url";

// Sets a secure proxy check URL to test before committing to using the Data
// Reduction Proxy. Note this check does not go through the Data Reduction
// Proxy.
const char kDataReductionProxySecureProxyCheckURL[] =
    "data-reduction-proxy-secure-proxy-check-url";

// Disables server experiments that may be enabled through field trial.
const char kDataReductionProxyServerExperimentsDisabled[] =
    "data-reduction-proxy-server-experiments-disabled";

// Enable the data reduction proxy.
const char kEnableDataReductionProxy[] = "enable-spdy-proxy-auth";

// Enable the data reduction proxy bypass warning.
const char kEnableDataReductionProxyBypassWarning[] =
    "enable-data-reduction-proxy-bypass-warning";

// Enables lite page from the data reduction proxy. This means a lite page
// should be requested instead of placeholders whenever Lo-Fi mode is on. Lo-fi
// must also be enabled via a flag or field trial.
const char kEnableDataReductionProxyLitePage[] =
    "enable-data-reduction-proxy-lite-page";

// Enables sending a pageload metrics pingback after every page load.
const char kEnableDataReductionProxyForcePingback[] =
    "enable-data-reduction-proxy-force-pingback";

// Enables a 1 MB savings promo for the data reduction proxy.
const char kEnableDataReductionProxySavingsPromo[] =
    "enable-data-reduction-proxy-savings-promo";

// Disables fetching of the warmup URL.
const char kDisableDataReductionProxyWarmupURLFetch[] =
    "disable-data-reduction-proxy-warmup-url-fetch";

}  // namespace switches
}  // namespace data_reduction_proxy
