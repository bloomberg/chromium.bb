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
using base::GlobalDescriptors;
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
    int ipc_fd,
    const GlobalDescriptors::Mapping& files_to_register,
    const StartSandboxedProcessCallback& callback) {
  JNIEnv* env = AttachCurrentThread();
  DCHECK(env);

  // Create the Command line String[]
  ScopedJavaLocalRef<jobjectArray> j_argv = ToJavaArrayOfStrings(env, argv);

  ScopedJavaLocalRef<jintArray> j_file_to_register_id_files(env,
      env->NewIntArray(files_to_register.size() * 2));
  scoped_array<jint> file_to_register_id_files(
      new jint[files_to_register.size() * 2]);
  for (size_t i = 0; i < files_to_register.size(); ++i) {
    const GlobalDescriptors::KeyFDPair& id_file = files_to_register[i];
    file_to_register_id_files[2 * i] = id_file.first;
    file_to_register_id_files[(2 * i) + 1] = id_file.second;
  }
  env->SetIntArrayRegion(j_file_to_register_id_files.obj(),
                         0, files_to_register.size() * 2,
                         file_to_register_id_files.get());
  Java_SandboxedProcessLauncher_start(env,
          base::android::GetApplicationContext(),
          static_cast<jobjectArray>(j_argv.obj()),
          static_cast<jint>(ipc_fd),
          j_file_to_register_id_files.obj(),
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
