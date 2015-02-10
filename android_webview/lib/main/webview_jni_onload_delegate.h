// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_LIB_MAIN_JNI_ONLOAD_DELEGATE_H_
#define ANDROID_WEBVIEW_LIB_MAIN_JNI_ONLOAD_DELEGATE_H_

#include "base/android/jni_onload_delegate.h"

namespace android_webview {

class WebViewJNIOnLoadDelegate : public base::android::JNIOnLoadDelegate {
 public:
  bool RegisterJNI(JNIEnv* env) override;
  bool Init() override;
};

bool OnJNIOnLoad(JavaVM* vm);

}  // android_webview

#endif  // ANDROID_WEBVIEW_LIB_MAIN_JNI_ONLOAD_DELEGATE_H_
