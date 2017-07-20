// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_ADS_DETECTION_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_ADS_DETECTION_H_

#include <bitset>

namespace content {
class NavigationHandle;
}  // namespace content

namespace page_load_metrics {

// The types of ads that one can detect or filter on.
enum AdType {
  AD_TYPE_GOOGLE = 0,
  AD_TYPE_SUBRESOURCE_FILTER = 1,
  AD_TYPE_ALL = 2,
  AD_TYPE_MAX = AD_TYPE_ALL
};
using AdTypes = std::bitset<AD_TYPE_MAX>;

// Uses heuristics to determine if |navigation_handle| is a navigation for an
// ad.  Must be called no earlier than
// NavigationHandleImpl::ReadyToCommitNavigation.
const AdTypes& GetDetectedAdTypes(content::NavigationHandle* navigation_handle);

// Marks the |navigation_handle| as loading an advertisment of a given |type|.
void SetDetectedAdType(content::NavigationHandle* navigation_handle,
                       AdType type);

}  // namespace page_load_metrics

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_ADS_DETECTION_H_
