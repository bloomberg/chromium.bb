// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/native/intercepted_request_data.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "jni/InterceptedRequestData_jni.h"

using std::string;
using base::android::ScopedJavaLocalRef;

InterceptedRequestData::InterceptedRequestData(
    JNIEnv* env, base::android::JavaRef<jobject> obj) {
  java_object_.Reset(env, obj.obj());
}

InterceptedRequestData::~InterceptedRequestData() {
}

ScopedJavaLocalRef<jobject>
InterceptedRequestData::GetInputStream(JNIEnv* env) const {
  return Java_InterceptedRequestData_getData(env, java_object_.obj());
}

bool InterceptedRequestData::GetMimeType(JNIEnv* env,
                                         string* mime_type) const {
  ScopedJavaLocalRef<jstring> jstring_mime_type =
      Java_InterceptedRequestData_getMimeType(env, java_object_.obj());
  if (jstring_mime_type.is_null())
    return false;
  *mime_type = ConvertJavaStringToUTF8(jstring_mime_type);
  return true;
}

bool InterceptedRequestData::GetCharset(
    JNIEnv* env, string* charset) const {
  ScopedJavaLocalRef<jstring> jstring_charset =
      Java_InterceptedRequestData_getCharset(env, java_object_.obj());
  if (jstring_charset.is_null())
    return false;
  *charset = ConvertJavaStringToUTF8(jstring_charset);
  return true;
}

bool RegisterInterceptedRequestData(JNIEnv* env) {
  if (g_InterceptedRequestData_clazz)
    return true;
  if (!base::android::HasClass(env, kInterceptedRequestDataClassPath)) {
    DLOG(ERROR) << "Unable to find class InterceptedRequestData!";
    return false;
  }
  return RegisterNativesImpl(env);
}
