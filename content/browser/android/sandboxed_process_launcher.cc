// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/sandboxed_process_launcher.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "jni/SandboxedProcessLauncher_jni.h"

using base::android::AttachCurrentThread;
using base::android::ToJavaArrayOfStrings;
using base::android::ScopedJavaLocalRef;
using content::StartSandboxedProcessCallback;

namespace content {

// Called from SandboxedProcessLauncher.java when the SandboxedProcess was
// started.
// |client_context| is the pointer to StartSandboxedProcessCallback which was
// passed in from StartSandboxedProcess.
// |handle| is the processID of the child process as originated in Java, 0 if
// the SandboxedProcess could not be created.
static void OnSandboxedProcessStarted(JNIEnv*,
                                      jclass,
                                      jint client_context,
                                      jint handle) {
  StartSandboxedProcessCallback* callback =
      reinterpret_cast<StartSandboxedProcessCallback*>(client_context);
  if (handle)
    callback->Run(static_cast<base::ProcessHandle>(handle));
  delete callback;
}

void StartSandboxedProcess(
    const CommandLine::StringVector& argv,
    const std::vector<content::FileDescriptorInfo>& files_to_register,
    const StartSandboxedProcessCallback& callback) {
  JNIEnv* env = AttachCurrentThread();
  DCHECK(env);

  // Create the Command line String[]
  ScopedJavaLocalRef<jobjectArray> j_argv = ToJavaArrayOfStrings(env, argv);

  size_t file_count = files_to_register.size();
  DCHECK(file_count > 0);

  ScopedJavaLocalRef<jintArray> j_file_ids(env, env->NewIntArray(file_count));
  base::android::CheckException(env);
  jint* file_ids = env->GetIntArrayElements(j_file_ids.obj(), NULL);
  base::android::CheckException(env);
  ScopedJavaLocalRef<jintArray> j_file_fds(env, env->NewIntArray(file_count));
  base::android::CheckException(env);
  jint* file_fds = env->GetIntArrayElements(j_file_fds.obj(), NULL);
  base::android::CheckException(env);
  ScopedJavaLocalRef<jbooleanArray> j_file_auto_close(
      env, env->NewBooleanArray(file_count));
  base::android::CheckException(env);
  jboolean* file_auto_close =
      env->GetBooleanArrayElements(j_file_auto_close.obj(), NULL);
  base::android::CheckException(env);
  for (size_t i = 0; i < file_count; ++i) {
    const content::FileDescriptorInfo& fd_info = files_to_register[i];
    file_ids[i] = fd_info.id;
    file_fds[i] = fd_info.fd.fd;
    file_auto_close[i] = fd_info.fd.auto_close;
  }
  env->ReleaseIntArrayElements(j_file_ids.obj(), file_ids, 0);
  env->ReleaseIntArrayElements(j_file_fds.obj(), file_fds, 0);
  env->ReleaseBooleanArrayElements(j_file_auto_close.obj(), file_auto_close, 0);

  Java_SandboxedProcessLauncher_start(env,
      base::android::GetApplicationContext(),
      j_argv.obj(),
      j_file_ids.obj(),
      j_file_fds.obj(),
      j_file_auto_close.obj(),
      reinterpret_cast<jint>(new StartSandboxedProcessCallback(callback)));
}

void StopSandboxedProcess(base::ProcessHandle handle) {
  JNIEnv* env = AttachCurrentThread();
  DCHECK(env);
  Java_SandboxedProcessLauncher_stop(env, static_cast<jint>(handle));
}

bool RegisterSandboxedProcessLauncher(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace content
