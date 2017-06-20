// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_TRACKER_PUBLIC_FEATURE_LIST_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_TRACKER_PUBLIC_FEATURE_LIST_H_

#include <vector>

#include "base/feature_list.h"
#include "build/build_config.h"
#include "components/feature_engagement_tracker/public/feature_constants.h"
#include "components/flags_ui/feature_entry.h"

namespace feature_engagement_tracker {
using FeatureVector = std::vector<const base::Feature*>;

// The param name for the FeatureVariation configuration, which is used by
// chrome://flags to set the variable name for the selected feature. The
// FeatureEngagementTracker backend will then read this to figure out which
// feature (if any) was selected by the end user.
extern const char kIPHDemoModeFeatureChoiceParam[];

namespace {

// Defines a const flags_ui::FeatureEntry::FeatureParam for the given
// base::Feature. The constant name will be on the form
// kFooFeature --> kFooFeatureVariation. The |feature_name| argument must
// match the base::Feature::name member of the |base_feature|.
// This is intended to be used with VARIATION_ENTRY below to be able to insert
// it into an array of flags_ui::FeatureEntry::FeatureVariation.
#define DEFINE_VARIATION_PARAM(base_feature, feature_name)                     \
  constexpr flags_ui::FeatureEntry::FeatureParam base_feature##Variation[] = { \
      {kIPHDemoModeFeatureChoiceParam, feature_name}}

// Defines a single flags_ui::FeatureEntry::FeatureVariation entry, fully
// enclosed. This is intended to be used with the declaration of
// |kIPHDemoModeChoiceVariations| below.
#define VARIATION_ENTRY(base_feature)                                \
  {                                                                  \
    base_feature##Variation[0].param_value, base_feature##Variation, \
        arraysize(base_feature##Variation), nullptr                  \
  }

// Defines a flags_ui::FeatureEntry::FeatureParam for each feature.
DEFINE_VARIATION_PARAM(kIPHDummyFeature, "IPH_Dummy");
#if defined(OS_ANDROID)
DEFINE_VARIATION_PARAM(kIPHDataSaverPreviewFeature, "IPH_DataSaverPreview");
DEFINE_VARIATION_PARAM(kIPHDataSaverDetailFeature, "IPH_DataSaverDetail");
DEFINE_VARIATION_PARAM(kIPHDownloadPageFeature, "IPH_DownloadPage");
DEFINE_VARIATION_PARAM(kIPHDownloadHomeFeature, "IPH_DownloadHome");
#endif  // OS_ANDROID
#if defined(OS_WIN)
DEFINE_VARIATION_PARAM(kIPHNewTabFeature, "IPH_NewTab");
#endif  // OS_WIN

}  // namespace

// Defines the array of which features should be listed in the chrome://flags
// UI to be able to select them alone for demo-mode. The features listed here
// are possible to enable on their own in demo mode.
constexpr flags_ui::FeatureEntry::FeatureVariation
    kIPHDemoModeChoiceVariations[] = {
#if defined(OS_ANDROID)
        VARIATION_ENTRY(kIPHDataSaverPreviewFeature),
        VARIATION_ENTRY(kIPHDataSaverDetailFeature),
        VARIATION_ENTRY(kIPHDownloadPageFeature),
        VARIATION_ENTRY(kIPHDownloadHomeFeature),
#elif defined(OS_WIN)
        VARIATION_ENTRY(kIPHNewTabFeature),
#else
        VARIATION_ENTRY(kIPHDummyFeature),  // Ensures non-empty array.
#endif
};

#undef DEFINE_VARIATION_PARAM
#undef VARIATION_ENTRY

// Returns all the features that are in use for engagement tracking.
FeatureVector GetAllFeatures();

}  // namespace feature_engagement_tracker

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_TRACKER_PUBLIC_FEATURE_LIST_H_
