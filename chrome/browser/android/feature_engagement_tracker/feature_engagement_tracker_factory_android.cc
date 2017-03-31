// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/feature_engagement_tracker/feature_engagement_tracker_factory_android.h"

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/browser/feature_engagement_tracker/feature_engagement_tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "components/feature_engagement_tracker/public/feature_engagement_tracker.h"
#include "jni/FeatureEngagementTrackerFactory_jni.h"

static base::android::ScopedJavaLocalRef<jobject>
GetFeatureEngagementTrackerForProfile(
    JNIEnv* env,
    const base::android::JavaParamRef<jclass>& clazz,
    const base::android::JavaParamRef<jobject>& jprofile) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(jprofile);
  DCHECK(profile);
  return feature_engagement_tracker::FeatureEngagementTracker::GetJavaObject(
      FeatureEngagementTrackerFactory::GetInstance()->GetForBrowserContext(
          profile));
}

bool RegisterFeatureEngagementTrackerFactoryJni(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
