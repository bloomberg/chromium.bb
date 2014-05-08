// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_BROWSER_DATA_REDUCTION_PROXY_METRICS_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_BROWSER_DATA_REDUCTION_PROXY_METRICS_H_

#include "base/time/time.h"

namespace net {
class URLRequest;
}

class PrefService;

namespace data_reduction_proxy {

enum DataReductionProxyRequestType {
  VIA_DATA_REDUCTION_PROXY,  // A request served by the data reduction proxy.

  // Below are reasons why a request is not served by the enabled data reduction
  // proxy. Off-the-record profile data is not counted in all cases.
  HTTPS,  // An https request.
  SHORT_BYPASS,  // The client is bypassed by the proxy for a short time.
  LONG_BYPASS,  // The client is bypassed by the proxy for a long time (due
                // to country bypass policy, for example).
  UNKNOWN_TYPE,  // Any other reason not listed above.
};

// Returns DataReductionProxyRequestType for |request|.
DataReductionProxyRequestType GetDataReductionProxyRequestType(
    const net::URLRequest* request);

// Returns |received_content_length| as adjusted original content length if
// |original_content_length| has the invalid value (-1) or |request_type|
// is not |VIA_DATA_REDUCTION_PROXY|.
int64 GetAdjustedOriginalContentLength(
    DataReductionProxyRequestType request_type,
    int64 original_content_length,
    int64 received_content_length);

// This is only exposed for testing. It is normally called by
// UpdateContentLengthPrefs.
void UpdateContentLengthPrefsForDataReductionProxy(
    int received_content_length,
    int original_content_length,
    bool with_data_reduction_proxy_enabled,
    DataReductionProxyRequestType request_type,
    base::Time now, PrefService* prefs);

// Records daily data savings statistics to prefs and reports data savings UMA.
void UpdateContentLengthPrefs(
    int received_content_length,
    int original_content_length,
    bool with_data_reduction_proxy_enabled,
    DataReductionProxyRequestType request_type,
    PrefService* prefs);

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_BROWSER_DATA_REDUCTION_PROXY_METRICS_H_
