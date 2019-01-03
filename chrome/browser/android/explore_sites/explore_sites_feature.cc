// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/explore_sites/explore_sites_feature.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "chrome/browser/android/chrome_feature_list.h"

namespace chrome {
namespace android {
namespace explore_sites {

const char kExploreSitesVariationParameterName[] = "variation";

const char kExploreSitesVariationExperimental[] = "experiment";
const char kExploreSitesVariationPersonalized[] = "personalized";

ExploreSitesVariation GetExploreSitesVariation() {
  if (base::FeatureList::IsEnabled(kExploreSites)) {
    if (base::GetFieldTrialParamValueByFeature(
            kExploreSites, kExploreSitesVariationParameterName) ==
        kExploreSitesVariationExperimental) {
      return ExploreSitesVariation::EXPERIMENT;
    }
    if (base::GetFieldTrialParamValueByFeature(
            kExploreSites, kExploreSitesVariationParameterName) ==
        kExploreSitesVariationPersonalized) {
      return ExploreSitesVariation::PERSONALIZED;
    }
    return ExploreSitesVariation::ENABLED;
  }
  return ExploreSitesVariation::DISABLED;
}

}  // namespace explore_sites
}  // namespace android
}  // namespace chrome
