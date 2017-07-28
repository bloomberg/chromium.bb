// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/test/embedded_test_server/aw_test_jni_onload.h"

#include "base/android/base_jni_onload.h"

namespace android_webview {
namespace test {

bool OnJNIOnLoadInit() {
  return base::android::OnJNIOnLoadInit();
}

}  // namespace test
}  // namespace android_webview
