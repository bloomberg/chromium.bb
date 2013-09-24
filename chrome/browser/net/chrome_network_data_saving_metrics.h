// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_CHROME_NETWORK_DATA_SAVING_METRICS_H_
#define CHROME_BROWSER_NET_CHROME_NETWORK_DATA_SAVING_METRICS_H_

#include "base/time/time.h"

class PrefService;

namespace chrome_browser_net {

#if defined(OS_ANDROID)
// This is only exposed for testing. It is normally called by
// UpdateContentLengthPrefs.
void UpdateContentLengthPrefsForDataReductionProxy(
    int received_content_length, int original_content_length,
    bool with_data_reduction_proxy_enabled, bool via_data_reduction_proxy,
    base::Time now, PrefService* prefs);
#endif

// Records daily data savings statistics to prefs and reports data savings UMA.
void UpdateContentLengthPrefs(
    int received_content_length,
    int original_content_length,
    bool with_data_reduction_proxy_enabled,
    bool via_data_reduction_proxy,
    PrefService* prefs);

}  // namespace chrome_browser_net

#endif  // CHROME_BROWSER_NET_CHROME_NETWORK_DATA_SAVING_METRICS_H_
