// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/common/data_reduction_proxy_features.h"

namespace data_reduction_proxy {
namespace features {

// Enables the Data Reduction Proxy footer in the main menu on Android.
const base::Feature kDataReductionMainMenu{"DataReductionProxyMainMenu",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

// Enables the site breakdown on the Data Reduction Proxy settings page.
const base::Feature kDataReductionSiteBreakdown{
    "DataReductionProxySiteBreakdown", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables a new version of the data reduction proxy protocol where the server
// decides if a server-generated preview should be served. The previous
// version required the client to make this decision. The new protocol relies
// on updates primarily to the Chrome-Proxy-Accept-Transform header.
const base::Feature kDataReductionProxyDecidesTransform{
    "DataReductionProxyDecidesTransform", base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace features
}  // namespace data_reduction_proxy
