// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/data_usage/data_use_tab_ui_manager_android.h"

#include "base/android/jni_android.h"
#include "chrome/browser/profiles/profile_android.h"
#include "jni/DataUseTabUIManager_jni.h"

// static
jboolean CheckDataUseTrackingStarted(JNIEnv* env,
                                     const JavaParamRef<jclass>& clazz,
                                     jint tab_id,
                                     const JavaParamRef<jobject>& jprofile) {
  // TODO(megjablon): Get the DataUseTabUIManager which is a keyed service and
  // ask it if data use tracking has started.
  // Profile* profile = ProfileAndroid::FromProfileAndroid(jprofile);
  return false;
}

// static
jboolean CheckDataUseTrackingEnded(JNIEnv* env,
                                   const JavaParamRef<jclass>& clazz,
                                   jint tab_id,
                                   const JavaParamRef<jobject>& jprofile) {
  // TODO(megjablon): Get the DataUseTabUIManager which is a keyed service and
  // ask it if data use tracking has ended.
  // Profile* profile = ProfileAndroid::FromProfileAndroid(jprofile);
  return false;
}

// static
void OnCustomTabInitialNavigation(JNIEnv* env,
                                  const JavaParamRef<jclass>& clazz,
                                  jint tab_id,
                                  const JavaParamRef<jstring>& url,
                                  const JavaParamRef<jstring>& packageName,
                                  const JavaParamRef<jobject>& jprofile) {
  // TODO(megjablon): Get the DataUseTabUIManager which is a keyed service and
  // tell it about the custom tab package.
}

bool RegisterDataUseTabUIManager(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
