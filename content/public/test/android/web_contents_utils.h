// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_ANDROID_WEB_CONTENTS_UTILS_H_
#define CONTENT_PUBLIC_TEST_ANDROID_WEB_CONTENTS_UTILS_H_

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"

namespace content {

void EvaluateJavaScript(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jweb_contents,
    const base::android::JavaParamRef<jstring>& script,
    const base::android::JavaParamRef<jobject>& callback);

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_ANDROID_WEB_CONTENTS_UTILS_H_
