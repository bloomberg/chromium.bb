// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/remote_debugging_controller.h"

#include <jni.h>

#include "content/public/browser/android/devtools_server.h"
#include "jni/remote_debugging_controller_jni.h"

namespace content {

static void StartRemoteDebugging(JNIEnv*, jclass) {
  DevToolsServer::GetInstance()->Start();
}

static void StopRemoteDebugging(JNIEnv* , jclass) {
  DevToolsServer::GetInstance()->Stop();
}

static jboolean IsRemoteDebuggingEnabled(JNIEnv*, jclass) {
  return DevToolsServer::GetInstance()->IsStarted();
}

bool RegisterRemoteDebuggingController(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace content
