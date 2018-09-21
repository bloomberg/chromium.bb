// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/welcome/nux_helper.h"
#include "base/feature_list.h"
#include "build/build_config.h"

// TODO(scottchen): remove #if guard once components/nux/ is moved to
// chrome/browser/ui/webui/welcome/ and included by non-win platforms.
// Also check if it makes sense to merge nux_helper.* with nux/constants.*.
#if defined(OS_WIN) && defined(GOOGLE_CHROME_BUILD)
#include "components/nux/constants.h"
#endif  // defined(OS_WIN) && defined(GOOGLE_CHROME_BUILD)

namespace nux {
// This feature flag is used to force the feature to be turned on for non-win
// and non-branded builds, like with tests or development on other platforms.
extern const base::Feature kNuxOnboardingForceEnabled{
    "NuxOnboardingForceEnabled", base::FEATURE_DISABLED_BY_DEFAULT};

bool IsNuxOnboardingEnabled() {
  if (base::FeatureList::IsEnabled(nux::kNuxOnboardingForceEnabled)) {
    return true;
  } else {
#if defined(OS_WIN) && defined(GOOGLE_CHROME_BUILD)
    return base::FeatureList::IsEnabled(nux::kNuxOnboardingFeature);
#else
    return false;
#endif  // defined(OS_WIN) && defined(GOOGLE_CHROME_BUILD)
  }
}
}  // namespace nux
