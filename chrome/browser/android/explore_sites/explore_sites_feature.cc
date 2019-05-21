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
const char kExploreSitesVariationCondensed[] = "condensed";
const char kExploreSitesVariationMostLikelyTile[] = "mostLikelyTile";

const char kExploreSitesMostLikelyVariationParameterName[] =
    "mostLikelyVariation";

const char kExploreSitesMostLikelyVariationIconArrow[] = "arrowIcon";
const char kExploreSitesMostLikelyVariationIconDots[] = "dotsIcon";
const char kExploreSitesMostLikelyVariationIconGrouped[] = "groupedIcon";

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
    if (base::GetFieldTrialParamValueByFeature(
            kExploreSites, kExploreSitesVariationParameterName) ==
        kExploreSitesVariationCondensed) {
      return ExploreSitesVariation::CONDENSED;
    }
    if (base::GetFieldTrialParamValueByFeature(
            kExploreSites, kExploreSitesVariationParameterName) ==
        kExploreSitesVariationMostLikelyTile) {
      return ExploreSitesVariation::MOST_LIKELY;
    }
    return ExploreSitesVariation::ENABLED;
  }
  return ExploreSitesVariation::DISABLED;
}

MostLikelyVariation GetMostLikelyVariation() {
  if (base::FeatureList::IsEnabled(kExploreSites) &&
      base::GetFieldTrialParamValueByFeature(
          kExploreSites, kExploreSitesVariationParameterName) ==
          kExploreSitesVariationMostLikelyTile) {
    if (base::GetFieldTrialParamValueByFeature(
            kExploreSites, kExploreSitesMostLikelyVariationParameterName) ==
        kExploreSitesMostLikelyVariationIconArrow) {
      return MostLikelyVariation::ICON_ARROW;
    }
    if (base::GetFieldTrialParamValueByFeature(
            kExploreSites, kExploreSitesMostLikelyVariationParameterName) ==
        kExploreSitesMostLikelyVariationIconDots) {
      return MostLikelyVariation::ICON_DOTS;
    }
    if (base::GetFieldTrialParamValueByFeature(
            kExploreSites, kExploreSitesMostLikelyVariationParameterName) ==
        kExploreSitesMostLikelyVariationIconGrouped) {
      return MostLikelyVariation::ICON_GROUPED;
    }
  }
  return MostLikelyVariation::NONE;
}

}  // namespace explore_sites
}  // namespace android
}  // namespace chrome
