// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_tiles/constants.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "components/ntp_tiles/switches.h"

namespace ntp_tiles {

const char kPopularSitesFieldTrialName[] = "NTPPopularSites";

extern const base::Feature kPopularSitesBakedInContentFeature{
    "NTPPopularSitesBakedInContent", base::FEATURE_ENABLED_BY_DEFAULT};

extern const base::Feature kNtpMostLikelyFaviconsFromServerFeature{
    "NTPMostLikelyFaviconsFromServer", base::FEATURE_ENABLED_BY_DEFAULT};

extern const base::Feature kLowerResolutionFaviconsFeature{
    "NTPTilesLowerResolutionFavicons", base::FEATURE_DISABLED_BY_DEFAULT};

extern const base::Feature kSiteExplorationUiFeature{
    "SiteExplorationUi", base::FEATURE_DISABLED_BY_DEFAULT};

bool AreNtpMostLikelyFaviconsFromServerEnabled() {
  // Check if the experimental flag is forced on or off.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(
          switches::kEnableNtpMostLikelyFaviconsFromServer)) {
    return true;
  } else if (command_line->HasSwitch(
                 switches::kDisableNtpMostLikelyFaviconsFromServer)) {
    return false;
  }

  // Check if the finch experiment is turned on.
  return base::FeatureList::IsEnabled(kNtpMostLikelyFaviconsFromServerFeature);
}

}  // namespace ntp_tiles
