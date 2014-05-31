// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_COMMON_DATA_REDUCTION_PROXY_PREF_NAMES_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_COMMON_DATA_REDUCTION_PROXY_PREF_NAMES_H_

namespace data_reduction_proxy {
namespace prefs {

// Alphabetical list of preference names specific to the data_reduction_proxy
// component. Keep alphabetized, and document each in the .cc file.

extern const char kDailyContentLengthHttpsWithDataReductionProxyEnabled[];
extern const char kDailyContentLengthLongBypassWithDataReductionProxyEnabled[];
extern const char kDailyContentLengthShortBypassWithDataReductionProxyEnabled[];
extern const char kDailyContentLengthUnknownWithDataReductionProxyEnabled[];
extern const char kDailyContentLengthViaDataReductionProxy[];
extern const char kDailyContentLengthWithDataReductionProxyEnabled[];
extern const char kDailyHttpContentLengthLastUpdateDate[];
extern const char kDailyHttpOriginalContentLength[];
extern const char kDailyHttpReceivedContentLength[];
extern const char kDailyOriginalContentLengthViaDataReductionProxy[];
extern const char kDailyOriginalContentLengthWithDataReductionProxyEnabled[];
extern const char kDataReductionProxy[];
extern const char kDataReductionProxyEnabled[];
extern const char kDataReductionProxyAltEnabled[];
extern const char kDataReductionProxyWasEnabledBefore[];
extern const char kHttpOriginalContentLength[];
extern const char kHttpReceivedContentLength[];

}  // namespace prefs
}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_COMMON_DATA_REDUCTION_PROXY_PREF_NAMES_H_
