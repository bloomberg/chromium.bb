// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/subresource_filter/navigation_throttle_util.h"

#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "components/safe_browsing_db/database_manager.h"
#include "components/safe_browsing_db/v4_feature_list.h"
#include "components/subresource_filter/content/browser/subresource_filter_safe_browsing_activation_throttle.h"
#include "components/subresource_filter/core/browser/subresource_filter_features.h"
#include "content/public/browser/navigation_handle.h"

content::NavigationThrottle* MaybeCreateSubresourceFilterNavigationThrottle(
    content::NavigationHandle* navigation_handle,
    safe_browsing::SafeBrowsingService* safe_browsing_service) {
  if (navigation_handle->IsInMainFrame() && safe_browsing_service &&
      safe_browsing_service->database_manager()->IsSupported() &&
      safe_browsing::V4FeatureList::GetV4UsageStatus() ==
          safe_browsing::V4FeatureList::V4UsageStatus::V4_ONLY) {
    return new subresource_filter::
        SubresourceFilterSafeBrowsingActivationThrottle(
            navigation_handle, safe_browsing_service->database_manager());
  }
  return nullptr;
}
