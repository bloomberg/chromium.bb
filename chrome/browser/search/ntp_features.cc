// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/ntp_features.h"

#include "build/build_config.h"
#include "components/ntp_tiles/constants.h"
#include "ui/base/ui_base_features.h"

namespace features {

// If enabled, the user will see a configuration UI, and be able to select
// background images to set on the New Tab Page.
const base::Feature kNtpBackgrounds{"NewTabPageBackgrounds",
                                    base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, the user will see the Most Visited tiles updated with Material
// Design elements.
const base::Feature kNtpIcons{"NewTabPageIcons",
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

}  // namespace features
