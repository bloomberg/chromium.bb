// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CHILD_PROCESS_LAUNCHER_HELPER_ANDROID_H_
#define CONTENT_BROWSER_CHILD_PROCESS_LAUNCHER_HELPER_ANDROID_H_

#include <jni.h>

namespace content {

bool RegisterChildProcessLauncher(JNIEnv* env);

}  // namespace content

#endif  // CONTENT_BROWSER_CHILD_PROCESS_LAUNCHER_HELPER_ANDROID_H_
