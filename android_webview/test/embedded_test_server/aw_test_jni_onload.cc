// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/test/embedded_test_server/aw_test_jni_onload.h"

#include "android_webview/test/embedded_test_server/aw_embedded_test_server.h"
#include "base/android/base_jni_onload.h"
#include "base/android/base_jni_registrar.h"
#include "base/android/jni_android.h"
#include "base/bind.h"
#include "net/test/android/net_test_jni_onload.h"
#include "net/test/embedded_test_server/android/embedded_test_server_android.h"

namespace android_webview {
namespace test {

namespace {

bool RegisterJNI(JNIEnv* env) {
  return android_webview::test::RegisterCustomHandlers(env);
}

}  // namesapce

bool OnJNIOnLoadRegisterJNI(JNIEnv* env) {
  return net::test::OnJNIOnLoadRegisterJNI(env) && RegisterJNI(env);
}

bool OnJNIOnLoadInit() {
  return base::android::OnJNIOnLoadInit();
}

}  // namespace test
}  // namespace android_webview
