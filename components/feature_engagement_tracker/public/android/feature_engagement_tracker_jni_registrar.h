// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_TRACKER_PUBLIC_ANDROID_FEATURE_ENGAGEMENT_TRACKER_JNI_REGISTRAR_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_TRACKER_PUBLIC_ANDROID_FEATURE_ENGAGEMENT_TRACKER_JNI_REGISTRAR_H_

#include "base/android/jni_android.h"

namespace feature_engagement_tracker {

// Register the FeatureEngagementTracker's native methods through JNI.
bool RegisterFeatureEngagementTrackerJni(JNIEnv* env);

}  // namespace feature_engagement_tracker

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_TRACKER_PUBLIC_ANDROID_FEATURE_ENGAGEMENT_TRACKER_JNI_REGISTRAR_H_
