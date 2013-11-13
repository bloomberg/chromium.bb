// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_NATIVE_INTERCEPTED_REQUEST_DATA_IMPL_H_
#define ANDROID_WEBVIEW_NATIVE_INTERCEPTED_REQUEST_DATA_IMPL_H_

#include "android_webview/browser/intercepted_request_data.h"
#include "base/android/scoped_java_ref.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"

namespace android_webview {

class InputStream;

class InterceptedRequestDataImpl : public InterceptedRequestData {
 public:
  // It is expected that |obj| is an instance of the Java-side
  // org.chromium.android_webview.InterceptedRequestData class.
  InterceptedRequestDataImpl(const base::android::JavaRef<jobject>& obj);
  virtual ~InterceptedRequestDataImpl();

  virtual scoped_ptr<InputStream> GetInputStream(JNIEnv* env) const OVERRIDE;
  virtual bool GetMimeType(JNIEnv* env, std::string* mime_type) const OVERRIDE;
  virtual bool GetCharset(JNIEnv* env, std::string* charset) const OVERRIDE;

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_object_;

  DISALLOW_COPY_AND_ASSIGN(InterceptedRequestDataImpl);
};

bool RegisterInterceptedRequestData(JNIEnv* env);

} // namespace android_webview

#endif  // ANDROID_WEBVIEW_NATIVE_INTERCEPTED_REQUEST_DATA_IMPL_H_
