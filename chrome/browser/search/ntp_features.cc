// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/ntp_features.h"

#include "build/build_config.h"
#include "components/ntp_tiles/constants.h"
#include "ui/base/ui_base_features.h"

namespace features {

// If enabled, the user will see Doodles on the New Tab Page.
const base::Feature kDoodlesOnLocalNtp{"DoodlesOnLocalNtp",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, the user will see a configuration UI, and be able to select
// background images to set on the New Tab Page. Implicitly enables |kNtpIcons|.
const base::Feature kNtpBackgrounds{"NewTabPageBackgrounds",
                                    base::FEATURE_ENABLED_BY_DEFAULT};

// If enabled, the user will see the Most Visited tiles updated with Material
// Design elements. Implicitly enables |kNtpUIMd|.
const base::Feature kNtpIcons{"NewTabPageIcons",
                              base::FEATURE_ENABLED_BY_DEFAULT};

// If enabled, the user will see the New Tab Page updated with Material Design
// elements.
const base::Feature kNtpUIMd{"NewTabPageUIMd",
                             base::FEATURE_ENABLED_BY_DEFAULT};

// If enabled, the user will sometimes see promos on the NTP.
const base::Feature kPromosOnLocalNtp{"PromosOnLocalNtp",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, the user will sometimes see search suggestions on the NTP.
const base::Feature kSearchSuggestionsOnLocalNtp{
    "SearchSuggestionsOnLocalNtp", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables using the local NTP if Google is the default search engine.
const base::Feature kUseGoogleLocalNtp{"UseGoogleLocalNtp",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

bool IsCustomLinksEnabled() {
  return ntp_tiles::IsCustomLinksEnabled();
}

bool IsCustomBackgroundsEnabled() {
  return base::FeatureList::IsEnabled(kNtpBackgrounds) ||
         base::FeatureList::IsEnabled(features::kExperimentalUi);
}

bool IsMDIconsEnabled() {
  return base::FeatureList::IsEnabled(kNtpIcons) ||
         base::FeatureList::IsEnabled(kNtpBackgrounds) ||
         base::FeatureList::IsEnabled(ntp_tiles::kNtpCustomLinks) ||
         base::FeatureList::IsEnabled(features::kExperimentalUi);
}

bool IsMDUIEnabled() {
  return base::FeatureList::IsEnabled(kNtpUIMd) ||
         // MD UI changes are implicitly enabled if Material Design icons,
         // custom link, or custom backgrounds are enabled.
         base::FeatureList::IsEnabled(kNtpIcons) ||
         base::FeatureList::IsEnabled(kNtpBackgrounds) ||
         base::FeatureList::IsEnabled(ntp_tiles::kNtpCustomLinks) ||
         base::FeatureList::IsEnabled(features::kExperimentalUi);
}

}  // namespace features
