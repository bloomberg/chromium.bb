// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/dom_distiller_service_android.h"

#include "base/android/jni_android.h"
#include "components/dom_distiller/core/distilled_page_prefs.h"
#include "components/dom_distiller/core/distilled_page_prefs_android.h"
#include "components/dom_distiller/core/dom_distiller_service.h"
#include "jni/DomDistillerService_jni.h"

namespace dom_distiller {
namespace android {

DomDistillerServiceAndroid::DomDistillerServiceAndroid(
    DomDistillerService* service)
    : service_(service) {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> local_java_ref =
      Java_DomDistillerService_create(env, reinterpret_cast<intptr_t>(this));
  java_ref_.Reset(env, local_java_ref.obj());
}

DomDistillerServiceAndroid::~DomDistillerServiceAndroid() {
}

jlong DomDistillerServiceAndroid::GetDistilledPagePrefsPtr(JNIEnv* env,
                                                           jobject obj) {
  return reinterpret_cast<intptr_t>(service_->GetDistilledPagePrefs());
}

bool DomDistillerServiceAndroid::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace android
}  // namespace dom_distiller
