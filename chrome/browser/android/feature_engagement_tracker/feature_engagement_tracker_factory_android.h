// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_FEATURE_ENGAGEMENT_TRACKER_FEATURE_ENGAGEMENT_TRACKER_FACTORY_ANDROID_H_
#define CHROME_BROWSER_ANDROID_FEATURE_ENGAGEMENT_TRACKER_FEATURE_ENGAGEMENT_TRACKER_FACTORY_ANDROID_H_

#include "base/android/jni_android.h"

// Register the FeatureEngagementTrackerFactory's native methods through JNI.
bool RegisterFeatureEngagementTrackerFactoryJni(JNIEnv* env);

#endif  // CHROME_BROWSER_ANDROID_FEATURE_ENGAGEMENT_TRACKER_FEATURE_ENGAGEMENT_TRACKER_FACTORY_ANDROID_H_
