// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/common/data_reduction_proxy_pref_names.h"

namespace data_reduction_proxy {
namespace prefs {

// A List pref that contains daily totals of the size of all HTTPS
// content received when the data reduction proxy was enabled.
const char kDailyContentLengthHttpsWithDataReductionProxyEnabled[] =
    "data_reduction.daily_received_length_https_with_"
    "data_reduction_proxy_enabled";

// A List pref that contains daily totals of the size of all HTTP/HTTPS
// content received when a bypass of more than 30 minutes is in effect.
const char kDailyContentLengthLongBypassWithDataReductionProxyEnabled[] =
    "data_reduction.daily_received_length_long_bypass_with_"
    "data_reduction_proxy_enabled";

// A List pref that contains daily totals of the size of all HTTP/HTTPS
// content received when a bypass of less than 30 minutes is in effect.
const char kDailyContentLengthShortBypassWithDataReductionProxyEnabled[] =
    "data_reduction.daily_received_length_short_bypass_with_"
    "data_reduction_proxy_enabled";

// TODO(bengr): what is this?
const char kDailyContentLengthUnknownWithDataReductionProxyEnabled[] =
    "data_reduction.daily_received_length_unknown_with_"
    "data_reduction_proxy_enabled";

// A List pref that contains daily totals of the size of all HTTP/HTTPS
// content received via the data reduction proxy.
const char kDailyContentLengthViaDataReductionProxy[] =
    "data_reduction.daily_received_length_via_data_reduction_proxy";

// A List pref that contains daily totals of the size of all HTTP/HTTPS
// content received while the data reduction proxy is enabled.
const char kDailyContentLengthWithDataReductionProxyEnabled[] =
    "data_reduction.daily_received_length_with_data_reduction_proxy_enabled";

// An int64 pref that contains an internal representation of midnight on the
// date of the last update to |kDailyHttp{Original,Received}ContentLength|.
const char kDailyHttpContentLengthLastUpdateDate[] =
    "data_reduction.last_update_date";

// A List pref that contains daily totals of the original size of all HTTP/HTTPS
// content received from the network.
const char kDailyHttpOriginalContentLength[] =
    "data_reduction.daily_original_length";

// A List pref that contains daily totals of the size of all HTTP/HTTPS content
// received from the network.
const char kDailyHttpReceivedContentLength[] =
    "data_reduction.daily_received_length";

// A List pref that contains daily totals of the original size of all HTTP/HTTPS
// content received via the data reduction proxy.
const char kDailyOriginalContentLengthViaDataReductionProxy[] =
    "data_reduction.daily_original_length_via_data_reduction_proxy";

// A List pref that contains daily totals of the original size of all HTTP/HTTPS
// content received while the data reduction proxy is enabled.
const char kDailyOriginalContentLengthWithDataReductionProxyEnabled[] =
    "data_reduction.daily_original_length_with_data_reduction_proxy_enabled";

// String that specifies the origin allowed to use data reduction proxy
// authentication, if any.
const char kDataReductionProxy[] = "auth.spdyproxy.origin";

// A boolean specifying whether the data reduction proxy is enabled.
const char kDataReductionProxyEnabled[] = "spdy_proxy.enabled";

// A boolean specifying whether the data reduction proxy alternative is enabled.
const char kDataReductionProxyAltEnabled[] = "data_reduction_alt.enabled";

// A boolean specifying whether the data reduction proxy was ever enabled
// before.
const char kDataReductionProxyWasEnabledBefore[] =
    "spdy_proxy.was_enabled_before";

// An int64 pref that contains the total size of all HTTP content received from
// the network.
const char kHttpReceivedContentLength[] = "http_received_content_length";

// An int64 pref that contains the total original size of all HTTP content
// received over the network.
const char kHttpOriginalContentLength[] = "http_original_content_length";

}  // namespace prefs
}  // namespace data_reduction_proxy
