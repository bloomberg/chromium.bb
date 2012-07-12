// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_REMOTE_DEBUGGING_CONTROLLER_H_
#define CONTENT_BROWSER_ANDROID_REMOTE_DEBUGGING_CONTROLLER_H_

#include <jni.h>

namespace content {

bool RegisterRemoteDebuggingController(JNIEnv* env);

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_REMOTE_DEBUGGING_CONTROLLER_H_
