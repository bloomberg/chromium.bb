// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_TRACKER_PUBLIC_FEATURE_CONSTANTS_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_TRACKER_PUBLIC_FEATURE_CONSTANTS_H_

#include "base/feature_list.h"

namespace feature_engagement_tracker {
// All the features declared in this file should also be declared in the Java
// version: org.chromium.components.feature_engagement_tracker.FeatureConstants.

// A dummy feature until real features start using the backend.
extern const base::Feature kIPHDummyFeature;

}  // namespace feature_engagement_tracker

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_TRACKER_PUBLIC_FEATURE_CONSTANTS_H_
