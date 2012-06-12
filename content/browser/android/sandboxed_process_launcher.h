// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_SANDBOXED_PROCESS_LAUNCHER_H_
#define CONTENT_BROWSER_ANDROID_SANDBOXED_PROCESS_LAUNCHER_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/process.h"

namespace content {

typedef base::Callback<void(base::ProcessHandle)> StartSandboxedProcessCallback;
// Starts a process as a sandboxed process spawned by the Android
// ActivityManager.
// The connection object returned may be used with a subsequent call to
// CancelStartSandboxedProcess().
// The created process pid is returned to the |callback| on success, 0 is
// retuned if the process could not be created.
base::android::ScopedJavaLocalRef<jobject> StartSandboxedProcess(
    const CommandLine::StringVector& argv,
    int ipc_fd,
    int crash_fd,
    const StartSandboxedProcessCallback& callback);

// Cancel the starting of a sanboxed process.
//
// |connection| is the one returned by StartSandboxedProcess.
void CancelStartSandboxedProcess(
    const base::android::JavaRef<jobject>& connection);

// Stops a sandboxed process based on the handle returned form
// StartSandboxedProcess.
void StopSandboxedProcess(base::ProcessHandle handle);

// Registers JNI methods, this must be called before any other methods in this
// file.
bool RegisterSandboxedProcessLauncher(JNIEnv* env);

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_SANDBOXED_PROCESS_LAUNCHER_H_
