/// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement_tracker/public/feature_list.h"

#include "base/feature_list.h"
#include "components/feature_engagement_tracker/public/feature_constants.h"
#include "components/flags_ui/feature_entry.h"

namespace feature_engagement_tracker {

namespace {

// Defines a const flags_ui::FeatureEntry::FeatureParam for the given
// base::Feature. The constant name will be on the form
// kFooFeature --> kFooFeatureVariation. This is intended to be used with
// VARIATION_ENTRY below to be able to insert it into an array of
// flags_ui::FeatureEntry::FeatureVariation.
#define DEFINE_VARIATION_PARAM(base_feature)                                   \
  constexpr flags_ui::FeatureEntry::FeatureParam base_feature##Variation[] = { \
      {kIPHDemoModeFeatureChoiceParam, base_feature.name}}

// Defines a single flags_ui::FeatureEntry::FeatureVariation entry, fully
// enclosed. This is intended to be used with the declaration of
// |kIPHDemoModeChoiceVariations| below.
#define VARIATION_ENTRY(base_feature)                                \
  {                                                                  \
    base_feature##Variation[0].param_value, base_feature##Variation, \
        arraysize(base_feature##Variation), nullptr                  \
  }

// Whenever a feature is added to |kAllFeatures|, it should also be added as
// DEFINE_VARIATION_PARAM below, and also added to the
// |kIPHDemoModeChoiceVariations| array.
const base::Feature* kAllFeatures[] = {
    &kIPHDataSaverPreviewFeature, &kIPHDataSaverDetailFeature,
    &kIPHDownloadPageFeature, &kIPHDownloadHomeFeature};

// Defines a flags_ui::FeatureEntry::FeatureParam for each feature.
DEFINE_VARIATION_PARAM(kIPHDataSaverPreviewFeature);
DEFINE_VARIATION_PARAM(kIPHDataSaverDetailFeature);
DEFINE_VARIATION_PARAM(kIPHDownloadPageFeature);
DEFINE_VARIATION_PARAM(kIPHDownloadHomeFeature);

}  // namespace

const char kIPHDemoModeFeatureChoiceParam[] = "chosen_feature";

constexpr flags_ui::FeatureEntry::FeatureVariation
    kIPHDemoModeChoiceVariations[] = {
        VARIATION_ENTRY(kIPHDataSaverPreviewFeature),
        VARIATION_ENTRY(kIPHDataSaverDetailFeature),
        VARIATION_ENTRY(kIPHDownloadPageFeature),
        VARIATION_ENTRY(kIPHDownloadHomeFeature),
        // Note: When changing the number of entries in this array, the constant
        // kIPHDemoModeChoiceVariationsLen in feature_list.h must also be
        // updated to reflect the real size.
};

std::vector<const base::Feature*> GetAllFeatures() {
  return std::vector<const base::Feature*>(
      kAllFeatures, kAllFeatures + arraysize(kAllFeatures));
}

}  // namespace feature_engagement_tracker
