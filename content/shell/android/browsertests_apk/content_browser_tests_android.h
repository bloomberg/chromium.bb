// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_ANDROID_BROWSERTESTS_APK_CONTENT_BROWSER_TESTS_ANDROID_H_
#define CONTENT_SHELL_ANDROID_BROWSERTESTS_APK_CONTENT_BROWSER_TESTS_ANDROID_H_

#include <jni.h>

namespace content {

bool RegisterContentBrowserTestsAndroid(JNIEnv* env);

}  // namespace content

#endif  // CONTENT_SHELL_ANDROID_BROWSERTESTS_APK_CONTENT_BROWSER_TESTS_ANDROID_H_
