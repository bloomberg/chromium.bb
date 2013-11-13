// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/native/intercepted_request_data_impl.h"

#include "android_webview/native/input_stream_impl.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "jni/InterceptedRequestData_jni.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_job.h"

using base::android::ScopedJavaLocalRef;

namespace android_webview {

InterceptedRequestDataImpl::InterceptedRequestDataImpl(
    const base::android::JavaRef<jobject>& obj)
  : java_object_(obj) {
}

InterceptedRequestDataImpl::~InterceptedRequestDataImpl() {
}

scoped_ptr<InputStream>
InterceptedRequestDataImpl::GetInputStream(JNIEnv* env) const {
  ScopedJavaLocalRef<jobject> jstream =
      Java_InterceptedRequestData_getData(env, java_object_.obj());
  if (jstream.is_null())
    return scoped_ptr<InputStream>();
  return make_scoped_ptr<InputStream>(new InputStreamImpl(jstream));
}

bool InterceptedRequestDataImpl::GetMimeType(JNIEnv* env,
                                             std::string* mime_type) const {
  ScopedJavaLocalRef<jstring> jstring_mime_type =
      Java_InterceptedRequestData_getMimeType(env, java_object_.obj());
  if (jstring_mime_type.is_null())
    return false;
  *mime_type = ConvertJavaStringToUTF8(jstring_mime_type);
  return true;
}

bool InterceptedRequestDataImpl::GetCharset(
    JNIEnv* env, std::string* charset) const {
  ScopedJavaLocalRef<jstring> jstring_charset =
      Java_InterceptedRequestData_getCharset(env, java_object_.obj());
  if (jstring_charset.is_null())
    return false;
  *charset = ConvertJavaStringToUTF8(jstring_charset);
  return true;
}

bool RegisterInterceptedRequestData(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

} // namespace android_webview
