// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "jni/ExploreSitesBridge_jni.h"

namespace explore_sites {

using base::android::JavaParamRef;

// static
void JNI_ExploreSitesBridge_GetNtpCatalog(
    JNIEnv* env,
    const JavaParamRef<jclass>& j_caller,
    const JavaParamRef<jobject>& j_profile,
    const JavaParamRef<jobject>& j_result_obj,
    const JavaParamRef<jobject>& j_callback_obj) {
  // TODO(chili): fill this in
}

// static
void JNI_ExploreSitesBridge_GetEspCatalog(
    JNIEnv* env,
    const JavaParamRef<jclass>& j_caller,
    const JavaParamRef<jobject>& j_profile,
    const JavaParamRef<jobject>& j_result_obj,
    const JavaParamRef<jobject>& j_callback_obj) {
  // TODO(chili): fill this in
}

}  // namespace explore_sites
