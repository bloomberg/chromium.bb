// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/common/data_reduction_proxy_switches.h"

namespace data_reduction_proxy {
namespace switches {

// The origin of the data reduction proxy.
const char kDataReductionProxy[]         = "spdy-proxy-auth-origin";

// The origin of the data reduction proxy dev.
const char kDataReductionProxyDev[]      = "spdy-proxy-dev-auth-origin";

// The origin of the data reduction proxy fallback.
const char kDataReductionProxyFallback[] = "spdy-proxy-auth-fallback";

// A test key for data reduction proxy authentication.
const char kDataReductionProxyKey[] = "spdy-proxy-auth-value";

// Sets a canary URL to test before committing to using the data reduction
// proxy. Note this canary does not go through the data reduction proxy.
const char kDataReductionProxyProbeURL[] = "data-reduction-proxy-probe-url";

// Enable the data reduction proxy.
const char kEnableDataReductionProxy[] = "enable-spdy-proxy-auth";

}  // namespace switches
}  // namespace data_reduction_proxy
