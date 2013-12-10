// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_CHROME_NETWORK_DATA_SAVING_METRICS_H_
#define CHROME_BROWSER_NET_CHROME_NETWORK_DATA_SAVING_METRICS_H_

#include "base/time/time.h"

namespace net {
class URLRequest;
}

class PrefService;
class Profile;

namespace chrome_browser_net {

enum DataReductionRequestType {
  VIA_DATA_REDUCTION_PROXY,  // A request served by the data reduction proxy.

  // Below are reasons why a request is not served by the enabled data
  // saving proxy.
  OFF_THE_RECORD,  // An off-the-record (incognito) request, either http
                   // or https.
  HTTPS,  // An https request that is not from an off-the-record profile.
  SHORT_BYPASS,  // The client is bypassed by the proxy for a short time.
  LONG_BYPASS,  // The client is bypassed by the proxy for a long time (due
                // to country bypass policy, for example).
  UNKNOWN_TYPE,  // Any other reason not listed above.
};

// Returns DataReductionRequestType for |request|.
DataReductionRequestType GetDataReductionRequestType(
    const Profile* profile,
    const net::URLRequest* request);

// Returns |received_content_length| as adjusted original content length if
// |original_content_length| has the invalid value (-1) or |data_reduction_type|
// is not |VIA_DATA_REDUCTION_PROXY|.
int64 GetAdjustedOriginalContentLength(
    DataReductionRequestType data_reduction_type,
    int64 original_content_length,
    int64 received_content_length);

#if defined(OS_ANDROID) || defined(OS_IOS)
// This is only exposed for testing. It is normally called by
// UpdateContentLengthPrefs.
void UpdateContentLengthPrefsForDataReductionProxy(
    int received_content_length,
    int original_content_length,
    bool with_data_reduction_proxy_enabled,
    DataReductionRequestType data_reduction_type,
    base::Time now, PrefService* prefs);
#endif

// Records daily data savings statistics to prefs and reports data savings UMA.
void UpdateContentLengthPrefs(
    int received_content_length,
    int original_content_length,
    bool with_data_reduction_proxy_enabled,
    DataReductionRequestType data_reduction_type,
    PrefService* prefs);

}  // namespace chrome_browser_net

#endif  // CHROME_BROWSER_NET_CHROME_NETWORK_DATA_SAVING_METRICS_H_
