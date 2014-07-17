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
    DistilledPagePrefs* distillerPagePrefsPtr)
    : distilled_page_prefs_(distillerPagePrefsPtr) {
}

DistilledPagePrefsAndroid::~DistilledPagePrefsAndroid() {
}

void DistilledPagePrefsAndroid::SetTheme(JNIEnv* env, jobject obj, jint theme) {
  distilled_page_prefs_->SetTheme((DistilledPagePrefs::Theme)theme);
}

jint DistilledPagePrefsAndroid::GetTheme(JNIEnv* env, jobject obj) {
  return (int)distilled_page_prefs_->GetTheme();
}

jlong Init(JNIEnv* env, jobject obj, jlong distilledPagePrefsPtr) {
  DistilledPagePrefs* distilledPagePrefs =
      reinterpret_cast<DistilledPagePrefs*>(distilledPagePrefsPtr);
  DistilledPagePrefsAndroid* distilled_page_prefs_android =
      new DistilledPagePrefsAndroid(env, obj, distilledPagePrefs);
  return reinterpret_cast<intptr_t>(distilled_page_prefs_android);
}

bool DistilledPagePrefsAndroid::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace android
}  // namespace dom_distiller
