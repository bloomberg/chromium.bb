// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_TRACKER_INTERNAL_FEATURE_LIST_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_TRACKER_INTERNAL_FEATURE_LIST_H_

#include <vector>

#include "base/feature_list.h"

namespace feature_engagement_tracker {
using FeatureVector = std::vector<const base::Feature*>;

// Returns all the features that are in use for engagement tracking.
FeatureVector GetAllFeatures();

}  // namespace feature_engagement_tracker

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_TRACKER_INTERNAL_FEATURE_LIST_H_
