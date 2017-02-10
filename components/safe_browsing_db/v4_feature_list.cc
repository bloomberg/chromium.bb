// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "components/safe_browsing_db/v4_feature_list.h"

namespace safe_browsing {

namespace V4FeatureList {

namespace {

const base::Feature kLocalDatabaseManagerEnabled{
    "SafeBrowsingV4LocalDatabaseManagerEnabled",
    base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kV4OnlyEnabled{"SafeBrowsingV4OnlyEnabled",
                                   base::FEATURE_DISABLED_BY_DEFAULT};

bool IsV4OnlyEnabled() {
  return base::FeatureList::IsEnabled(kV4OnlyEnabled);
}

bool IsLocalDatabaseManagerEnabled() {
  return base::FeatureList::IsEnabled(kLocalDatabaseManagerEnabled) ||
         IsV4OnlyEnabled();
}

}  // namespace

V4UsageStatus GetV4UsageStatus() {
  V4UsageStatus v4_usage_status;
  if (safe_browsing::V4FeatureList::IsV4OnlyEnabled()) {
    v4_usage_status = V4UsageStatus::V4_ONLY;
  } else if (safe_browsing::V4FeatureList::IsLocalDatabaseManagerEnabled()) {
    v4_usage_status = V4UsageStatus::V4_INSTANTIATED;
  } else {
    v4_usage_status = V4UsageStatus::V4_DISABLED;
  }
  return v4_usage_status;
}

}  // namespace V4FeatureList

}  // namespace safe_browsing
