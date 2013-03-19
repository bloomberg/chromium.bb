// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_CHILD_PROCESS_LAUNCHER_ANDROID_H_
#define CONTENT_BROWSER_ANDROID_CHILD_PROCESS_LAUNCHER_ANDROID_H_

#include <jni.h>

#include "base/callback.h"
#include "base/command_line.h"
#include "base/platform_file.h"
#include "base/process.h"
#include "content/public/browser/file_descriptor_info.h"

namespace content {

typedef base::Callback<void(base::ProcessHandle)> StartChildProcessCallback;
// Starts a process as a child process spawned by the Android
// ActivityManager.
// The created process handle is returned to the |callback| on success, 0 is
// retuned if the process could not be created.
void StartChildProcess(
    const CommandLine::StringVector& argv,
    const std::vector<FileDescriptorInfo>& files_to_register,
    const StartChildProcessCallback& callback);

// Stops a child process based on the handle returned form
// StartChildProcess.
void StopChildProcess(base::ProcessHandle handle);

bool RegisterChildProcessLauncher(JNIEnv* env);

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_CHILD_PROCESS_LAUNCHER_ANDROID_H_
