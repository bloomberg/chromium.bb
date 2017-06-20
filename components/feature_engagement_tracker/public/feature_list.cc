/// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement_tracker/public/feature_list.h"

#include "components/feature_engagement_tracker/public/feature_constants.h"

namespace feature_engagement_tracker {

namespace {
// Whenever a feature is added to |kAllFeatures|, it should also be added as
// DEFINE_VARIATION_PARAM below, and also added to the
// |kIPHDemoModeChoiceVariations| array.
const base::Feature* kAllFeatures[] = {
    &kIPHDummyFeature,  // Ensures non-empty array for all platforms.
#if defined(OS_ANDROID)
    &kIPHDataSaverPreviewFeature,
    &kIPHDataSaverDetailFeature,
    &kIPHDownloadPageFeature,
    &kIPHDownloadHomeFeature,
#endif  // OS_ANDROID
#if defined(OS_WIN)
    &kIPHNewTabFeature,
#endif  // OS_WIN
};
}  // namespace

const char kIPHDemoModeFeatureChoiceParam[] = "chosen_feature";

std::vector<const base::Feature*> GetAllFeatures() {
  return std::vector<const base::Feature*>(
      kAllFeatures, kAllFeatures + arraysize(kAllFeatures));
}

}  // namespace feature_engagement_tracker
