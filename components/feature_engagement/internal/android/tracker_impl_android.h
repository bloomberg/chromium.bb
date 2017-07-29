// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_ANDROID_TRACKER_IMPL_ANDROID_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_ANDROID_TRACKER_IMPL_ANDROID_H_

#include <string>
#include <unordered_map>

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "base/supports_user_data.h"
#include "components/feature_engagement/internal/tracker_impl.h"
#include "components/feature_engagement/public/feature_list.h"

namespace base {
struct Feature;
}  // namespace base

namespace feature_engagement {

// JNI bridge between TrackerImpl in Java and C++. See the
// public API of Tracker for documentation for all methods.
class TrackerImplAndroid : public base::SupportsUserData::Data {
 public:
  using FeatureMap = std::unordered_map<std::string, const base::Feature*>;
  static bool RegisterJni(JNIEnv* env);
  static TrackerImplAndroid* FromJavaObject(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& jobj);

  TrackerImplAndroid(TrackerImpl* tracker_impl, FeatureVector features);
  ~TrackerImplAndroid() override;

  base::android::ScopedJavaLocalRef<jobject> GetJavaObject();

  TrackerImpl* tracker_impl() { return tracker_impl_; }

  // Tracker JNI bridge implementation.
  virtual void NotifyEvent(JNIEnv* env,
                           const base::android::JavaRef<jobject>& jobj,
                           const base::android::JavaParamRef<jstring>& jevent);
  virtual bool ShouldTriggerHelpUI(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& jobj,
      const base::android::JavaParamRef<jstring>& jfeature);
  virtual jint GetTriggerState(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& jobj,
      const base::android::JavaParamRef<jstring>& jfeature);
  virtual void Dismissed(JNIEnv* env,
                         const base::android::JavaRef<jobject>& jobj,
                         const base::android::JavaParamRef<jstring>& jfeature);
  virtual bool IsInitialized(JNIEnv* env,
                             const base::android::JavaRef<jobject>& jobj);
  virtual void AddOnInitializedCallback(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& jobj,
      const base::android::JavaParamRef<jobject>& j_callback_obj);

 private:
  // A map from the feature name to the base::Feature, to ensure that the Java
  // version of the API can use the string name. If base::Feature becomes a Java
  // class as well, we should remove this mapping.
  FeatureMap features_;

  // The TrackerImpl this is a JNI bridge for.
  TrackerImpl* tracker_impl_;

  // The Java-side of this JNI bridge.
  base::android::ScopedJavaGlobalRef<jobject> java_obj_;

  DISALLOW_COPY_AND_ASSIGN(TrackerImplAndroid);
};

}  // namespace feature_engagement

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_ANDROID_TRACKER_IMPL_ANDROID_H_
