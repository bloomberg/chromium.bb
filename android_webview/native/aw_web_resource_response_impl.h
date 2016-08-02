// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_NATIVE_INTERCEPTED_REQUEST_DATA_IMPL_H_
#define ANDROID_WEBVIEW_NATIVE_INTERCEPTED_REQUEST_DATA_IMPL_H_

#include <memory>

#include "android_webview/browser/net/aw_web_resource_response.h"
#include "base/android/scoped_java_ref.h"
#include "base/compiler_specific.h"
#include "base/macros.h"

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
  ~AwWebResourceResponseImpl() override;

  std::unique_ptr<InputStream> GetInputStream(JNIEnv* env) const override;
  bool GetMimeType(JNIEnv* env, std::string* mime_type) const override;
  bool GetCharset(JNIEnv* env, std::string* charset) const override;
  bool GetStatusInfo(JNIEnv* env,
                     int* status_code,
                     std::string* reason_phrase) const override;
  bool GetResponseHeaders(JNIEnv* env,
                          net::HttpResponseHeaders* headers) const override;

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_object_;

  DISALLOW_COPY_AND_ASSIGN(AwWebResourceResponseImpl);
};

} // namespace android_webview

#endif  // ANDROID_WEBVIEW_NATIVE_INTERCEPTED_REQUEST_DATA_IMPL_H_
