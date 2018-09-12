// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "content/public/test/android/web_contents_utils.h"
#include "jni/JsUtils_jni.h"

using base::android::ConvertJavaStringToUTF16;
using base::android::JavaParamRef;

void JNI_JsUtils_EvaluateJavaScript(JNIEnv* env,
                                    const JavaParamRef<jclass>& clazz,
                                    const JavaParamRef<jobject>& jweb_contents,
                                    const JavaParamRef<jstring>& script,
                                    const JavaParamRef<jobject>& callback) {
  content::EvaluateJavaScript(env, jweb_contents, script, callback);
}
