// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_NATIVE_INTERCEPTED_REQUEST_DATA_IMPL_H_
#define ANDROID_WEBVIEW_NATIVE_INTERCEPTED_REQUEST_DATA_IMPL_H_

#include "android_webview/browser/aw_web_resource_response.h"
#include "base/android/scoped_java_ref.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"

namespace net {
class HttpResponseHeaders;
}

namespace android_webview {

class InputStream;

class AwWebResourceResponseImpl : public AwWebResourceResponse {
 public:
  // It is expected that |obj| is an instance of the Java-side
  // org.chromium.android_webview.AwWebResourceResponse class.
  AwWebResourceResponseImpl(const base::android::JavaRef<jobject>& obj);
  virtual ~AwWebResourceResponseImpl();

  virtual scoped_ptr<InputStream> GetInputStream(JNIEnv* env) const override;
  virtual bool GetMimeType(JNIEnv* env, std::string* mime_type) const override;
  virtual bool GetCharset(JNIEnv* env, std::string* charset) const override;
  virtual bool GetStatusInfo(JNIEnv* env,
                             int* status_code,
                             std::string* reason_phrase) const override;
  virtual bool GetResponseHeaders(
      JNIEnv* env,
      net::HttpResponseHeaders* headers) const override;

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_object_;

  DISALLOW_COPY_AND_ASSIGN(AwWebResourceResponseImpl);
};

bool RegisterAwWebResourceResponse(JNIEnv* env);

} // namespace android_webview

#endif  // ANDROID_WEBVIEW_NATIVE_INTERCEPTED_REQUEST_DATA_IMPL_H_
