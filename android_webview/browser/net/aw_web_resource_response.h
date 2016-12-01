// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_NET_AW_WEB_RESOURCE_RESPONSE_H_
#define ANDROID_WEBVIEW_BROWSER_NET_AW_WEB_RESOURCE_RESPONSE_H_

#include <memory>
#include <string>

#include "base/android/jni_android.h"
#include "base/macros.h"

namespace net {
class HttpResponseHeaders;
class URLRequest;
}

namespace android_webview {

class InputStream;

// This class represents the Java-side data that is to be used to complete a
// particular URLRequest.
class AwWebResourceResponse {
 public:
  virtual ~AwWebResourceResponse() {}

  virtual std::unique_ptr<InputStream> GetInputStream(JNIEnv* env) const = 0;
  virtual bool GetMimeType(JNIEnv* env, std::string* mime_type) const = 0;
  virtual bool GetCharset(JNIEnv* env, std::string* charset) const = 0;
  virtual bool GetStatusInfo(JNIEnv* env,
                             int* status_code,
                             std::string* reason_phrase) const = 0;
  // If true is returned then |headers| contain the headers, if false is
  // returned |headers| were not updated.
  virtual bool GetResponseHeaders(
      JNIEnv* env,
      net::HttpResponseHeaders* headers) const = 0;

 protected:
  AwWebResourceResponse() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(AwWebResourceResponse);
};

} // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_NET_AW_WEB_RESOURCE_RESPONSE_H_
