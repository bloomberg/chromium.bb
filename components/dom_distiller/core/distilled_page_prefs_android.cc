// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/distilled_page_prefs_android.h"

#include "components/dom_distiller/core/distilled_page_prefs.h"
#include "components/dom_distiller/core/dom_distiller_service.h"
#include "jni/DistilledPagePrefs_jni.h"

namespace dom_distiller {

namespace android {

DistilledPagePrefsAndroid::DistilledPagePrefsAndroid(
    JNIEnv* env,
    jobject obj,
    DistilledPagePrefs* distilled_page_prefs_ptr)
    : distilled_page_prefs_(distilled_page_prefs_ptr) {
}

DistilledPagePrefsAndroid::~DistilledPagePrefsAndroid() {
}

void DistilledPagePrefsAndroid::SetTheme(JNIEnv* env, jobject obj, jint theme) {
  distilled_page_prefs_->SetTheme((DistilledPagePrefs::Theme)theme);
}

jint DistilledPagePrefsAndroid::GetTheme(JNIEnv* env, jobject obj) {
  return (int)distilled_page_prefs_->GetTheme();
}

jlong Init(JNIEnv* env, jobject obj, jlong distilled_page_prefs_ptr) {
  DistilledPagePrefs* distilled_page_prefs =
      reinterpret_cast<DistilledPagePrefs*>(distilled_page_prefs_ptr);
  DistilledPagePrefsAndroid* distilled_page_prefs_android =
      new DistilledPagePrefsAndroid(env, obj, distilled_page_prefs);
  return reinterpret_cast<intptr_t>(distilled_page_prefs_android);
}

bool DistilledPagePrefsAndroid::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

void DistilledPagePrefsAndroid::AddObserver(JNIEnv* env,
                                            jobject obj,
                                            jlong observer_ptr) {
  DistilledPagePrefsObserverAndroid* distilled_page_prefs_observer_wrapper =
      reinterpret_cast<DistilledPagePrefsObserverAndroid*>(observer_ptr);
  distilled_page_prefs_->AddObserver(distilled_page_prefs_observer_wrapper);
}

void DistilledPagePrefsAndroid::RemoveObserver(JNIEnv* env,
                                               jobject obj,
                                               jlong observer_ptr) {
  DistilledPagePrefsObserverAndroid* distilled_page_prefs_observer_wrapper =
      reinterpret_cast<DistilledPagePrefsObserverAndroid*>(observer_ptr);
  distilled_page_prefs_->RemoveObserver(distilled_page_prefs_observer_wrapper);
}

DistilledPagePrefsObserverAndroid::DistilledPagePrefsObserverAndroid(
    JNIEnv* env,
    jobject obj) {
  java_ref_.Reset(env, obj);
}

DistilledPagePrefsObserverAndroid::~DistilledPagePrefsObserverAndroid() {}

void DistilledPagePrefsObserverAndroid::DestroyObserverAndroid(JNIEnv* env,
                                                               jobject obj) {
  delete this;
}

void DistilledPagePrefsObserverAndroid::OnChangeTheme(
    DistilledPagePrefs::Theme new_theme) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_DistilledPagePrefsObserverWrapper_onChangeTheme(
      env, java_ref_.obj(), (int)new_theme);
}

jlong InitObserverAndroid(JNIEnv* env, jobject obj) {
  DistilledPagePrefsObserverAndroid* observer_android =
      new DistilledPagePrefsObserverAndroid(env, obj);
  return reinterpret_cast<intptr_t>(observer_android);
}

}  // namespace android

}  // namespace dom_distiller
