// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/tab_model/tab_model_base.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/jni_weak_ref.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "jni/TabModelBase_jni.h"

using base::android::AttachCurrentThread;
using base::android::CheckException;
using base::android::ConvertUTF8ToJavaString;
using base::android::ConvertUTF16ToJavaString;
using base::android::ScopedJavaLocalRef;

TabModelBase::TabModelBase(JNIEnv* env, jobject obj, bool is_incognito)
    : TabModelJniBridge(env, obj, is_incognito) {
}


// ----------------------------------------------------------------------------
// Native JNI methods
// ----------------------------------------------------------------------------

static jlong Init(JNIEnv* env, jobject obj, jboolean is_incognito) {
  TabModel* tab_model = new TabModelBase(env, obj, is_incognito);
  return reinterpret_cast<intptr_t>(tab_model);
}

// Register native methods

bool RegisterTabModelBase(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
