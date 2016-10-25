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

const base::Feature kV4HybridEnabled{"SafeBrowsingV4HybridEnabled",
                                     base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace

bool IsLocalDatabaseManagerEnabled() {
  return base::FeatureList::IsEnabled(kLocalDatabaseManagerEnabled) ||
         IsV4HybridEnabled();
}

bool IsV4HybridEnabled() {
  return base::FeatureList::IsEnabled(kV4HybridEnabled);
}

}  // namespace V4FeatureList

}  // namespace safe_browsing
