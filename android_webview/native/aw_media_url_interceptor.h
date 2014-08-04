// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_NATIVE_AW_MEDIA_URL_INTERCEPTOR_H_
#define ANDROID_WEBVIEW_NATIVE_AW_MEDIA_URL_INTERCEPTOR_H_

#include <string>

#include "base/android/jni_android.h"
#include "media/base/android/media_url_interceptor.h"

namespace android_webview {

// Interceptor to handle urls for media assets in the apk.
class AwMediaUrlInterceptor : public media::MediaUrlInterceptor {
 public:
  virtual bool Intercept(const std::string& url,
                         int* fd,
                         int64* offset,
                         int64* size) const OVERRIDE;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_NATIVE_AW_MEDIA_URL_INTERCEPTOR_H_
