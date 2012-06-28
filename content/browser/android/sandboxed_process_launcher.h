// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_SANDBOXED_PROCESS_LAUNCHER_H_
#define CONTENT_BROWSER_ANDROID_SANDBOXED_PROCESS_LAUNCHER_H_

#include <jni.h>

#include "base/callback.h"
#include "base/command_line.h"
#include "base/global_descriptors_posix.h"
#include "base/platform_file.h"
#include "base/process.h"

namespace content {

typedef base::Callback<void(base::ProcessHandle)> StartSandboxedProcessCallback;
// Starts a process as a sandboxed process spawned by the Android
// ActivityManager.
// The created process handle is returned to the |callback| on success, 0 is
// retuned if the process could not be created.
void StartSandboxedProcess(
    const CommandLine::StringVector& argv,
    int ipc_fd,
    const base::GlobalDescriptors::Mapping& files_to_register,
    const StartSandboxedProcessCallback& callback);

// Stops a sandboxed process based on the handle returned form
// StartSandboxedProcess.
void StopSandboxedProcess(base::ProcessHandle handle);

// Registers JNI methods, this must be called before any other methods in this
// file.
bool RegisterSandboxedProcessLauncher(JNIEnv* env);

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_SANDBOXED_PROCESS_LAUNCHER_H_

