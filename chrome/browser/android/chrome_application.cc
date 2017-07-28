// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/chrome_application.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "chrome/common/chrome_content_client.h"
#include "jni/ChromeApplication_jni.h"

using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

static ScopedJavaLocalRef<jstring> GetBrowserUserAgent(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz) {
  return ConvertUTF8ToJavaString(env, GetUserAgent());
}
