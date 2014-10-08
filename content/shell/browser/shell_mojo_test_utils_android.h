// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_SHELL_MOJO_TEST_UTILS_ANDROID_H_
#define CONTENT_SHELL_BROWSER_SHELL_MOJO_TEST_UTILS_ANDROID_H_

#include <jni.h>

#include "base/android/jni_android.h"

namespace content {

bool RegisterShellMojoTestUtils(JNIEnv* env);

}  // namespace content

#endif  // CONTENT_SHELL_BROWSER_SHELL_MOJO_TEST_UTILS_ANDROID_H_
