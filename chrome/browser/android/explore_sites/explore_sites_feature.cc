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

ExploreSitesVariation GetExploreSitesVariation() {
  if (base::FeatureList::IsEnabled(kExploreSites)) {
    if (base::GetFieldTrialParamValueByFeature(
            kExploreSites, kExploreSitesVariationParameterName) ==
        kExploreSitesVariationExperimental) {
      return ExploreSitesVariation::EXPERIMENT;
    }
    return ExploreSitesVariation::ENABLED;
  }
  return ExploreSitesVariation::DISABLED;
}

}  // namespace explore_sites
}  // namespace android
}  // namespace chrome
