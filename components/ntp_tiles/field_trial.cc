// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_tiles/field_trial.h"

#include "build/build_config.h"

#if defined(OS_ANDROID) && !defined(NDEBUG)
#include <jni.h>
#endif

#include "base/feature_list.h"
#include "components/ntp_tiles/constants.h"

#if defined(OS_ANDROID) && !defined(NDEBUG)
#include "base/android/jni_android.h"
#include "jni/MostVisitedSites_jni.h"
#endif

namespace ntp_tiles {

bool ShouldShowPopularSites() {
// Don't check force-enabling during tests as this confuses trybots.
#if defined(OS_ANDROID) && !defined(NDEBUG)
  if (Java_MostVisitedSites_isPopularSitesForceEnabled(
          base::android::AttachCurrentThread())) {
    return true;
  }
#endif

  return base::FeatureList::IsEnabled(kUsePopularSitesSuggestions);
}

}  // namespace ntp_tiles
