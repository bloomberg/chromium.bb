// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_CONTENT_UTIL_H_
#define CONTENT_BROWSER_ANDROID_CONTENT_UTIL_H_

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "content/public/common/file_chooser_params.h"

namespace content {

base::android::ScopedJavaLocalRef<jobject> ToJavaFileChooserParams(
    JNIEnv* env,
    const FileChooserParams& file_chooser_params);

content::FileChooserParams ToNativeFileChooserParams(
    JNIEnv* env,
    jint mode,
    jstring title,
    jstring default_file,
    jobjectArray accept_types,
    jstring capture);

};  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_CONTENT_UTIL_H_
