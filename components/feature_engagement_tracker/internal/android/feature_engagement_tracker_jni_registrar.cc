// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement_tracker/public/android/feature_engagement_tracker_jni_registrar.h"

#include "base/android/jni_registrar.h"
#include "components/feature_engagement_tracker/internal/android/feature_engagement_tracker_impl_android.h"

namespace feature_engagement_tracker {
namespace {
base::android::RegistrationMethod
    kFeatureEngagementTrackerRegistrationMethods[] = {
        {"FeatureEngagementTrackerImplAndroid",
         FeatureEngagementTrackerImplAndroid::RegisterJni},
};
}  // namespace

// This method is declared in //components/feature_engagement_tracker/public/
//     android/feature_engagement_tracker_jni_registrar.h,
// and should be linked in to any binary that calls
// feature_engagement_tracker::RegisterFeatureEngagementTrackerJni.
bool RegisterFeatureEngagementTrackerJni(JNIEnv* env) {
  return base::android::RegisterNativeMethods(
      env, kFeatureEngagementTrackerRegistrationMethods,
      arraysize(kFeatureEngagementTrackerRegistrationMethods));
}

}  // namespace feature_engagement_tracker
