/// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/public/feature_list.h"

#include "components/feature_engagement/public/feature_constants.h"

namespace feature_engagement {

namespace {
// Whenever a feature is added to |kAllFeatures|, it should also be added as
// DEFINE_VARIATION_PARAM in the header, and also added to the
// |kIPHDemoModeChoiceVariations| array.
const base::Feature* const kAllFeatures[] = {
    &kIPHDummyFeature,  // Ensures non-empty array for all platforms.
#if defined(OS_ANDROID)
    &kIPHDataSaverDetailFeature,
    &kIPHDataSaverPreviewFeature,
    &kIPHDownloadHomeFeature,
    &kIPHDownloadPageFeature,
    &kIPHDownloadPageScreenshotFeature,
    &kIPHChromeHomeExpandFeature,
    &kIPHMediaDownloadFeature,
#endif  // defined(OS_ANDROID)
#if defined(OS_WIN) || defined(OS_LINUX)
    &kIPHIncognitoWindowFeature,
    &kIPHNewTabFeature,
#endif  // defined(OS_WIN) || defined(OS_LINUX)
#if defined(OS_IOS)
    &kIPHNewTabTipFeature,
    &kIPHNewIncognitoTabTipFeature,
    &kIPHBadgedReadingListFeature,
#endif  // defined(OS_IOS)
};
}  // namespace

const char kIPHDemoModeFeatureChoiceParam[] = "chosen_feature";

std::vector<const base::Feature*> GetAllFeatures() {
  return std::vector<const base::Feature*>(
      kAllFeatures, kAllFeatures + arraysize(kAllFeatures));
}

}  // namespace feature_engagement
