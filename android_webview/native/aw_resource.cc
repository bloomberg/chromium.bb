// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/native/aw_resource.h"

#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "jni/AwResource_jni.h"

namespace android_webview {
namespace AwResource {

// These JNI functions are used by the Renderer but rely on Java data
// structures that exist in the Browser. By virtue of the WebView running
// in single process we can just reach over and use them. When WebView is
// multi-process capable, we'll need to rethink these. See crbug.com/156062.

std::string GetLoadErrorPageContent() {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> content =
      Java_AwResource_getLoadErrorPageContent(env);
  return base::android::ConvertJavaStringToUTF8(content);
}

std::string GetNoDomainPageContent() {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> content =
      Java_AwResource_getNoDomainPageContent(env);
  return base::android::ConvertJavaStringToUTF8(content);
}

bool RegisterAwResource(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace AwResource
}  // namespace android_webview
