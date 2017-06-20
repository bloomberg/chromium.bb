// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_TRACKER_PUBLIC_FEATURE_CONSTANTS_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_TRACKER_PUBLIC_FEATURE_CONSTANTS_H_

#include "base/feature_list.h"
#include "build/build_config.h"

namespace feature_engagement_tracker {

// A feature for enabling a demonstration mode for In-Product Help.
extern const base::Feature kIPHDemoMode;

// A feature to ensure all arrays can contain at least one feature.
extern const base::Feature kIPHDummyFeature;

// All the features declared for Android below that are also used in Java,
// should also be declared in:
// org.chromium.components.feature_engagement_tracker.FeatureConstants.
#if defined(OS_ANDROID)
extern const base::Feature kIPHDataSaverPreviewFeature;
extern const base::Feature kIPHDataSaverDetailFeature;
extern const base::Feature kIPHDownloadPageFeature;
extern const base::Feature kIPHDownloadHomeFeature;
#endif  // OS_ANDROID

#if defined(OS_WIN)
extern const base::Feature kIPHNewTabFeature;
#endif  // OS_WIN

}  // namespace feature_engagement_tracker

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_TRACKER_PUBLIC_FEATURE_CONSTANTS_H_
