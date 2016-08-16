// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/blimp/chrome_blimp_client_context_delegate_android.h"

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/android/blimp/chrome_blimp_client_context_delegate.h"
#include "chrome/browser/profiles/profile_android.h"
#include "jni/ChromeBlimpClientContextDelegate_jni.h"

// static
bool ChromeBlimpClientContextDelegateAndroid::RegisterJni(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

// static
static jlong Init(JNIEnv* env,
                  const base::android::JavaParamRef<jobject>& jobj,
                  const base::android::JavaParamRef<jobject>& jprofile) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(jprofile.obj());
  return reinterpret_cast<intptr_t>(
      new ChromeBlimpClientContextDelegateAndroid(env, jobj, profile));
}

ChromeBlimpClientContextDelegateAndroid::
    ChromeBlimpClientContextDelegateAndroid(JNIEnv* env,
                                            jobject jobj,
                                            Profile* profile)
    : ChromeBlimpClientContextDelegate(profile) {
  java_obj_.Reset(env, jobj);
}

void ChromeBlimpClientContextDelegateAndroid::Destroy(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jobj) {
  delete this;
}

ChromeBlimpClientContextDelegateAndroid::
    ~ChromeBlimpClientContextDelegateAndroid() {
  Java_ChromeBlimpClientContextDelegate_clearNativePtr(
      base::android::AttachCurrentThread(), java_obj_);
}
