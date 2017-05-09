// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement_tracker/internal/android/feature_engagement_tracker_impl_android.h"

#include <vector>

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/bind.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "components/feature_engagement_tracker/public/feature_engagement_tracker.h"
#include "components/feature_engagement_tracker/public/feature_list.h"
#include "jni/FeatureEngagementTrackerImpl_jni.h"

namespace feature_engagement_tracker {

namespace {

const char kFeatureEngagementTrackerImplAndroidKey[] =
    "feature_engagement_tracker_impl_android";

// Create mapping from feature name to base::Feature.
FeatureEngagementTrackerImplAndroid::FeatureMap CreateMapFromNameToFeature(
    FeatureVector features) {
  FeatureEngagementTrackerImplAndroid::FeatureMap feature_map;
  for (auto it = features.begin(); it != features.end(); ++it) {
    feature_map[(*it)->name] = *it;
  }
  return feature_map;
}

FeatureEngagementTrackerImplAndroid* FromFeatureEngagementTrackerImpl(
    FeatureEngagementTracker* feature_engagement_tracker) {
  FeatureEngagementTrackerImpl* impl =
      static_cast<FeatureEngagementTrackerImpl*>(feature_engagement_tracker);
  FeatureEngagementTrackerImplAndroid* impl_android =
      static_cast<FeatureEngagementTrackerImplAndroid*>(
          impl->GetUserData(kFeatureEngagementTrackerImplAndroidKey));
  if (!impl_android) {
    impl_android =
        new FeatureEngagementTrackerImplAndroid(impl, GetAllFeatures());
    impl->SetUserData(kFeatureEngagementTrackerImplAndroidKey,
                      base::WrapUnique(impl_android));
  }
  return impl_android;
}

}  // namespace

// static
bool FeatureEngagementTrackerImplAndroid::RegisterJni(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

// static
FeatureEngagementTrackerImplAndroid*
FeatureEngagementTrackerImplAndroid::FromJavaObject(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& jobj) {
  return reinterpret_cast<FeatureEngagementTrackerImplAndroid*>(
      Java_FeatureEngagementTrackerImpl_getNativePtr(env, jobj));
}

// This function is declared in
// //components/feature_engagement_tracker/public/feature_engagement_tracker.h
// and should be linked in to any binary using
// FeatureEngagementTracker::GetJavaObject.
// static
base::android::ScopedJavaLocalRef<jobject>
FeatureEngagementTracker::GetJavaObject(
    FeatureEngagementTracker* feature_engagement_tracker) {
  return FromFeatureEngagementTrackerImpl(feature_engagement_tracker)
      ->GetJavaObject();
}

FeatureEngagementTrackerImplAndroid::FeatureEngagementTrackerImplAndroid(
    FeatureEngagementTrackerImpl* feature_engagement_tracker_impl,
    FeatureVector features)
    : features_(CreateMapFromNameToFeature(features)),
      feature_engagement_tracker_impl_(feature_engagement_tracker_impl) {
  JNIEnv* env = base::android::AttachCurrentThread();

  java_obj_.Reset(env, Java_FeatureEngagementTrackerImpl_create(
                           env, reinterpret_cast<intptr_t>(this))
                           .obj());
}

FeatureEngagementTrackerImplAndroid::~FeatureEngagementTrackerImplAndroid() {
  Java_FeatureEngagementTrackerImpl_clearNativePtr(
      base::android::AttachCurrentThread(), java_obj_);
}

base::android::ScopedJavaLocalRef<jobject>
FeatureEngagementTrackerImplAndroid::GetJavaObject() {
  return base::android::ScopedJavaLocalRef<jobject>(java_obj_);
}

void FeatureEngagementTrackerImplAndroid::NotifyEvent(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& jobj,
    const base::android::JavaParamRef<jstring>& jevent) {
  std::string event = ConvertJavaStringToUTF8(env, jevent);
  feature_engagement_tracker_impl_->NotifyEvent(event);
}

bool FeatureEngagementTrackerImplAndroid::ShouldTriggerHelpUI(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& jobj,
    const base::android::JavaParamRef<jstring>& jfeature) {
  std::string feature = ConvertJavaStringToUTF8(env, jfeature);
  DCHECK(features_.find(feature) != features_.end());

  return feature_engagement_tracker_impl_->ShouldTriggerHelpUI(
      *features_[feature]);
}

void FeatureEngagementTrackerImplAndroid::Dismissed(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& jobj,
    const base::android::JavaParamRef<jstring>& jfeature) {
  std::string feature = ConvertJavaStringToUTF8(env, jfeature);
  DCHECK(features_.find(feature) != features_.end());

  feature_engagement_tracker_impl_->Dismissed(*features_[feature]);
}

bool FeatureEngagementTrackerImplAndroid::IsInitialized(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& jobj) {
  return feature_engagement_tracker_impl_->IsInitialized();
}

void FeatureEngagementTrackerImplAndroid::AddOnInitializedCallback(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& jobj,
    const base::android::JavaParamRef<jobject>& j_callback_obj) {
  // Disambiguate RunCallbackAndroid to get the reference to the bool version.
  void (*runBoolCallback)(const base::android::JavaRef<jobject>&, bool) =
      &base::android::RunCallbackAndroid;
  feature_engagement_tracker_impl_->AddOnInitializedCallback(
      base::Bind(runBoolCallback,
                 base::android::ScopedJavaGlobalRef<jobject>(j_callback_obj)));
}

}  // namespace feature_engagement_tracker
