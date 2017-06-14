// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_TEST_EMBEDDED_TEST_SERVER_AW_TEST_JNI_ONLOAD_H_
#define ANDROID_WEBVIEW_TEST_EMBEDDED_TEST_SERVER_AW_TEST_JNI_ONLOAD_H_

#include <jni.h>

namespace android_webview {
namespace test {

bool OnJNIOnLoadRegisterJNI(JNIEnv* env);
bool OnJNIOnLoadInit();

}  // namespace test
}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_TEST_EMBEDDED_TEST_SERVER_AW_TEST_JNI_ONLOAD_H_
